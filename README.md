# ANDRTF3 Temperature Sensor Library

Simple Arduino library for the ANDRTF3/MD wall-mount RS485 temperature sensor from Andivi.

## Features

- Simple temperature reading via Modbus RTU
- Temperature range: -40°C to +125°C with 0.1°C resolution
- Built on ModbusDevice v2.1.0 framework
- Synchronous and asynchronous operation
- Fixed-point temperature format (no floating point math)

## Hardware

The ANDRTF3/MD is a wall-mount temperature sensor with:
- RS485 Modbus RTU interface
- Default address: 3
- Default settings: 9600 baud, 8N1
- Temperature register: 50 (input register)
- Power: 12-24V DC

## Installation

### PlatformIO

Add to your `platformio.ini`:

```ini
lib_deps = 
    ANDRTF3
```

## Usage

```cpp
#include <Arduino.h>
#include <esp32ModbusRTU.h>
#include <ModbusDevice.h>
#include <ANDRTF3.h>

// Use the andrtf3 namespace
using namespace andrtf3;

// Create Modbus RTU instance
esp32ModbusRTU modbusRTU(&Serial2, -1);  // -1 for auto-direction transceiver

// Create sensor instance
ANDRTF3 sensor(3);  // Address 3

void setup() {
    Serial.begin(115200);
    Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
    
    // Initialize Modbus
    modbusRTU.begin();
    setGlobalModbusRTU(&modbusRTU);
    
    // Register callbacks for ModbusDevice framework
    modbusRTU.onData(mainHandleData);
    modbusRTU.onError([](esp32Modbus::Error error) {
        handleError(0, error);
    });
}

void loop() {
    // Read temperature (returns temperature * 10)
    if (sensor.readTemperature()) {
        int16_t temp = sensor.getTemperature();  // 261 = 26.1°C
        Serial.printf("Temperature: %d.%d°C\n", temp / 10, abs(temp % 10));
    }
    
    delay(5000);
}
```

## API Reference

### Basic Methods

- `readTemperature()` - Synchronous temperature read
- `getTemperature()` - Get last temperature in deci-degrees Celsius (261 = 26.1°C)
- `getTemperatureData()` - Get complete temperature data structure
- `isConnected()` - Check if sensor is responding

### Async Methods

- `requestTemperature()` - Start async temperature read
- `isReadComplete()` - Check if async read is complete
- `getAsyncResult(data)` - Get async read result
- `process()` - Process queued responses (call in loop when using async)

### Configuration

```cpp
ANDRTF3::Config config;
config.timeout = 1000;  // Response timeout in ms
config.retries = 3;     // Number of retries

sensor.setConfig(config);
```

## Temperature Format

This library uses fixed-point arithmetic to avoid floating-point operations:
- Temperature values are stored as `int16_t` 
- Values are in deci-degrees (value * 10)
- Example: 261 = 26.1°C, -45 = -4.5°C

## Examples

See the `examples/basic` folder for a complete working example.

## Dependencies

- [ModbusDevice](https://github.com/your-repo/ModbusDevice) v2.1.0
- [esp32ModbusRTU](https://github.com/your-repo/esp32ModbusRTU)

## Advanced Features

For users who need advanced features like callbacks, auto-read timers, statistics, and multi-device management, please use the full-featured version available in the `feature/full-featured-v0.5` branch.

## Debugging

To enable debug output, define `ANDRTF3_DEBUG` in your build flags:

```ini
build_flags = 
    -DANDRTF3_DEBUG
```

This will output detailed information about:
- Raw Modbus data received
- Temperature value conversions
- State changes
- Warnings when 0°C (0x0000) is received

### Custom Logging

The library uses logging macros that default to no-op. To use custom logging, define these macros before including ANDRTF3.h:

```cpp
// Example: Use ESP-IDF logging
#define ANDRTF3_LOG_E(tag, format, ...) ESP_LOGE(tag, format, ##__VA_ARGS__)
#define ANDRTF3_LOG_W(tag, format, ...) ESP_LOGW(tag, format, ##__VA_ARGS__)
#define ANDRTF3_LOG_I(tag, format, ...) ESP_LOGI(tag, format, ##__VA_ARGS__)
#define ANDRTF3_LOG_D(tag, format, ...) ESP_LOGD(tag, format, ##__VA_ARGS__)

#include <ANDRTF3.h>
```

## License

MIT License - see LICENSE file for details.

## Version History

- 1.0.3 - Replace Serial.print with logging macros (2025-08-01)
  - Replaced all direct Serial.print/printf usage with ANDRTF3_LOG_* macros
  - Library no longer writes to Serial port directly
  - Applications can override logging macros to use their own logging system
  - Added ANDRTF3Logging.h with empty macro definitions

- 1.0.2 - Fix 0x0000 sensor error handling (2025-01-18)
  - Fixed critical bug where sensor returning 0x0000 was treated as valid 0°C
  - 0x0000 is now correctly identified as sensor error/communication fault
  - Previous temperature value is preserved when error occurs
  - Prevents heating systems from incorrectly activating on sensor errors

- 1.0.1 - Debug logging (2025-01-18)
  - Added comprehensive debug output to diagnose 0°C reading issue
  - Helps identify if 0x0000 is from sensor or library state management

- 1.0.0 - Simplified architecture
  - Removed auto-read, callbacks, statistics, diagnostics
  - Removed IDeviceInstance interface and FreeRTOS resources
  - Removed ANDRTF3Manager class
  - Follows MB8ART pattern - library handles only Modbus communication
  - Application controls timing and synchronization

- 0.5.0 - Previous full-featured version (see feature/full-featured-v0.5 branch)