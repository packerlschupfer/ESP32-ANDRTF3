/*
 * ANDRTF3.h - part of the ESP32-ANDRTF3 library
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

#ifndef ANDRTF3_H
#define ANDRTF3_H

#include <Arduino.h>
#include <QueuedModbusDevice.h>
#include <array>
#include "ANDRTF3Registers.h"

// Import specific types from modbus namespace
using modbus::QueuedModbusDevice;
using modbus::ModbusError;
using modbus::ModbusResult;

namespace andrtf3 {

/**
 * ANDRTF3/MD Temperature Sensor Driver
 *
 * Simple driver for wall-mount RS485 temperature sensor from Andivi
 * - Temperature range: -40°C to +125°C
 * - Resolution: 0.1°C
 * - Modbus RTU communication
 * - Factory default: address 3, 9600 baud, 8E1 (even parity)
 *
 * Register Map:
 * - 0x0032 (50): Temperature in deci-degrees Celsius
 * - 51..77: status and diagnostics, open firmware only
 *   (https://github.com/packerlschupfer/ANDRTF3-firmware)
 *
 * Validity (default): a raw value of 0x0000 or 0xFFFF is treated as a sensor
 * fault, so a genuine reading of exactly 0.0°C is reported as invalid
 * (SENSOR_FAULT). With Config::useStatusRegister on an open-firmware sensor,
 * the sensor's own status bit decides instead (see Config).
 *
 * Threading: all read and getter methods are meant to be called from a single
 * task. Bus access itself is serialized by the ModbusDevice bus mutex.
 */
class ANDRTF3 : public QueuedModbusDevice {
public:
    // Configuration structure
    struct Config {
        uint8_t address;           // Informational: the bus address is the constructor's
        uint16_t timeout;          // Reserved - not applied; the ModbusDevice layer owns timeouts
        uint8_t retries;           // Reserved - not applied; one bus transaction per read

        /**
         * Read registers 50..51 (temperature + status) in one transaction and let
         * the sensor's status bit 0 decide validity. Needs the open firmware;
         * default false = only FC04 reg 50, count 1, as without this option.
         *
         * - status bit 0 set: valid, including a genuine 0.0°C / -0.1°C
         * - status bit 0 clear: SENSOR_FAULT (same connected grace as 0x0000)
         * - bit 5 (value bridged by the sensor's filter, at most 2 readings):
         *   valid and timestamp advances, getLastGoodReadAgeMs() does not
         *
         * Never makes reads fail: exception 02 on the 2-register read or a status
         * word the open firmware cannot produce (e.g. the original firmware, which
         * answers reg 51 with 0) switches back to the count-1 read for the life of
         * the object - see isStatusRegisterActive(), getStatusFallbackReason().
         * detectFirmware() re-arms it when it finds the open firmware.
         */
        bool useStatusRegister = false;
    };

    // Which firmware detectFirmware() found (input register 2)
    enum class Firmware : uint8_t {
        UNKNOWN = 0,       // not detected yet, or the read failed
        ORIGINAL,          // Andivi firmware (reg 2 = 7 on the units seen so far)
        OPEN               // open firmware (reg 2 bit 15 set, low 12 bits = minor version)
    };

    // Why the status-register mode is not (or no longer) in use
    enum class StatusFallback : uint8_t {
        NONE = 0,          // not configured, or active
        EXCEPTION,         // the 2-register read was answered with a Modbus exception
        IMPLAUSIBLE,       // reg 51 held a value the open firmware never reports
        ORIGINAL_FIRMWARE  // detectFirmware() found the original firmware
    };

    // Register 51 bits and the 50..77 block (see ANDRTF3Registers.h)
    static constexpr uint16_t STATUS_VALID = andrtf3::STATUS_VALID;
    static constexpr uint16_t STATUS_TIMEOUT = andrtf3::STATUS_TIMEOUT;
    static constexpr uint16_t STATUS_OVERFLOW = andrtf3::STATUS_OVERFLOW;
    static constexpr uint16_t STATUS_RANGE = andrtf3::STATUS_RANGE;
    static constexpr uint16_t STATUS_NEVER = andrtf3::STATUS_NEVER;
    static constexpr uint16_t STATUS_HELD = andrtf3::STATUS_HELD;
    using Diagnostics = andrtf3::Diagnostics;
    using Identity = andrtf3::Identity;

    // Why the last read failed
    enum class ReadError : uint8_t {
        NONE = 0,          // Last read succeeded
        BUS_BUSY,          // Bus mutex not acquired - nothing was sent (contention)
        TIMEOUT,           // Request sent, sensor did not answer
        CRC,               // Answer arrived corrupted (noise, wrong parity)
        DEVICE_EXCEPTION,  // Sensor answered with a Modbus exception code
        BAD_RESPONSE,      // Answer malformed, wrong length or empty
        SENSOR_FAULT,      // Sensor answered 0x0000 / 0xFFFF
        OUT_OF_RANGE,      // Value outside -40.0..125.0°C
        OTHER,             // Driver/framework error (not initialized, queue full, ...)
        COUNT              // Number of kinds (not an error)
    };

    // Temperature data (fixed-point format: value * 10)
    struct TemperatureData {
        int16_t celsius;           // Temperature * 10 (261 = 26.1°C); last good value, kept on failure
        uint32_t timestamp;        // millis() of the read that produced the current valid value (0 = never);
                                   // advances on a value bridged by the sensor (see getLastGoodReadAgeMs)
        bool valid;                // Result of the most recent read
        String error;              // Human-readable reason, empty when valid
        ReadError errorKind;       // Machine-readable reason, NONE when valid
    };

    // Constructor/Destructor
    explicit ANDRTF3(uint8_t address = 3);
    virtual ~ANDRTF3() = default;

    // Configuration
    // Switching useStatusRegister on clears an earlier fallback (it re-arms the mode)
    void setConfig(const Config& config);
    [[nodiscard]] Config getConfig() const noexcept { return _config; }

    // Device identification
    [[nodiscard]] uint8_t getDeviceAddress() const { return getServerAddress(); }

    // Temperature reading (blocking, one bus transaction)
    [[nodiscard]] bool readTemperature();
    [[nodiscard]] int16_t getTemperature() const noexcept;
    [[nodiscard]] TemperatureData getTemperatureData() const noexcept { return _lastReading; }

    // Failure classification of the most recent read (NONE after a success)
    [[nodiscard]] ReadError getLastReadError() const noexcept { return _lastReading.errorKind; }
    [[nodiscard]] static const char* readErrorToString(ReadError error) noexcept;

    /**
     * @brief Cumulative number of reads that ended with the given kind
     *
     * NONE counts successful reads. Counters start at 0 and wrap at 2^32.
     * Each counter is a single 32-bit word, safe to read from another task.
     */
    [[nodiscard]] uint32_t getReadErrorCount(ReadError kind) const noexcept;
    void resetReadErrorCounts() noexcept { _readCounts.fill(0); }

    /**
     * @brief Milliseconds since the last successful read of a FRESH measurement
     *
     * Same as millis() - timestamp, except in status-register mode while the
     * sensor bridges a glitch (STATUS_HELD): the value stays valid and
     * timestamp advances, but this age keeps growing - it is the age of the
     * underlying measurement. The open firmware bridges at most 2 readings.
     * @return Age in ms, or UINT32_MAX if no fresh measurement was read yet
     */
    [[nodiscard]] uint32_t getLastGoodReadAgeMs() const noexcept;

    /**
     * Request-style API, kept for compatibility.
     *
     * requestTemperature() performs the complete read synchronously (same as
     * readTemperature()), so isReadComplete() is always true afterwards and
     * getAsyncResult() returns the stored result. RS485 is half-duplex: the bus
     * is occupied for request + response however the call is structured.
     */
    [[nodiscard]] bool requestTemperature();
    [[nodiscard]] bool isReadComplete() const noexcept { return true; }
    [[nodiscard]] bool getAsyncResult(TemperatureData& data);

    // Status: false after a failed read, except a SENSOR_FAULT stays connected
    // until the 3rd consecutive one
    [[nodiscard]] bool isConnected() const noexcept { return _connected; }

    /**
     * @brief Read input register 2 once and record which firmware the sensor runs
     *
     * Call it before other FC04 reads of this sensor are in flight (e.g. at init,
     * before the poller starts): the Modbus layer matches replies by function
     * code, so a late reg-2 reply (7) could otherwise be taken as reg 50 (0.7°C).
     *
     * One bus transaction. Not called automatically (no init reads). Finding the
     * open firmware re-arms Config::useStatusRegister after a fallback; finding
     * the original firmware switches it off (StatusFallback::ORIGINAL_FIRMWARE).
     * Does not touch the temperature result or the read counters.
     * @return false if the read failed (getFirmware() then keeps its old value)
     */
    [[nodiscard]] bool detectFirmware();
    [[nodiscard]] Firmware getFirmware() const noexcept { return _firmware; }
    [[nodiscard]] uint16_t getFirmwareVersion() const noexcept { return _firmwareVersion; }  // raw reg 2

    // Status-register mode (Config::useStatusRegister)
    [[nodiscard]] bool isStatusRegisterActive() const noexcept {
        return _config.useStatusRegister && _statusFallback == StatusFallback::NONE;
    }
    [[nodiscard]] StatusFallback getStatusFallbackReason() const noexcept { return _statusFallback; }
    [[nodiscard]] static const char* statusFallbackToString(StatusFallback reason) noexcept;
    // Reg 51 from the most recent successful 2-register read (0 if none yet)
    [[nodiscard]] uint16_t getLastStatus() const noexcept { return _lastStatus; }

    // true if the open firmware can report this reg 51 value (used for the fallback decision)
    [[nodiscard]] static constexpr bool isPlausibleStatus(uint16_t status) noexcept {
        return andrtf3::isPlausibleStatus(status);
    }

    /**
     * @brief Read registers 50..77 of the open firmware (one transaction)
     *
     * Separate from the temperature path: does not change the temperature
     * result, the counters, isConnected() or the bound pointers. Returns false
     * without a bus transaction if detectFirmware()
     * found the original firmware, which has none of these registers.
     * @return true if `out` was filled
     */
    [[nodiscard]] bool readDiagnostics(Diagnostics& out);

    /**
     * @brief Read the sensor's identity (open firmware, two transactions)
     *
     * Factory unique ID (FC04 78..83, different on every chip) plus the user's
     * asset number and label (FC03 532..539, set with the firmware repo's
     * tools/mbident.py). Lets a master check which physical unit answers at an
     * address, e.g. after a swap. Like readDiagnostics(), no effect on the
     * temperature path. Needs detectFirmware() first: false without a
     * transaction unless it found the open firmware.
     * @return true if `out` was filled
     */
    [[nodiscard]] bool readIdentity(Identity& out);

    // Drains the async response queue if the application called enableAsync()
    void process();

    /**
     * @brief Bind temperature data pointers (unified mapping API)
     *
     * After every read the library writes into the bound variables:
     * - success: *tempPtr = raw value (tenths of °C, no offset), *validPtr = true
     * - failure: *validPtr = false, *tempPtr keeps its previous value
     *
     * There is no debouncing and no locking: validity follows each single read,
     * and the two writes are not atomic as a pair. The variables must outlive
     * the sensor object (or be unbound with nullptr). Applications with their
     * own validity policy (offsets, hysteresis, hold-last-value) should use the
     * getters - getTemperatureData(), getLastReadError(), getLastGoodReadAgeMs() -
     * instead.
     *
     * @param tempPtr Pointer to int16_t for temperature value (in tenths of degrees Celsius)
     * @param validPtr Pointer to bool for validity flag
     *
     * Example usage:
     * @code
     * int16_t insideTemp = 0;  // tenths of degrees (261 = 26.1°C)
     * bool isTempValid = false;
     * andrtf3->bindTemperaturePointers(&insideTemp, &isTempValid);
     * @endcode
     */
    void bindTemperaturePointers(int16_t* tempPtr, bool* validPtr);

    // Static utility method
    static Config getDefaultConfig();

protected:
    // QueuedModbusDevice interface (only used if the application enables async mode)
    void onAsyncResponse(uint8_t functionCode, uint16_t address,
                        const uint8_t* data, size_t length) override;

private:
    Config _config;
    TemperatureData _lastReading;
    bool _connected;

    // Unified mapping architecture (simple binding)
    int16_t* _temperaturePtr;  // Pointer to tenths of degrees (Temperature_t)
    bool* _validityPtr;

    // Per-kind read outcome counters, indexed by ReadError
    std::array<uint32_t, static_cast<size_t>(ReadError::COUNT)> _readCounts{};

    // Consecutive 0x0000/0xFFFF answers (log escalation, isConnected() grace)
    uint8_t _consecutive0x0000Errors;

    // Firmware detection / status-register mode
    Firmware _firmware = Firmware::UNKNOWN;
    uint16_t _firmwareVersion = 0;
    StatusFallback _statusFallback = StatusFallback::NONE;
    uint16_t _lastStatus = 0;
    uint32_t _lastFreshMs = 0;     // millis() of the last fresh (not bridged) measurement
    bool _hasFreshRead = false;

    // Internal methods
    bool performRead();
    bool performStatusRead();
    bool processRegisterValue(uint16_t value);
    bool processStatusValue(uint16_t value, uint16_t status);
    bool recordSensorFault(const char* message);
    void fallBackToSingleRegister(StatusFallback reason);
    bool recordModbusFailure(ModbusError error);
    bool recordFailure(ReadError kind, const char* message, bool connected);
    void recordSuccess(int16_t value, bool fresh = true);

    // Constants
    static constexpr uint16_t TEMP_REGISTER = 50;      // Temperature register (0-based)
    static constexpr uint8_t FUNCTION_CODE = 0x04;     // Read Input Registers
    static constexpr uint16_t REGISTER_COUNT = 1;      // Single register
    static constexpr uint16_t STATUS_REGISTER_COUNT = 2;   // 50..51 in status mode
    static constexpr uint16_t VERSION_REGISTER = 2;    // firmware version
    static constexpr uint16_t OPEN_FIRMWARE_FLAG = 0x8000;

    static constexpr int16_t TEMP_MIN = -400;          // -40.0°C
    static constexpr int16_t TEMP_MAX = 1250;          // +125.0°C
};

} // namespace andrtf3

#endif // ANDRTF3_H