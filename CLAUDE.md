# ANDRTF3 Library - CLAUDE.md

## Overview
ANDRTF3 is a driver library for the ANDRTF3/MD wall-mount RS485 temperature sensor from Andivi. The library is built on the ModbusDevice v2.1.0 framework and provides both synchronous and asynchronous temperature reading capabilities.

## Key Features
- Temperature reading via Modbus RTU (register 50)
- Range: -40°C to +125°C with 0.1°C resolution
- **Temperature_t (int16_t)** format - tenths of degrees
- Simple pointer binding for direct updates
- Inherits from QueuedModbusDevice for async operation
- Lightweight design (no IDeviceInstance overhead)

## Simple Unified Mapping

The library provides lightweight pointer binding without heavy interface overhead.

### Usage Example:
```cpp
// 1. Create ANDRTF3 instance
andrtf3::ANDRTF3 sensor(3);

// 2. Bind temperature pointers (library writes directly)
int16_t insideTemp = 0;      // Temperature_t format (tenths)
bool isTempValid = false;
sensor.bindTemperaturePointers(&insideTemp, &isTempValid);

// 3. Read temperature
sensor.requestTemperature();
// insideTemp automatically updated (e.g., 245 = 24.5°C)
// isTempValid automatically updated (true/false)
```

### Temperature Format:
- Stored as `int16_t` (tenths of degrees)
- ANDRTF3 hardware returns deci-degrees - perfect match!
- Direct assignment, no conversion needed

## Architecture
The library follows the MB8ART pattern:
- Inherits from both `QueuedModbusDevice` and `IDeviceInstance`
- Implements `onAsyncResponse()` for handling queued Modbus responses
- Uses base class methods for actual Modbus communication
- Supports both sync and async operation modes
- Provides IDeviceInstance interface for standardized device interaction
- Thread-safe with FreeRTOS mutexes and event groups

## Implementation Details

### Class Hierarchy
```
IModbusDevice                    IDeviceInstance
    └── ModbusDevice                    │
            └── QueuedModbusDevice ─────┤
                                       │
                    ANDRTF3 ───────────┘
```

### Key Methods
- `readTemperature()` - Synchronous temperature read
- `requestTemperature()` - Asynchronous temperature read
- `onAsyncResponse()` - Handles queued Modbus responses
- `process()` - Processes queue and handles auto-read

### Modbus Details
- Function Code: 0x04 (Read Input Registers)
- Register: 50 (0x0032)
- Data Format: Signed 16-bit integer in deci-degrees Celsius
- Default Settings: Address 3, 9600 baud, 8E1

## Usage Example
```cpp
// Create sensor
ANDRTF3 sensor(3);  // Address 3

// Register with ModbusDevice framework
ModbusDevice::registerDevice(&sensor);

// Read temperature
if (sensor.readTemperature()) {
    float temp = sensor.getTemperature();
    Serial.printf("Temperature: %.1f°C\n", temp);
}

// Async read
sensor.requestTemperature();
// ... do other work ...
if (sensor.isReadComplete()) {
    ANDRTF3::TemperatureData data;
    sensor.getAsyncResult(data);
}
```

## Build Commands
```bash
# Build the library
cd ~/.platformio/lib/ESP32-ANDRTF3
pio run

# Run tests (if available)
pio test
```

## Recent Changes (2025-07-18)
- **v1.1.0**: Initial implementation with QueuedModbusDevice
  - Migrated from IModbusDevice to QueuedModbusDevice inheritance
  - Implemented actual Modbus RTU communication (replaced simulated values)
  - Added onAsyncResponse() for handling queued responses
  - Updated performRead() to use base class readInputRegisters()
  - Added proper initialization phase setting
  - Updated dependencies to include ModbusDevice v2.1.0

- **v2.0.0**: Added IDeviceInstance support
  - Now inherits from both QueuedModbusDevice and IDeviceInstance (like MB8ART)
  - Implemented full IDeviceInstance interface
  - Added FreeRTOS resources (mutexes, event groups)
  - Added event notification system
  - Thread-safe data access with mutex guards
  - Support for generic device callbacks via IDeviceInstance
  - Added dependency on IDeviceInstance v4.0.0

- **v2.0.1**: MutexGuard compatibility fix
  - Fixed MutexGuard API usage (changed isLocked() to hasLock())
  - Added MutexGuard as explicit dependency
  - Removed owner prefixes from dependencies for broader compatibility

- **v2.0.2**: Compilation fixes
  - Fixed incorrect Result member access (error() -> error, value() -> value)
  - Added local modbusErrorToString() helper function
  - Fixed compilation errors with ModbusResult usage

## Template Conflict Resolution
The ANDRTF3 library has been carefully implemented to avoid template conflicts:
- Never uses the raw `Result<T>` template from ModbusTypes.h
- Uses `ModbusResult<T>` for all Modbus operations (properly type-aliased)
- Uses `IDeviceInstance::DeviceResult<T>` for all IDeviceInstance operations
- While ModbusTypes.h is included (needed for ModbusError enum), the problematic global `Result` template is never used

See TEMPLATE_CONFLICT_RESOLUTION.md for detailed information.

## Future Improvements
- Add temperature alarm thresholds
- Implement sensor address auto-discovery
- Add temperature averaging/filtering options
- Support for reading multiple sensors in batch