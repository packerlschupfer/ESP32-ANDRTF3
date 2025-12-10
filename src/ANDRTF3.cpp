#include "ANDRTF3.h"
#include "ANDRTF3Logging.h"

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
      _asyncPending(false),
      _asyncStartTime(0),
      _temperaturePtr(nullptr),
      _validityPtr(nullptr) {

    _config = getDefaultConfig();
    _config.address = address;

    _lastReading.celsius = 0;
    _lastReading.timestamp = 0;
    _lastReading.valid = false;
    
    ANDRTF3_LOG_D("ANDRTF3", "Constructor: Init celsius=%d, valid=%d", 
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

int16_t ANDRTF3::getTemperature() const {
    return _lastReading.celsius;
}

bool ANDRTF3::requestTemperature() {
    // Clear any stale pending state if timed out
    if (_asyncPending && (millis() - _asyncStartTime) > _config.timeout) {
        _asyncPending = false;
    }
    
    if (_asyncPending) {
        return false;
    }
    
    // Enable async mode if not already enabled
    if (!isAsyncEnabled()) {
        if (!enableAsync(5)) {
            return false;
        }
    }
    
    _asyncPending = true;
    _asyncStartTime = millis();

    // Start the async read with SENSOR priority (safety-critical data)
    auto result = readInputRegistersWithPriority(TEMP_REGISTER, 1, esp32Modbus::SENSOR);
    if (!result.isOk()) {
        _asyncPending = false;
        return false;
    }

    return true;
}

bool ANDRTF3::isReadComplete() const {
    if (!_asyncPending) return true;
    
    // Check if the read has timed out
    if ((millis() - _asyncStartTime) >= _config.timeout) {
        return true;
    }
    
    return false;
}

bool ANDRTF3::getAsyncResult(TemperatureData& data) {
    // Always clear pending flag when getting result
    bool wasPending = _asyncPending;
    _asyncPending = false;
    
    ANDRTF3_LOG_D("ANDRTF3", "getAsyncResult: wasPending=%d, valid=%d, "
                  "celsius=%d, timestamp=%lu, millis=%lu", 
                  wasPending, _lastReading.valid, _lastReading.celsius, 
                  _lastReading.timestamp, millis());
    
    if (!wasPending) {
        // No async operation was pending, return last reading
        data = _lastReading;
        return _lastReading.valid;
    }
    
    // Check if the read timed out
    uint32_t elapsed = millis() - _asyncStartTime;
    if (elapsed >= _config.timeout) {
        // Timeout occurred
        _lastReading.valid = false;
        _lastReading.error = "Timeout";
        data = _lastReading;
        return false;
    }
    
    // If we have a valid recent reading from onAsyncResponse, use it
    if (_lastReading.valid && (millis() - _lastReading.timestamp) < _config.timeout) {
        data = _lastReading;
        return true;
    }
    
    // Otherwise, perform a synchronous read to get the result
    bool success = performRead();
    data = _lastReading;
    return success;
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

    ANDRTF3_LOG_D("ANDRTF3", "performRead: ModbusResult ok=%d, error=%d",
                  result.isOk(), static_cast<int>(result.error()));
    
    if (!result.isOk()) {
        _lastReading.valid = false;
        _lastReading.error = modbusErrorToString(result.error());
        _connected = false;
        return false;
    }
    
    // Get the temperature value
    auto values = result.value();
    
    ANDRTF3_LOG_D("ANDRTF3", "performRead: values.size()=%d", values.size());
    
    if (values.empty()) {
        _lastReading.valid = false;
        _lastReading.error = "No data returned";
        _connected = false;
        return false;
    }
    
    int16_t rawValue = static_cast<int16_t>(values[0]);
    
    ANDRTF3_LOG_D("ANDRTF3", "performRead: raw uint16=0x%04X (%u), as int16=%d", 
                  values[0], values[0], rawValue);
    
    // Check for Modbus error codes:
    // 0x0000 = Sensor error or communication fault
    // 0xFFFF = Common Modbus error/no response (-1 as signed)
    if (rawValue == 0 || values[0] == 0xFFFF) {
        ANDRTF3_LOG_E("ANDRTF3", "ERROR: Received 0x%04X - sensor error or communication fault!", values[0]);
        _lastReading.valid = false;
        _lastReading.error = (values[0] == 0xFFFF) ? "Modbus error 0xFFFF" : "Sensor returned 0x0000";
        _connected = false;
        // Do NOT update celsius value - keep previous reading
        return false;
    }
    
    // Validate range
    if (rawValue < TEMP_MIN || rawValue > TEMP_MAX) {
        _lastReading.valid = false;
        _lastReading.error = "Temperature out of range";
        _connected = false;
        return false;
    }
    
    // Store raw value (already in deci-degrees)
    _lastReading.celsius = rawValue;
    _lastReading.timestamp = millis();
    _lastReading.valid = true;
    _lastReading.error = "";
    _connected = true;
    
    return true;
}

void ANDRTF3::handleReadComplete(bool success, int16_t value) {
    ANDRTF3_LOG_D("ANDRTF3", "handleReadComplete: success=%d, value=%d, "
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
            ANDRTF3_LOG_W("ANDRTF3", "WARNING: handleReadComplete set celsius to 0 - suspicious!");
        }
    } else {
        _lastReading.valid = false;
        _connected = false;

        // Update bound pointers for error case
        if (_validityPtr != nullptr) {
            *_validityPtr = false;
        }

        // Note: celsius value is NOT updated on error - retains previous value

        ANDRTF3_LOG_D("ANDRTF3", "handleReadComplete: Error - celsius remains %d",
                      _lastReading.celsius);
    }
}

// Handle async Modbus responses
void ANDRTF3::onAsyncResponse(uint8_t functionCode, uint16_t address,
                             const uint8_t* data, size_t length) {
    ANDRTF3_LOG_D("ANDRTF3", "onAsyncResponse: FC=0x%02X, addr=%d, len=%d", 
                  functionCode, address, length);
    
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
        
        ANDRTF3_LOG_D("ANDRTF3", "onAsyncResponse: data[0]=0x%02X, data[1]=0x%02X, "
                      "rawValue=%d (0x%04X)", data[0], data[1], rawValue, 
                      static_cast<uint16_t>(rawValue));
        
        // Check for Modbus error codes:
        // 0x0000 = Sensor error or communication fault
        // 0xFFFF = Common Modbus error/no response
        uint16_t unsignedValue = static_cast<uint16_t>(rawValue);
        if (rawValue == 0 || unsignedValue == 0xFFFF) {
            ANDRTF3_LOG_E("ANDRTF3", "ERROR: Received 0x%04X - sensor error or communication fault!", unsignedValue);
            _lastReading.valid = false;
            _lastReading.error = (unsignedValue == 0xFFFF) ? "Modbus error 0xFFFF" : "Sensor returned 0x0000";
            _connected = false;
            // Do NOT update celsius value - keep previous reading
        } else {
            handleReadComplete(true, rawValue);
        }
    } else {
        _lastReading.valid = false;
        _lastReading.error = "Invalid response length";
        _connected = false;
        
        ANDRTF3_LOG_D("ANDRTF3", "onAsyncResponse: Invalid length %d, expected >= 2", length);
    }
    
    // Mark async operation complete
    _asyncPending = false;
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
    ANDRTF3_LOG_D("ANDRTF3", "Binding temperature pointers (unified mapping API)");

    _temperaturePtr = tempPtr;
    _validityPtr = validPtr;

    if (tempPtr != nullptr && validPtr != nullptr) {
        ANDRTF3_LOG_D("ANDRTF3", "Temperature bound to temp=0x%p (int16_t tenths), valid=0x%p", tempPtr, validPtr);
    } else if (tempPtr == nullptr && validPtr == nullptr) {
        ANDRTF3_LOG_D("ANDRTF3", "Temperature pointers unbound (both nullptr)");
    } else {
        ANDRTF3_LOG_W("ANDRTF3", "Partial binding - temp=0x%p, valid=0x%p", tempPtr, validPtr);
    }
}

} // namespace andrtf3