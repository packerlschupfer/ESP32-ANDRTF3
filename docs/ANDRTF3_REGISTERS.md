# ANDRTF3/MD Register Specifications

## Overview

This document provides detailed specifications for all Modbus registers in the ANDRTF3/MD temperature sensor. The ANDRTF3/MD is part of Andivi's multi-sensor platform, but this specific model **only provides temperature measurement**.

## Addressing Convention

**Important**: Modbus has two addressing schemes:

| Scheme | Register 50 | Used By |
|--------|-------------|---------|
| PDU (Protocol Data Unit) | 50 | Library code, Modbus protocol |
| ADU (Application Data Unit) | 51 | mbpoll, most Modbus tools |

**This document uses both conventions for clarity:**
- **PDU Address**: Used in code (`readInputRegisters(50, 1)`)
- **mbpoll Address**: Used in testing (`mbpoll -r 51`)

## Register Map Summary

### ANDRTF3/MD Available Registers

| PDU Addr | mbpoll | FC | Type | Gain | Range | R/W | Description |
|----------|--------|----|----|------|-------|-----|-------------|
| 50 | 51 | 0x04 | int16_t | 10 | -400 to 1250 | R | Temperature (°C × 10) |

### Platform Registers (Other Andivi Sensor Variants)

These registers are **NOT available** in ANDRTF3/MD but exist in other Andivi sensor models:

| PDU Addr | mbpoll | FC | Measurand | Gain | Variant |
|----------|--------|-----|-----------|------|---------|
| 10 | 11 | 0x04 | CO2 (ppm) | 1 | CO2 sensors |
| 20 | 21 | 0x04 | Temperature (°C) | 10 | Combo sensors |
| 21 | 22 | 0x04 | Humidity (%rH) | 10 | Combo sensors |
| 22 | 23 | 0x04 | Abs. Humidity (kg/m³) | 10 | Combo sensors |
| 23 | 24 | 0x04 | Dewpoint (°C) | 10 | Combo sensors |
| 24 | 25 | 0x04 | Enthalpy (J) | 10 | Combo sensors |
| 30 | 31 | 0x04 | VOC (ppm) | 1 | VOC sensors |
| 40 | 41 | 0x04 | Diff. Pressure (mbar) | 1 | Pressure sensors |
| 41 | 42 | 0x04 | Diff. Pressure (mbar) | 1 | Pressure sensors |
| 60-61 | 62-63 | 0x04 | Brightness (LUX) | 1 | Light sensors |

## Register 50: Temperature

### Specification

| Property | Value |
|----------|-------|
| **PDU Address** | 50 |
| **mbpoll Address** | 51 |
| **Function Code** | 0x04 (Read Input Registers) |
| **Data Type** | Signed 16-bit integer (int16_t) |
| **Byte Order** | Big-endian (Modbus standard) |
| **Unit** | Deci-degrees Celsius (°C × 10) |
| **Gain/Multiplier** | 10 |
| **Range** | -400 to 1250 (-40.0°C to +125.0°C) |
| **Resolution** | 1 unit = 0.1°C |
| **Accuracy** | ±0.2K ±1.0% full scale (after 60min) |
| **Update Rate** | Continuous (no delays needed) |
| **Access** | Read-only |

### Value Format

**Formula**: `Temperature (°C) = Register Value / 10`

**Examples**:
```
Register Value    Hex        Temperature
-400          → 0xFE70  →  -40.0°C
-45           → 0xFFD3  →   -4.5°C
0             → 0x0000  →    0.0°C (ERROR! See note below)
230           → 0x00E6  →   23.0°C
264           → 0x0108  →   26.4°C
1250          → 0x04E2  → +125.0°C
65535 (0xFFFF)           →  ERROR! (See note below)
```

### Special Values and Error Handling

#### Error Value: 0x0000

**IMPORTANT**: The value `0x0000` (0 decimal) is **NOT** a valid temperature reading of 0.0°C.

**Reason**: When the sensor encounters an error or communication fault, it returns `0x0000` instead of a valid reading. True 0.0°C is within the sensor's range, but the probability of exactly 0.0°C in real use is extremely low.

**Library Behavior** (since v1.0.2):
- Values of `0x0000` are **rejected** as sensor errors
- Previous valid temperature is **retained**
- `_connected` flag set to `false`
- Error message: "Sensor returned 0x0000"

**Why This Matters**:
- Heating systems could incorrectly activate if 0.0°C was treated as valid
- Better to use previous valid reading than accept false 0.0°C

#### Error Value: 0xFFFF

**Meaning**: Common Modbus error response or device not responding

**Library Behavior**:
- Values of `0xFFFF` (-1 as signed int16_t) are **rejected**
- Error message: "Modbus error 0xFFFF"
- Connection marked as failed

#### Valid Range Checking

The library validates all readings against the sensor's physical range:

```cpp
if (rawValue < -400 || rawValue > 1250) {
    // Reject as out of range
}
```

### Reading Temperature (C++ Library)

```cpp
#include <ANDRTF3.h>

using namespace andrtf3;

ANDRTF3 sensor(4);  // Device address 4

// Synchronous read
if (sensor.readTemperature()) {
    int16_t temp = sensor.getTemperature();  // Value in tenths (264 = 26.4°C)

    // Convert to float if needed
    float tempC = temp / 10.0f;

    // Or format directly
    Serial.printf("Temperature: %d.%d°C\n", temp / 10, abs(temp % 10));
}

// Async read
sensor.requestTemperature();
// ... do other work ...
ANDRTF3::TemperatureData data;
if (sensor.getAsyncResult(data)) {
    if (data.valid) {
        Serial.printf("Temp: %d.%d°C at %lu ms\n",
                      data.celsius / 10,
                      abs(data.celsius % 10),
                      data.timestamp);
    } else {
        Serial.printf("Error: %s\n", data.error.c_str());
    }
}
```

### Reading Temperature (mbpoll)

```bash
# Read temperature (Input Register)
mbpoll -a 4 -r 51 -t 3 -c 1 -b 9600 -P none /dev/ttyUSB0

# Example output:
# [51]: 264
# Interpretation: 264 / 10 = 26.4°C

# Read in hex format
mbpoll -a 4 -r 51 -t 3:hex -c 1 -b 9600 -P none /dev/ttyUSB0

# Example output:
# [51]: 0x0108
```

### Reading Temperature (Python + pyModbus)

```python
from pymodbus.client import ModbusSerialClient

client = ModbusSerialClient(
    method='rtu',
    port='/dev/ttyUSB0',
    baudrate=9600,
    parity='N',
    stopbits=1,
    bytesize=8,
    timeout=0.2
)

client.connect()

# Read Input Register 50 (PDU addressing)
result = client.read_input_registers(address=50, count=1, slave=4)

if not result.isError():
    raw_value = result.registers[0]

    # Convert to signed int16
    if raw_value > 32767:
        raw_value -= 65536

    # Check for error values
    if raw_value == 0 or raw_value == -1:
        print("Sensor error!")
    else:
        temperature = raw_value / 10.0
        print(f"Temperature: {temperature:.1f}°C")
else:
    print(f"Modbus error: {result}")

client.close()
```

## Response Timing

### Typical Response Times (9600 baud, measured)

| Scenario | Time (ms) | Notes |
|----------|-----------|-------|
| Minimum response | 60 | Best case |
| Average response | 66-88 | Typical |
| Maximum response | 269 | Occasional |
| Recommended timeout | 200 | Accounts for library overhead |

### Library Overhead (ESP32 with FreeRTOS)

When using the ANDRTF3 library with QueuedModbusDevice:

| Component | Overhead (ms) |
|-----------|---------------|
| Queue processing | 5-15 |
| FreeRTOS task switching | 1-5 |
| Semaphore contention | 1-10 |
| Bus arbitration | 5-20 |
| **Total** | **12-50** |

**Implication**: While mbpoll (direct access) needs only 100ms timeout, the library needs 200ms to account for overhead.

## Bus Initialization

### Register for Connection Testing

**Question**: Should we use register 50 for connection testing during initialization?

**Analysis**:
- Register 50 is the only known register in ANDRTF3/MD
- Initial tests showed registers 1, 2, 3, 7, 65, 68 returning values
- Register 4 was **not tested** (mentioned in bus initialization fix)

**Recommendation**: Use register 50 for connection testing:

```cpp
bool ANDRTF3::testConnection() {
    // Quick read of temperature register
    auto result = readInputRegisters(TEMP_REGISTER, 1);

    if (!result.isOk()) {
        return false;
    }

    // Verify we got a response (even if value is out of range)
    auto values = result.value();
    if (values.empty()) {
        return false;
    }

    // Check it's not an error value
    int16_t value = static_cast<int16_t>(values[0]);
    if (value == 0 || value == -1) {
        return false;
    }

    _connected = true;
    return true;
}
```

### Unknown Registers (Needs Investigation)

Your initial test showed these registers had data:

| mbpoll Addr | Value | Possible Purpose |
|-------------|-------|------------------|
| 1 | 0 | Status/Device ID? |
| 2 | 1 | Firmware version? |
| 3 | 7 | Hardware version? |
| 7 | ? | Unknown |
| 65 | 1 | Configuration flag? |
| 68 | 265 | Alternate temperature? |

**Action Required**: Run register scan to identify these:

```bash
./scan_registers_quick.sh /dev/ttyUSB0 4
```

This will help determine:
1. What register 4 contains (for bus init testing)
2. Whether register 68 is an alternate temperature source
3. If there are status/error registers available

## Modbus Frame Examples

### Read Temperature Request (FC 0x04)

```
Field               Bytes       Example
-----------------------------------------
Slave Address       1           0x04 (device 4)
Function Code       1           0x04 (Read Input Registers)
Starting Address    2           0x00 0x32 (50 decimal)
Quantity            2           0x00 0x01 (1 register)
CRC                 2           [calculated]
```

**Complete Frame**: `04 04 00 32 00 01 [CRC]`

### Read Temperature Response (FC 0x04)

```
Field               Bytes       Example
-----------------------------------------
Slave Address       1           0x04 (device 4)
Function Code       1           0x04 (Read Input Registers)
Byte Count          1           0x02 (2 bytes of data)
Data                2           0x01 0x08 (264 decimal = 26.4°C)
CRC                 2           [calculated]
```

**Complete Frame**: `04 04 02 01 08 [CRC]`

### Error Response

```
Field               Bytes       Example
-----------------------------------------
Slave Address       1           0x04
Function Code       1           0x84 (0x04 + 0x80 = error)
Exception Code      1           0x02 (Illegal Data Address)
CRC                 2           [calculated]
```

**Common Exception Codes**:
- `0x01`: Illegal Function
- `0x02`: Illegal Data Address (register doesn't exist)
- `0x03`: Illegal Data Value
- `0x04`: Slave Device Failure

## Register Scan Results

### Expected Results for ANDRTF3/MD

When running the register scanner, you should find:

**Input Registers (FC 0x04)**:
- Register 50: Temperature (200-300 range for room temp)
- Register 51: Unknown (if exists)
- Possibly registers 1-10: Status/config

**Holding Registers (FC 0x03)**:
- Likely none (ANDRTF3/MD is read-only sensor)

### How to Scan

```bash
# Quick scan (0-100)
./scan_registers_quick.sh /dev/ttyUSB0 4

# Full scan (0-255)
./scan_all_registers.sh /dev/ttyUSB0 4

# Test stability
./test_status_registers.sh /dev/ttyUSB0 4
```

## Best Practices

### Reading Temperature

1. **Use appropriate timeout**: 200ms minimum for library, 100ms for direct mbpoll
2. **Validate range**: Check -400 to 1250 range
3. **Reject error values**: 0x0000 and 0xFFFF are not valid temperatures
4. **Preserve last valid**: Don't overwrite with error readings
5. **Wait for stabilization**: 60 minutes after power-on for full accuracy

### Multi-Device Bus

1. **Stagger initialization**: 50ms between device inits
2. **Use proper termination**: 120Ω at both bus ends
3. **Minimize polling**: Poll only when needed (e.g., every 100ms, not continuously)
4. **Handle failures gracefully**: Don't block on one device failure

### Error Handling

```cpp
if (sensor.readTemperature()) {
    auto data = sensor.getTemperatureData();

    if (data.valid) {
        // Use data.celsius (in tenths of degrees)
        processTemperature(data.celsius);
    } else {
        // Handle error
        LOG_ERROR("Temperature read failed: %s", data.error.c_str());

        // Use previous valid reading or default
        processTemperature(lastKnownGood);
    }
} else {
    // Communication failure
    LOG_ERROR("Modbus communication failed");
}
```

## Future Enhancements

### Potential Additional Registers

If Andivi adds registers in future firmware:

- **Device Information**: Model number, serial number, firmware version
- **Status Register**: Error flags, sensor health, calibration status
- **Configuration**: Averaging, filtering, update rate settings
- **Statistics**: Min/max temperature, uptime, read count
- **Calibration**: Offset adjustment, gain correction

### Feature Requests

Submit to Andivi (info@andivi.com) or library maintainer:

1. Status register for quick connection testing
2. Device information registers (model, firmware, serial)
3. Min/max temperature tracking
4. Configurable averaging/filtering
5. Alarm threshold registers

## References

- [ANDRTF3_HARDWARE.md](./ANDRTF3_HARDWARE.md) - Complete hardware documentation
- [Andivi Datasheet](https://www.andivi.com/wp-content/uploads/2024/09/ANDRTF3-MD-Datasheet-%E2%80%93-Andivi.pdf)
- [Modbus Protocol Specification](https://www.modbus.org/specs.php)
- [../TESTING.md](../TESTING.md) - Testing procedures
- [../FINAL_ANALYSIS.md](../FINAL_ANALYSIS.md) - Timeout analysis and fix
