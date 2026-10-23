# ANDRTF3 Temperature Sensor Library

Simple Arduino library for the ANDRTF3/MD wall-mount RS485 temperature sensor from Andivi.

## Features

- Simple temperature reading via Modbus RTU
- Temperature range: -40°C to +125°C with 0.1°C resolution
- Built on ModbusDevice v2.1.0 framework
- Synchronous and asynchronous operation
- Fixed-point temperature format (no floating point math)
- Optional support for the [open ANDRTF3 firmware](https://github.com/packerlschupfer/ANDRTF3-firmware): status register,
  firmware detection, diagnostics (opt-in; the default stays byte-identical on the bus)

## Hardware

The ANDRTF3/MD is a wall-mount temperature sensor from Andivi with:
- **Sensor**: PT1000 Class B, DIN EN 60751 (2-wire)
- **Range**: -40°C to +125°C
- **Accuracy**: ±0.2K ±1.0% full scale (after 60min)
- **Interface**: RS485 Modbus RTU/ASCII
- **Default Settings**: 9600 baud, 8E1 (even parity), address 3
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
    Serial2.begin(9600, SERIAL_8E1, RX_PIN, TX_PIN);
    
    // Initialize Modbus
    modbusRTU.begin();
    setGlobalModbusRTU(&modbusRTU);
    
    // Register callbacks for ModbusDevice framework
    modbusRTU.onData(mainHandleData);
    // Pass the server address: handleError() finds the device by address,
    // otherwise exceptions and CRC errors end as timeouts
    modbusRTU.onError([](uint16_t serverAddress, esp32Modbus::Error error) {
        handleError(static_cast<uint8_t>(serverAddress), error);
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
- `getLastReadError()` - Why the last read failed (`ReadError::NONE` after a success)
- `getLastGoodReadAgeMs()` - Milliseconds since the last good read (`UINT32_MAX` if never)
- `getReadErrorCount(kind)` - Cumulative count per `ReadError` (`NONE` = successful reads); `resetReadErrorCounts()`
- `bindTemperaturePointers(&temp, &valid)` - Let the library write each result into your variables

### Failure classification

`ReadError` (also in `TemperatureData::errorKind`) separates bus contention from a dead sensor:

| Value | Meaning |
|-------|---------|
| `BUS_BUSY` | Bus mutex not acquired - nothing was sent (another device held the bus) |
| `TIMEOUT` | Request sent, sensor did not answer |
| `CRC` | Answer corrupted (noise, wrong parity) |
| `DEVICE_EXCEPTION` | Sensor answered with a Modbus exception |
| `BAD_RESPONSE` | Answer malformed or empty |
| `SENSOR_FAULT` | Sensor answered 0x0000 / 0xFFFF |
| `OUT_OF_RANGE` | Value outside -40.0..125.0°C |
| `OTHER` | Driver/framework error |

Note: because 0x0000 is the sensor's fault value, a genuine 0.0°C reading is reported as `SENSOR_FAULT`.

### Open firmware (opt-in)

For sensors running the [open firmware](https://github.com/packerlschupfer/ANDRTF3-firmware). Nothing changes unless you call
these; the default read stays FC04 register 50, count 1, with no init reads.

- `detectFirmware()` - one read of input register 2; `getFirmware()` = `ORIGINAL` / `OPEN`,
  `getFirmwareVersion()` = raw value (open: bit 15 set, e.g. 0x8001 = v0.1; original: 7)
- `Config::useStatusRegister` - read 50..51 in one transaction; status bit 0 decides validity:
  - set: valid, including a genuine 0.0°C (no more `SENSOR_FAULT` at freezing point)
  - clear: `SENSOR_FAULT`, with the reason in `TemperatureData::error`
  - bit 5 (value bridged by the sensor's filter, at most 2 readings): valid, `timestamp` advances,
    `getLastGoodReadAgeMs()` keeps growing - it is the age of the underlying measurement
- Degrades, never fails: exception 02 on the 2-register read, or a status word the open firmware
  cannot produce (the original answers 0; bits above 5 are ignored for this check), switches back to the count-1 read for the life of the
  object. `isStatusRegisterActive()`, `getStatusFallbackReason()`, `getLastStatus()`.
  `detectFirmware()` re-arms it when it finds the open firmware, as does switching the flag on again
  with `setConfig()`. Other exceptions and bus errors are ordinary read failures.
- `readDiagnostics(d)` - registers 50..77 in one transaction (resistance, age of the last good
  reading, ADC error counters, uptime, reset cause, bus error counters, update slot). Separate from
  the temperature path: no effect on results, counters or bound pointers. Returns false at once
  if `detectFirmware()` found the original firmware.
  The decoding lives in the Arduino-free `ANDRTF3Registers.h` and is tested on the host:
  `pio test -e native`.

Call `detectFirmware()` once at init, before your poller starts: the Modbus layer matches replies
by function code only, so a late reply to the reg-2 read (7) must not meet a pending reg-50 read
(0.7°C). All reads also check the reply length, which catches mix-ups between reads of different
sizes.

- `readIdentity(id)` - the chip's 96-bit factory unique ID plus a user asset
  number and 14-character label (set with `tools/mbident.py` from the firmware repo). Two
  transactions, sent only after `detectFirmware()` found the open firmware; use it to check which physical sensor answers at an address, e.g. after a swap.

```cpp
if (sensor->detectFirmware() && sensor->getFirmware() == ANDRTF3::Firmware::OPEN) {
    auto config = sensor->getConfig();
    config.useStatusRegister = true;
    sensor->setConfig(config);
}
```

The library never retries; one call is one bus transaction (plus one count-1 read, once, when an
exception triggers the fallback).

### Request-style Methods (compatibility)

- `requestTemperature()` - Same as `readTemperature()`: completes the read synchronously
- `isReadComplete()` - Always true
- `getAsyncResult(data)` - Returns the stored result
- `process()` - Drains the async queue, only relevant if you called `enableAsync()`

RS485 is half-duplex, so the bus is occupied for request + response however the call is structured.

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

### Quick Register Reference

| Register | Address (mbpoll) | Function Code | Description | Format |
|----------|------------------|---------------|-------------|--------|
| 50 (PDU) | 51 | 0x04 | Temperature | int16_t × 10 (deci-degrees) |
| 2 (PDU) | 3 | 0x04 | Firmware version | original 7, open firmware 0x8000 \| minor |
| 51-77 (PDU) | 52-78 | 0x04 | Status and diagnostics | open firmware only |
| 78-83 (PDU) | 79-84 | 0x04 | Factory unique ID (96 bit) | open firmware |
| 532-539 (PDU) | 533-540 | 0x03 | Asset number, 14-char label | open firmware |

**Example**: Register value 264 = 26.4°C

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

This library is released under the **GNU General Public License v3.0**. See the [LICENSE](LICENSE) file for the full text.

Copyright (C) 2025-2026 packerlschupfer

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version. It is distributed WITHOUT ANY WARRANTY; without even the implied
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

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
  - Opt-in open firmware support: `detectFirmware()`, `Config::useStatusRegister`,
    `readDiagnostics()` (default: FC04 reg 50, count 1, no init reads)
  - PT1000 Class B sensor, ±0.2K ±1.0% full scale accuracy