# ANDRTF3 Temperature Sensor Library

Simple Arduino library for the ANDRTF3/MD wall-mount RS485 temperature sensor from Andivi.

## Features

- Simple temperature reading via Modbus RTU
- Temperature range: -40°C to +125°C with 0.1°C resolution
- Built on ModbusDevice v2.1.0 framework
- Synchronous and asynchronous operation
- Fixed-point temperature format (no floating point math)

## Hardware

The ANDRTF3/MD is a wall-mount temperature sensor from Andivi with:
- **Sensor**: PT1000 Class B, DIN EN 60751 (2-wire)
- **Range**: -40°C to +125°C
- **Accuracy**: ±0.2K ±1.0% full scale (after 60min)
- **Interface**: RS485 Modbus RTU/ASCII
- **Default Settings**: 9600 baud, 8N1, address 3
- **Temperature Register**: 50 (PDU) / 51 (mbpoll) - Input Register (FC 0x04)
- **Power**: 20-34V AC/DC, 10-20mA
- **Enclosure**: 87.5×87.5×30mm, IP30

📖 **[Complete Hardware Documentation](docs/ANDRTF3_HARDWARE.md)**
📖 **[Register Specifications](docs/ANDRTF3_REGISTERS.md)**

**Manufacturer**: [Andivi d.o.o., Slovenia](https://www.andivi.com/)
**Datasheet**: [PDF](https://www.andivi.com/wp-content/uploads/2024/09/ANDRTF3-MD-Datasheet-%E2%80%93-Andivi.pdf)

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

## Documentation

- 📖 **[Hardware Documentation](docs/ANDRTF3_HARDWARE.md)** - Complete hardware specs, pinouts, DIP switches, timing
- 📖 **[Register Specifications](docs/ANDRTF3_REGISTERS.md)** - Detailed register map and Modbus protocol
- 📝 **[Testing Guide](TESTING.md)** - Testing procedures and troubleshooting
- 🔧 **[Register Scanning Guide](REGISTER_SCANNING_GUIDE.md)** - Tools to scan and identify registers
- 🐛 **[Timeout Analysis](FINAL_ANALYSIS.md)** - Analysis of 2% timeout issue and fix

### Quick Register Reference

| Register | Address (mbpoll) | Function Code | Description | Format |
|----------|------------------|---------------|-------------|--------|
| 50 (PDU) | 51 | 0x04 | Temperature | int16_t × 10 (deci-degrees) |

**Example**: Register value 264 = 26.4°C

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

- 0.1.0 - Initial release (2025-01-10)
  - Simple temperature reading via Modbus RTU
  - Built on QueuedModbusDevice framework
  - Synchronous and asynchronous operation modes
  - Fixed-point temperature format (deci-degrees)
  - Default timeout: 200ms (optimized for library overhead)
  - Comprehensive hardware documentation from Andivi datasheet
  - Complete register specifications and Modbus protocol details
  - Error value handling (rejects 0x0000 and 0xFFFF as sensor errors)
  - Logging macros (no direct Serial output, customizable)
  - Temperature range: -40°C to +125°C with 0.1°C resolution
  - PT1000 Class B sensor, ±0.2K ±1.0% full scale accuracy