# ANDRTF3 Hardware Documentation

This directory contains complete hardware and register documentation for the ANDRTF3/MD temperature sensor from Andivi.

## Documents

### [ANDRTF3_HARDWARE.md](ANDRTF3_HARDWARE.md)
**Complete hardware specification and behavior documentation**

This is the primary hardware reference document containing:
- Physical specifications (dimensions, enclosure, mounting)
- Electrical specifications (power supply, RS485 pinout, wiring)
- Sensor specifications (PT1000 details, accuracy, range)
- Modbus configuration (DIP switch settings for address and communication)
- Communication protocol details (RTU/ASCII, function codes)
- Response timing characteristics (measured values)
- Hardware behavior notes (what to expect during operation)
- Installation guidelines (where and how to mount)
- Troubleshooting guide (common issues and solutions)
- Performance specifications (accuracy, response time)
- Testing examples (mbpoll commands)

**Start here** if you need to:
- Install the hardware
- Configure DIP switches
- Understand communication settings
- Troubleshoot hardware issues
- Verify electrical connections

### [ANDRTF3_REGISTERS.md](ANDRTF3_REGISTERS.md)
**Detailed Modbus register specifications**

This is the technical reference for all register operations containing:
- Complete register map (with both PDU and mbpoll addressing)
- Register 50 detailed specification (temperature)
- Value format and conversion formulas
- Error value identification and handling
- Reading examples (C++, Python, mbpoll)
- Response timing analysis
- Modbus frame structure examples
- Best practices for register access
- Future enhancement notes

**Start here** if you need to:
- Read temperature values
- Understand register addressing
- Write Modbus code
- Handle error values
- Optimize communication

## Quick Reference

### Temperature Register

| Property | Value |
|----------|-------|
| **PDU Address** | 50 |
| **mbpoll Address** | 51 |
| **Function Code** | 0x04 (Read Input Registers) |
| **Data Type** | Signed 16-bit integer |
| **Format** | Deci-degrees Celsius (value × 10) |
| **Range** | -400 to 1250 (-40.0°C to +125.0°C) |
| **Example** | 264 = 26.4°C |

### Read Temperature with mbpoll

```bash
mbpoll -a 4 -r 51 -t 3 -c 1 -b 9600 -P none /dev/ttyUSB0
```

Output: `[51]: 264` (26.4°C)

### Read Temperature with Library

```cpp
#include <ANDRTF3.h>
using namespace andrtf3;

ANDRTF3 sensor(4);  // Device address 4

if (sensor.readTemperature()) {
    int16_t temp = sensor.getTemperature();  // 264 = 26.4°C
    Serial.printf("Temperature: %d.%d°C\n", temp / 10, abs(temp % 10));
}
```

## Hardware Specs Summary

| Specification | Value |
|---------------|-------|
| **Sensor** | PT1000 Class B, 2-wire |
| **Range** | -40°C to +125°C |
| **Accuracy** | ±0.2K ±1.0% full scale |
| **Resolution** | 0.1°C |
| **Response Time** | 60-90ms |
| **Interface** | RS485 Modbus RTU/ASCII |
| **Power** | 20-34V AC/DC, 10-20mA |
| **Dimensions** | 87.5×87.5×30mm |
| **Protection** | IP30 |

## DIP Switch Configuration

### S2 - Device Address (1-247)
8-position DIP switch - see datasheet for complete table

Common addresses:
- Address 3 (default): Switches 1,2 ON
- Address 4: Switch 3 ON

### S1 - Communication Settings

| Parameter | Setting | DIP Positions |
|-----------|---------|---------------|
| Baud Rate | 9600 (default) | S1.1=OFF, S1.2=OFF |
| Parity | None | S1.4=ON, S1.5=OFF |
| Protocol | RTU (default) | S1.6=OFF |
| Termination | 120Ω (if needed) | S1.8=ON |

## Manufacturer Information

**Company**: Andivi d.o.o., Slovenia
**Product Page**: https://www.andivi.com/sensors/modbus-sensors/modbus-temperature-sensors/modbus-indoor-temperature-sensor-surface-mounted/
**Datasheet**: https://www.andivi.com/wp-content/uploads/2024/09/ANDRTF3-MD-Datasheet-%E2%80%93-Andivi.pdf
**Contact**: info@andivi.com
**Phone**: +386 (0) 2 450 31 08

## Related Documentation

- [Main README](../README.md) - Library overview and usage
- [TESTING.md](../TESTING.md) - Testing procedures
- [FINAL_ANALYSIS.md](../FINAL_ANALYSIS.md) - Timeout issue analysis
- [REGISTER_SCANNING_GUIDE.md](../REGISTER_SCANNING_GUIDE.md) - Register scanning tools

## Testing Tools

Register scanning and testing tools are available in the parent directory:

```bash
# Quick register scan
./scan_registers_quick.sh /dev/ttyUSB0 4

# Timing optimization test
./test_andrtf3_timing.sh /dev/ttyUSB0 4

# Full register scan
./scan_all_registers.sh /dev/ttyUSB0 4
```

## Support

For library support, see the main repository.
For hardware support, contact Andivi (info@andivi.com).
