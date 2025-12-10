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

// Constructor
ANDRTF3::ANDRTF3(uint8_t address)
    : QueuedModbusDevice(address),
      _connected(false),
      _asyncStartTime(0),
      _temperaturePtr(nullptr),
      _validityPtr(nullptr),
      _consecutive0x0000Errors(0),
      _lastErrorTime(0) {
    // _asyncPending is initialized via in-class initializer (std::atomic<bool>{false})

    _config = getDefaultConfig();
    _config.address = address;

    _lastReading.celsius = 0;
    _lastReading.timestamp = 0;
    _lastReading.valid = false;

    ANDRTF3_LOG_D("Constructor: Init celsius=%d, valid=%d",
                  _lastReading.celsius, _lastReading.valid);

    // Set init phase for proper operation
    setInitPhase(InitPhase::READY);

    // Register device with ModbusDevice framework
    registerDevice();
}

bool ANDRTF3::readTemperature() {
    bool success = performRead();
    
    if (!success) {
        _connected = false;
    }
    
    return success;
}

int16_t ANDRTF3::getTemperature() const noexcept {
    return _lastReading.celsius;
}

bool ANDRTF3::requestTemperature() {
    // Clear any stale pending state if timed out
    if (_asyncPending.load() && (millis() - _asyncStartTime) > _config.timeout) {
        _asyncPending.store(false);
    }

    if (_asyncPending.load()) {
        return false;
    }

    _asyncPending.store(true);
    _asyncStartTime = millis();

    // Perform synchronous read and process result immediately
    auto result = readInputRegistersWithPriority(TEMP_REGISTER, 1, esp32Modbus::SENSOR);
    uint8_t addr = getServerAddress();

    if (!result.isOk()) {
        auto category = modbus::ModbusErrorTracker::categorizeError(result.error());
        modbus::ModbusErrorTracker::recordError(addr, category);
        _asyncPending.store(false);
        _lastReading.valid = false;
        _lastReading.error = modbusErrorToString(result.error());
        _connected = false;
        return false;
    }

    auto values = result.value();
    if (values.empty()) {
        modbus::ModbusErrorTracker::recordError(addr, modbus::ModbusErrorTracker::ErrorCategory::INVALID_DATA);
        _asyncPending.store(false);
        _lastReading.valid = false;
        _lastReading.error = "No data returned";
        _connected = false;
        return false;
    }

    int16_t rawValue = static_cast<int16_t>(values[0]);

    // Check for error codes
    if (rawValue == 0 || values[0] == 0xFFFF) {
        _consecutive0x0000Errors++;
        modbus::ModbusErrorTracker::recordError(addr, modbus::ModbusErrorTracker::ErrorCategory::INVALID_DATA);

        // Natural retry strategy: Use 5-second ModbusCoordinator tick interval
        // First error: silent (wait for next poll to confirm)
        // Second+ error: log ERROR (persistent fault confirmed)
        if (_consecutive0x0000Errors >= 2) {
            ANDRTF3_LOG_E("ERROR: Persistent 0x%04X (%d consecutive) - sensor fault confirmed",
                          values[0], _consecutive0x0000Errors);
        } else {
            // First error: silent tracking (coordinator will retry in 5 seconds)
            ANDRTF3_LOG_D("First 0x%04X detected - will verify on next poll (5s)", values[0]);
        }

        _asyncPending.store(false);
        _lastReading.valid = false;
        _lastReading.error = (values[0] == 0xFFFF) ? "Modbus error 0xFFFF" : "Sensor returned 0x0000";
        _connected = (_consecutive0x0000Errors < 3);  // Only disconnect after 3+ consecutive errors
        _lastErrorTime = millis();
        return false;
    }

    // Validate range
    if (rawValue < TEMP_MIN || rawValue > TEMP_MAX) {
        modbus::ModbusErrorTracker::recordError(addr, modbus::ModbusErrorTracker::ErrorCategory::INVALID_DATA);
        _asyncPending.store(false);
        _lastReading.valid = false;
        _lastReading.error = "Temperature out of range";
        _connected = false;
        return false;
    }

    // SUCCESS - store result immediately
    modbus::ModbusErrorTracker::recordSuccess(addr);
    _lastReading.celsius = rawValue;
    _lastReading.timestamp = millis();
    _lastReading.valid = true;
    _lastReading.error = "";
    _connected = true;
    _consecutive0x0000Errors = 0;  // Reset error counter on success

    // Update bound pointers
    if (_temperaturePtr != nullptr) {
        *_temperaturePtr = rawValue;
    }
    if (_validityPtr != nullptr) {
        *_validityPtr = true;
    }

    _asyncPending.store(false);  // Already complete!
    return true;
}

bool ANDRTF3::isReadComplete() const noexcept {
    // Sync read completes immediately, so always complete after requestTemperature()
    return !_asyncPending.load();
}

bool ANDRTF3::getAsyncResult(TemperatureData& data) {
    // Result is already in _lastReading from requestTemperature()
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
    // Use the base class to read the temperature register with SENSOR priority
    auto result = readInputRegistersWithPriority(TEMP_REGISTER, 1, esp32Modbus::SENSOR);
    uint8_t addr = getServerAddress();

    ANDRTF3_LOG_D("performRead: ModbusResult ok=%d, error=%d",
                  result.isOk(), static_cast<int>(result.error()));

    if (!result.isOk()) {
        auto category = modbus::ModbusErrorTracker::categorizeError(result.error());
        modbus::ModbusErrorTracker::recordError(addr, category);
        _lastReading.valid = false;
        _lastReading.error = modbusErrorToString(result.error());
        _connected = false;
        return false;
    }

    // Get the temperature value
    auto values = result.value();

    ANDRTF3_LOG_D("performRead: values.size()=%d", values.size());

    if (values.empty()) {
        modbus::ModbusErrorTracker::recordError(addr, modbus::ModbusErrorTracker::ErrorCategory::INVALID_DATA);
        _lastReading.valid = false;
        _lastReading.error = "No data returned";
        _connected = false;
        return false;
    }

    int16_t rawValue = static_cast<int16_t>(values[0]);

    ANDRTF3_LOG_D("performRead: raw uint16=0x%04X (%u), as int16=%d",
                  values[0], values[0], rawValue);

    // Check for Modbus error codes:
    // 0x0000 = Sensor error or communication fault
    // 0xFFFF = Common Modbus error/no response (-1 as signed)
    if (rawValue == 0 || values[0] == 0xFFFF) {
        _consecutive0x0000Errors++;
        modbus::ModbusErrorTracker::recordError(addr, modbus::ModbusErrorTracker::ErrorCategory::INVALID_DATA);

        // Natural retry strategy: Use 5-second ModbusCoordinator tick interval
        // First error: silent (wait for next poll to confirm)
        // Second+ error: log ERROR (persistent fault confirmed)
        if (_consecutive0x0000Errors >= 2) {
            ANDRTF3_LOG_E("ERROR: Persistent 0x%04X (%d consecutive) - sensor fault confirmed",
                          values[0], _consecutive0x0000Errors);
        } else {
            // First error: silent tracking (coordinator will retry in 5 seconds)
            ANDRTF3_LOG_D("First 0x%04X detected - will verify on next poll (5s)", values[0]);
        }

        _lastReading.valid = false;
        _lastReading.error = (values[0] == 0xFFFF) ? "Modbus error 0xFFFF" : "Sensor returned 0x0000";
        _connected = (_consecutive0x0000Errors < 3);  // Only disconnect after 3+ consecutive errors
        _lastErrorTime = millis();
        // Do NOT update celsius value - keep previous reading
        return false;
    }

    // Validate range
    if (rawValue < TEMP_MIN || rawValue > TEMP_MAX) {
        modbus::ModbusErrorTracker::recordError(addr, modbus::ModbusErrorTracker::ErrorCategory::INVALID_DATA);
        _lastReading.valid = false;
        _lastReading.error = "Temperature out of range";
        _connected = false;
        return false;
    }

    // Store raw value (already in deci-degrees)
    modbus::ModbusErrorTracker::recordSuccess(addr);
    _lastReading.celsius = rawValue;
    _lastReading.timestamp = millis();
    _lastReading.valid = true;
    _lastReading.error = "";
    _connected = true;
    _consecutive0x0000Errors = 0;  // Reset error counter on success

    return true;
}

void ANDRTF3::handleReadComplete(bool success, int16_t value) {
    ANDRTF3_LOG_D("handleReadComplete: success=%d, value=%d, "
                  "prev_celsius=%d, prev_valid=%d",
                  success, value, _lastReading.celsius, _lastReading.valid);

    if (success) {
        _lastReading.celsius = value;  // Already in deci-degrees
        _lastReading.timestamp = millis();
        _lastReading.valid = true;
        _lastReading.error = "";
        _connected = true;

        // Update bound pointers (unified mapping architecture)
        // Value is already in tenths of degrees - perfect for Temperature_t!
        if (_temperaturePtr != nullptr) {
            *_temperaturePtr = value;  // Direct assignment (both are int16_t tenths)
        }
        if (_validityPtr != nullptr) {
            *_validityPtr = true;
        }

        if (value == 0) {
            ANDRTF3_LOG_W("WARNING: handleReadComplete set celsius to 0 - suspicious!");
        }
    } else {
        _lastReading.valid = false;
        _connected = false;

        // Update bound pointers for error case
        if (_validityPtr != nullptr) {
            *_validityPtr = false;
        }

        // Note: celsius value is NOT updated on error - retains previous value

        ANDRTF3_LOG_D("handleReadComplete: Error - celsius remains %d",
                      _lastReading.celsius);
    }
}

// Handle async Modbus responses
void ANDRTF3::onAsyncResponse(uint8_t functionCode, uint16_t address,
                             const uint8_t* data, size_t length) {
    ANDRTF3_LOG_D("onAsyncResponse: FC=0x%02X, addr=%d, len=%d",
                  functionCode, address, length);

    uint8_t addr = getServerAddress();

    // We only expect input register reads
    if (functionCode != FUNCTION_CODE) {
        return;
    }

    // Check if this is our temperature register
    if (address != TEMP_REGISTER) {
        return;
    }

    // Process temperature data
    if (length >= 2) {
        int16_t rawValue = (data[0] << 8) | data[1];

        ANDRTF3_LOG_D("onAsyncResponse: data[0]=0x%02X, data[1]=0x%02X, "
                      "rawValue=%d (0x%04X)", data[0], data[1], rawValue,
                      static_cast<uint16_t>(rawValue));

        // Check for Modbus error codes:
        // 0x0000 = Sensor error or communication fault
        // 0xFFFF = Common Modbus error/no response
        uint16_t unsignedValue = static_cast<uint16_t>(rawValue);
        if (rawValue == 0 || unsignedValue == 0xFFFF) {
            _consecutive0x0000Errors++;
            modbus::ModbusErrorTracker::recordError(addr, modbus::ModbusErrorTracker::ErrorCategory::INVALID_DATA);
            ANDRTF3_LOG_E("ERROR: Received 0x%04X (%d consecutive) - sensor error or communication fault!",
                          unsignedValue, _consecutive0x0000Errors);
            _lastReading.valid = false;
            _lastReading.error = (unsignedValue == 0xFFFF) ? "Modbus error 0xFFFF" : "Sensor returned 0x0000";
            _connected = (_consecutive0x0000Errors < 3);  // Only disconnect after 3+ consecutive errors
            _lastErrorTime = millis();
            // Do NOT update celsius value - keep previous reading
        } else {
            _consecutive0x0000Errors = 0;  // Reset error counter on success
            modbus::ModbusErrorTracker::recordSuccess(addr);
            handleReadComplete(true, rawValue);
        }
    } else {
        modbus::ModbusErrorTracker::recordError(addr, modbus::ModbusErrorTracker::ErrorCategory::INVALID_DATA);
        _lastReading.valid = false;
        _lastReading.error = "Invalid response length";
        _connected = false;

        ANDRTF3_LOG_D("onAsyncResponse: Invalid length %d, expected >= 2", length);
    }

    // Mark async operation complete (atomic store for thread safety)
    _asyncPending.store(false);
}

// Static methods
ANDRTF3::Config ANDRTF3::getDefaultConfig() {
    return {
        3,        // address
        200,      // timeout (increased from 100ms to account for library overhead)
        3         // retries
    };
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