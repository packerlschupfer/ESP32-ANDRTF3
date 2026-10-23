/*
 * ANDRTF3.cpp - part of the ESP32-ANDRTF3 library
 *
 * Copyright (C) 2025-2026 packerlschupfer
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ANDRTF3.h"
#include "ANDRTF3Logging.h"
#include <ModbusErrorTracker.h>

namespace andrtf3 {

// Helper function to convert ModbusError to string
static const char* modbusErrorToString(ModbusError error) {
    switch (error) {
        case ModbusError::SUCCESS: return "Success";
        case ModbusError::ILLEGAL_FUNCTION: return "Illegal function";
        case ModbusError::ILLEGAL_DATA_ADDRESS: return "Illegal data address";
        case ModbusError::ILLEGAL_DATA_VALUE: return "Illegal data value";
        case ModbusError::SLAVE_DEVICE_FAILURE: return "Slave device failure";
        case ModbusError::TIMEOUT: return "Timeout";
        case ModbusError::CRC_ERROR: return "CRC error";
        case ModbusError::INVALID_RESPONSE: return "Invalid response";
        case ModbusError::QUEUE_FULL: return "Queue full";
        case ModbusError::NOT_INITIALIZED: return "Not initialized";
        case ModbusError::COMMUNICATION_ERROR: return "Communication error";
        case ModbusError::INVALID_PARAMETER: return "Invalid parameter";
        case ModbusError::RESOURCE_ERROR: return "Resource error";
        case ModbusError::NULL_POINTER: return "Null pointer";
        case ModbusError::NOT_SUPPORTED: return "Not supported";
        case ModbusError::MUTEX_ERROR: return "Mutex error";
        case ModbusError::INVALID_DATA_LENGTH: return "Invalid data length";
        case ModbusError::DEVICE_NOT_FOUND: return "Device not found";
        case ModbusError::RESOURCE_CREATION_FAILED: return "Resource creation failed";
        case ModbusError::INVALID_ADDRESS: return "Invalid address";
        default: return "Unknown error";
    }
}

// Map a framework error to the driver's failure classification
static ANDRTF3::ReadError classifyModbusError(ModbusError error) {
    switch (error) {
        case ModbusError::MUTEX_ERROR:
            return ANDRTF3::ReadError::BUS_BUSY;
        case ModbusError::TIMEOUT:
            return ANDRTF3::ReadError::TIMEOUT;
        case ModbusError::CRC_ERROR:
            return ANDRTF3::ReadError::CRC;
        case ModbusError::ILLEGAL_FUNCTION:
        case ModbusError::ILLEGAL_DATA_ADDRESS:
        case ModbusError::ILLEGAL_DATA_VALUE:
        case ModbusError::SLAVE_DEVICE_FAILURE:
            return ANDRTF3::ReadError::DEVICE_EXCEPTION;
        case ModbusError::INVALID_RESPONSE:
        case ModbusError::INVALID_DATA_LENGTH:
            return ANDRTF3::ReadError::BAD_RESPONSE;
        default:
            return ANDRTF3::ReadError::OTHER;
    }
}

// Constructor
ANDRTF3::ANDRTF3(uint8_t address)
    : QueuedModbusDevice(address),
      _connected(false),
      _temperaturePtr(nullptr),
      _validityPtr(nullptr),
      _consecutive0x0000Errors(0) {
    _config = getDefaultConfig();
    _config.address = address;

    _lastReading.celsius = 0;
    _lastReading.timestamp = 0;
    _lastReading.valid = false;
    _lastReading.errorKind = ReadError::NONE;

    // Set init phase for proper operation
    setInitPhase(InitPhase::READY);

    // Register device with ModbusDevice framework
    registerDevice();
}

bool ANDRTF3::readTemperature() {
    return performRead();
}

bool ANDRTF3::requestTemperature() {
    return performRead();
}

int16_t ANDRTF3::getTemperature() const noexcept {
    return _lastReading.celsius;
}

uint32_t ANDRTF3::getReadErrorCount(ReadError kind) const noexcept {
    size_t index = static_cast<size_t>(kind);
    return (index < _readCounts.size()) ? _readCounts[index] : 0;
}

uint32_t ANDRTF3::getLastGoodReadAgeMs() const noexcept {
    if (!_hasFreshRead) {
        return UINT32_MAX;
    }
    return millis() - _lastFreshMs;
}

bool ANDRTF3::getAsyncResult(TemperatureData& data) {
    data = _lastReading;
    return _lastReading.valid;
}

void ANDRTF3::process() {
    // Process any queued async responses
    if (isAsyncEnabled()) {
        processQueue();
    }
}

bool ANDRTF3::performRead() {
    if (isStatusRegisterActive()) {
        return performStatusRead();
    }

    // Use the base class to read the temperature register with SENSOR priority
    auto result = readInputRegistersWithPriority(TEMP_REGISTER, REGISTER_COUNT, esp32Modbus::SENSOR);

    if (!result.isOk()) {
        return recordModbusFailure(result.error());
    }

    const auto& values = result.value();
    if (values.empty()) {
        return recordFailure(ReadError::BAD_RESPONSE, "No data returned", false);
    }
    if (values.size() != REGISTER_COUNT) {
        // The base layer matches replies by function code only: a late reply to
        // another FC04 read of a different length must not pass as reg 50
        return recordFailure(ReadError::BAD_RESPONSE, "Wrong response length", false);
    }

    return processRegisterValue(values[0]);
}

// Status-register mode: 50..51 in one transaction, the sensor's status bit decides
bool ANDRTF3::performStatusRead() {
    auto result = readInputRegistersWithPriority(TEMP_REGISTER, STATUS_REGISTER_COUNT, esp32Modbus::SENSOR);

    if (!result.isOk()) {
        if (result.error() != ModbusError::ILLEGAL_DATA_ADDRESS) {
            return recordModbusFailure(result.error());    // transient, not a firmware question
        }
        // Exception 02: the sensor has no reg 51. Switch over for good and take
        // this reading with the count-1 request, so enabling the mode never costs a read
        fallBackToSingleRegister(StatusFallback::EXCEPTION);
        return performRead();
    }

    const auto& values = result.value();
    if (values.size() != STATUS_REGISTER_COUNT) {
        return recordFailure(ReadError::BAD_RESPONSE, "Wrong response length", false);
    }

    if (!isPlausibleStatus(values[1])) {
        // Not the open firmware (the original answers reg 51 with 0): judge
        // this value the default way and read count 1 from now on
        ANDRTF3_LOG_W("Register 51 = 0x%04X is no open-firmware status", values[1]);
        fallBackToSingleRegister(StatusFallback::IMPLAUSIBLE);
        return processRegisterValue(values[0]);
    }

    return processStatusValue(values[0], values[1]);
}

bool ANDRTF3::processStatusValue(uint16_t value, uint16_t status) {
    int16_t rawValue = static_cast<int16_t>(value);

    ANDRTF3_LOG_D("Register 50/51: raw=0x%04X (%d) status=0x%04X", value, rawValue, status);
    _lastStatus = status;

    if (!(status & STATUS_VALID)) {
        if (status & STATUS_NEVER) return recordSensorFault("Sensor has no reading yet");
        if (status & STATUS_TIMEOUT) return recordSensorFault("Sensor ADC timeout");
        if (status & STATUS_OVERFLOW) return recordSensorFault("Sensor ADC overflow");
        return recordSensorFault("Sensor PT1000 out of range");
    }

    if (rawValue < TEMP_MIN || rawValue > TEMP_MAX) {
        return recordFailure(ReadError::OUT_OF_RANGE, "Temperature out of range", false);
    }

    // Bridged by the sensor's filter: trustworthy now, but not a new
    // measurement - getLastGoodReadAgeMs() keeps counting from the last fresh one
    recordSuccess(rawValue, !(status & STATUS_HELD));
    return true;
}

void ANDRTF3::setConfig(const Config& config) {
    if (config.useStatusRegister && !_config.useStatusRegister) {
        _statusFallback = StatusFallback::NONE;
    }
    _config = config;
}

void ANDRTF3::fallBackToSingleRegister(StatusFallback reason) {
    if (_statusFallback == StatusFallback::NONE) {
        ANDRTF3_LOG_W("Status register off (%s) - reading register 50 only",
                      statusFallbackToString(reason));
    }
    _statusFallback = reason;
    _lastStatus = 0;
}

bool ANDRTF3::detectFirmware() {
    auto result = readInputRegistersWithPriority(VERSION_REGISTER, 1, esp32Modbus::SENSOR);
    if (!result.isOk() || result.value().size() != 1) {
        ANDRTF3_LOG_W("Firmware detection failed");
        return false;
    }

    _firmwareVersion = result.value()[0];
    if (_firmwareVersion & OPEN_FIRMWARE_FLAG) {
        _firmware = Firmware::OPEN;
        _statusFallback = StatusFallback::NONE;     // re-arm after an earlier fallback
    } else {
        _firmware = Firmware::ORIGINAL;
        if (_config.useStatusRegister) {
            fallBackToSingleRegister(StatusFallback::ORIGINAL_FIRMWARE);
        }
    }
    ANDRTF3_LOG_I("Firmware: %s (reg 2 = 0x%04X)",
                  (_firmware == Firmware::OPEN) ? "open" : "original", _firmwareVersion);
    return true;
}

bool ANDRTF3::readDiagnostics(Diagnostics& out) {
    if (_firmware == Firmware::ORIGINAL) {
        return false;                       // registers 51..77 do not exist there
    }
    auto result = readInputRegistersWithPriority(DIAG_FIRST, DIAG_COUNT, esp32Modbus::SENSOR);
    if (!result.isOk()) {
        ANDRTF3_LOG_D("Diagnostics read failed: %s", modbusErrorToString(result.error()));
        return false;
    }

    const auto& r = result.value();
    if (r.size() != DIAG_COUNT) {
        return false;                       // not the reply to this request
    }
    return decodeDiagnostics(r.data(), r.size(), out);
}

bool ANDRTF3::readIdentity(Identity& out) {
    // Only on a detected open firmware, so it never sends unknown requests
    if (_firmware != Firmware::OPEN) {
        return false;
    }
    auto uid = readInputRegistersWithPriority(UID_FIRST, UID_COUNT, esp32Modbus::SENSOR);
    if (!uid.isOk() || uid.value().size() != UID_COUNT) {
        return false;
    }
    auto ident = readHoldingRegistersWithPriority(IDENT_FIRST, IDENT_COUNT, esp32Modbus::SENSOR);
    if (!ident.isOk() || ident.value().size() != IDENT_COUNT) {
        return false;
    }
    return decodeIdentity(uid.value().data(), ident.value().data(), out);
}

// Single validation path for every read (sync and async)
bool ANDRTF3::processRegisterValue(uint16_t value) {
    int16_t rawValue = static_cast<int16_t>(value);

    ANDRTF3_LOG_D("Register 50: raw=0x%04X (%d)", value, rawValue);

    // 0x0000 = sensor error or communication fault
    // 0xFFFF = common Modbus error/no response (-1 as signed)
    if (value == 0x0000 || value == 0xFFFF) {
        return recordSensorFault((value == 0xFFFF) ? "Modbus error 0xFFFF" : "Sensor returned 0x0000");
    }

    if (rawValue < TEMP_MIN || rawValue > TEMP_MAX) {
        return recordFailure(ReadError::OUT_OF_RANGE, "Temperature out of range", false);
    }

    recordSuccess(rawValue);
    return true;
}

// The sensor answered but reports no usable value (0x0000 / 0xFFFF, or status bit 0 clear)
bool ANDRTF3::recordSensorFault(const char* message) {
    if (_consecutive0x0000Errors < UINT8_MAX) {
        _consecutive0x0000Errors++;
    }

    // First occurrence is silent - the next poll confirms or clears it
    if (_consecutive0x0000Errors >= 2) {
        ANDRTF3_LOG_E("%s (%d consecutive) - sensor fault confirmed", message, _consecutive0x0000Errors);
    } else {
        ANDRTF3_LOG_D("%s - will verify on next poll", message);
    }

    // The sensor answered, so stay connected until 3+ consecutive faults
    // (never promotes a not-yet-connected sensor to connected)
    return recordFailure(ReadError::SENSOR_FAULT, message,
                         _connected && _consecutive0x0000Errors < 3);
}

bool ANDRTF3::recordModbusFailure(ModbusError error) {
    modbus::ModbusErrorTracker::recordError(getServerAddress(),
        modbus::ModbusErrorTracker::categorizeError(error));
    return recordFailure(classifyModbusError(error), modbusErrorToString(error), false);
}

// Store a failed read. celsius and timestamp keep the last good value.
bool ANDRTF3::recordFailure(ReadError kind, const char* message, bool connected) {
    if (kind == ReadError::BAD_RESPONSE || kind == ReadError::SENSOR_FAULT ||
        kind == ReadError::OUT_OF_RANGE) {
        modbus::ModbusErrorTracker::recordError(getServerAddress(),
            modbus::ModbusErrorTracker::ErrorCategory::INVALID_DATA);
    }

    _readCounts[static_cast<size_t>(kind)]++;

    _lastReading.valid = false;
    _lastReading.error = message;
    _lastReading.errorKind = kind;
    _connected = connected;

    if (_validityPtr != nullptr) {
        *_validityPtr = false;
    }
    return false;
}

// fresh = false: a value the sensor's filter bridged - valid and timestamped, but
// getLastGoodReadAgeMs() keeps the age of the last real measurement
void ANDRTF3::recordSuccess(int16_t value, bool fresh) {
    modbus::ModbusErrorTracker::recordSuccess(getServerAddress());
    _readCounts[static_cast<size_t>(ReadError::NONE)]++;

    _lastReading.celsius = value;  // Already in deci-degrees
    _lastReading.timestamp = millis();
    _lastReading.valid = true;
    if (fresh) {
        _lastFreshMs = _lastReading.timestamp;
        _hasFreshRead = true;
    }
    _lastReading.error = "";
    _lastReading.errorKind = ReadError::NONE;
    _connected = true;
    _consecutive0x0000Errors = 0;

    // Value is already in tenths of degrees - direct assignment for Temperature_t
    if (_temperaturePtr != nullptr) {
        *_temperaturePtr = value;
    }
    if (_validityPtr != nullptr) {
        *_validityPtr = true;
    }
}

// Handle async Modbus responses (only when the application enabled async mode)
void ANDRTF3::onAsyncResponse(uint8_t functionCode, uint16_t address,
                             const uint8_t* data, size_t length) {
    ANDRTF3_LOG_D("onAsyncResponse: FC=0x%02X, addr=%d, len=%d",
                  functionCode, address, length);

    if (functionCode != FUNCTION_CODE || address != TEMP_REGISTER) {
        return;
    }

    if (length < 2) {
        recordFailure(ReadError::BAD_RESPONSE, "Invalid response length", false);
        return;
    }

    uint16_t value = (static_cast<uint16_t>(data[0]) << 8) | data[1];
    if (isStatusRegisterActive()) {
        // Status mode: only a 50..51 reply carries the validity; a count-1 reply
        // here would be judged without it and could mark a fault as valid
        if (length != 2 * STATUS_REGISTER_COUNT) {
            return;
        }
        uint16_t status = (static_cast<uint16_t>(data[2]) << 8) | data[3];
        if (!isPlausibleStatus(status)) {
            fallBackToSingleRegister(StatusFallback::IMPLAUSIBLE);
            processRegisterValue(value);
            return;
        }
        processStatusValue(value, status);
        return;
    }
    if (length != 2 * REGISTER_COUNT) {
        return;                             // not a reply to the count-1 read
    }
    processRegisterValue(value);
}

// Static methods
ANDRTF3::Config ANDRTF3::getDefaultConfig() {
    return {
        3,        // address
        200,      // timeout (increased from 100ms to account for library overhead)
        3         // retries
    };
}

const char* ANDRTF3::readErrorToString(ReadError error) noexcept {
    switch (error) {
        case ReadError::NONE: return "None";
        case ReadError::BUS_BUSY: return "Bus busy";
        case ReadError::TIMEOUT: return "Timeout";
        case ReadError::CRC: return "CRC error";
        case ReadError::DEVICE_EXCEPTION: return "Device exception";
        case ReadError::BAD_RESPONSE: return "Bad response";
        case ReadError::SENSOR_FAULT: return "Sensor fault";
        case ReadError::OUT_OF_RANGE: return "Out of range";
        case ReadError::OTHER: return "Other";
        case ReadError::COUNT: break;
    }
    return "Unknown";
}

const char* ANDRTF3::statusFallbackToString(StatusFallback reason) noexcept {
    switch (reason) {
        case StatusFallback::NONE: return "None";
        case StatusFallback::EXCEPTION: return "Exception on 2-register read";
        case StatusFallback::IMPLAUSIBLE: return "Implausible status register";
        case StatusFallback::ORIGINAL_FIRMWARE: return "Original firmware";
    }
    return "Unknown";
}

// ========== Unified Mapping API ==========

void ANDRTF3::bindTemperaturePointers(int16_t* tempPtr, bool* validPtr) {
    ANDRTF3_LOG_D("Binding temperature pointers (unified mapping API)");

    _temperaturePtr = tempPtr;
    _validityPtr = validPtr;

    if (tempPtr != nullptr && validPtr != nullptr) {
        ANDRTF3_LOG_D("Temperature bound to temp=0x%p (int16_t tenths), valid=0x%p", tempPtr, validPtr);
    } else if (tempPtr == nullptr && validPtr == nullptr) {
        ANDRTF3_LOG_D("Temperature pointers unbound (both nullptr)");
    } else {
        ANDRTF3_LOG_W("Partial binding - temp=0x%p, valid=0x%p", tempPtr, validPtr);
    }
}

} // namespace andrtf3