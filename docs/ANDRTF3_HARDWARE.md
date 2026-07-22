# ANDRTF3/MD Hardware Documentation

## Overview

The **ANDRTF3/MD** is a Modbus indoor temperature sensor from Andivi designed for measuring temperature in living and office spaces, reception halls, foyers, etc. This document provides complete technical specifications and hardware behavior details.

**Manufacturer**: Andivi d.o.o., Slovenia
**Product Page**: https://www.andivi.com/sensors/modbus-sensors/modbus-temperature-sensors/modbus-indoor-temperature-sensor-surface-mounted/
**Datasheet**: https://www.andivi.com/wp-content/uploads/2024/09/ANDRTF3-MD-Datasheet-%E2%80%93-Andivi.pdf

## Key Features

- **Sensor Type**: PT1000 Class B, DIN EN 60751 (2-wire)
- **Measurement Range**: -40°C to +125°C (from library)
- **Resolution**: 0.1°C (deci-degrees)
- **Accuracy**: ±0.2K ±1.0% full scale (after 60min stabilization)
- **Communication**: Modbus RTU/ASCII via RS485
- **Response Time**: 60-90ms typical
- **Power**: 20-34V AC/DC, 10-20mA consumption

## Physical Specifications

### Dimensions
- **Size**: 87.5 × 87.5 × 30 mm
- **Mounting Height**: 1.5m above floor (recommended)
- **Ventilation**: Bottom-aligned convection holes for air flow

### Enclosure
- **Material**: PA6 plastic, similar to RAL 9010 (white)
- **Protection**: IP30
- **Design**: Modern, plain design for inconspicuous mounting
- **Environment**: Indoor use, humidity resistant

### Installation Notes
- Mount on opposite wall from radiator
- Ensure air convection flows upward through bottom vents
- Avoid direct sunlight/insolation
- Allow 60 minutes for temperature stabilization

## Electrical Connections

### 5-Wire Connection (Screw Terminals)

| Pin | Signal | Description |
|-----|--------|-------------|
| 1 | +UB | Power supply (20-34V AC/DC) |
| 2 | GND | Ground |
| 3 | A | RS485 Data A (non-inverted) |
| 4 | B | RS485 Data B (inverted) |
| 5 | SHD | Shield (connect to earth) |

**Wire Specification**: Max 1.5 mm² for screw clamps

### RS485 Bus Configuration
- **Topology**: Multi-drop bus (up to 247 devices)
- **Termination**: 120Ω between A and B at both bus ends
- **Cable**: Twisted pair, shielded recommended
- **Maximum Length**: Depends on baud rate (typ. 1200m at 9600 baud)

## Modbus Configuration

### DIP Switch S2 - Device Address (1-247)

The device address is configured using an 8-position DIP switch (S2). See datasheet page 2 for complete address table.

**Common Addresses:**
- Address 1: All switches OFF
- Address 3: Switch 1 and 2 ON, rest OFF (library default)
- Address 4: Switch 3 ON, rest OFF (your configuration)

### DIP Switch S1 - Communication Settings

| Parameter | Options | DIP Positions |
|-----------|---------|---------------|
| **Baud Rate** | 9600 (default) | S1.1=OFF, S1.2=OFF |
| | 19200 | S1.1=OFF, S1.2=ON |
| | 38400 | S1.1=ON, S1.2=OFF |
| | 57600 | S1.1=ON, S1.2=ON |
| **Parity** | Even (default) | S1.4=OFF, S1.5=OFF |
| | Odd | S1.4=OFF, S1.5=ON |
| | None | S1.4=ON, S1.5=OFF or ON |
| **Protocol** | RTU (default) | S1.6=OFF |
| | ASCII | S1.6=ON |
| **Stop Bits** | 1 (default) | S1.7=OFF |
| | 2 | S1.7=ON |
| **Termination** | None (default) | S1.8=OFF |
| | 120Ω | S1.8=ON |

**Library Default Configuration:**
- Baud Rate: 9600
- Parity: None (library uses 8N1)
- Protocol: RTU
- Stop Bits: 1
- Termination: Configure per installation requirements

## Modbus Register Map

The ANDRTF3/MD is part of Andivi's multi-sensor platform. Different variants support different measurands. The ANDRTF3/MD specifically provides **temperature only**.

### Temperature Register (ANDRTF3/MD)

| Register | Address (mbpoll) | Function Code | Data Type | Gain | Range | Description |
|----------|------------------|---------------|-----------|------|-------|-------------|
| 50 | 51 | 0x04 (Input) | int16_t | 10 | -400 to 1250 | Temperature in deci-degrees Celsius |

**Value Format:**
- **Gain 10** means values are multiplied by 10 (deci-degrees)
- Example: 264 = 26.4°C, -45 = -4.5°C
- Range: -400 (-40.0°C) to 1250 (125.0°C)

### Other Platform Registers (Not Available in ANDRTF3/MD)

These registers are available in other Andivi sensor variants but **NOT in ANDRTF3/MD**:

| Register | Measurand | Variant |
|----------|-----------|---------|
| 10 | CO2 (ppm) | CO2 sensors |
| 20-24 | Temperature/Humidity/Dewpoint | Combo sensors |
| 30 | VOC (ppm) | Air quality sensors |
| 40-41 | Differential Pressure (mbar) | Pressure sensors |
| 60-61 | Brightness (LUX) | Light sensors |

**Note**: Only register 50 (temperature) is populated in ANDRTF3/MD.

## Communication Protocol Details

### Function Codes Supported
- **0x03**: Read Holding Registers (not used for temperature)
- **0x04**: Read Input Registers (used for temperature)

### Read Temperature Example (Function Code 0x04)

**Request:**
```
Slave Address: 0x04
Function Code: 0x04 (Read Input Registers)
Start Address: 0x0032 (50 decimal, PDU addressing)
Quantity:      0x0001 (1 register)
CRC:           [calculated]
```

**Response:**
```
Slave Address: 0x04
Function Code: 0x04
Byte Count:    0x02 (2 bytes)
Data:          0x0108 (264 decimal = 26.4°C)
CRC:           [calculated]
```

### Error Codes

If sensor fails or encounters an error, it may return:
- **0x0000**: Sensor error or communication fault (invalid, reject reading)
- **0xFFFF**: Common Modbus error/no response (invalid, reject reading)

The library correctly identifies and rejects these error values.

## Hardware Behavior Characteristics

### Temperature Sensor Characteristics

1. **PT1000 2-Wire Configuration**
   - Class B accuracy: ±0.2K at 0°C
   - Additional ±1.0% of full scale
   - DIN EN 60751 compliant

2. **Response Time**
   - **Initial Stabilization**: 60 minutes after power-on
   - **Reading Time**: 60-90ms per Modbus read (measured)
   - **Update Rate**: Can be polled continuously (no delays needed)

3. **Accuracy Over Temperature Range**
   - Best accuracy near room temperature (20-25°C)
   - Specified accuracy maintained across -40°C to +125°C range

### Modbus Communication Characteristics

From extensive testing (see FINAL_ANALYSIS.md):

1. **Reliability**: 100% success rate with proper timeout
2. **Response Time**: 60-90ms typical (9600 baud)
3. **No Busy States**: Sensor handles back-to-back reads without delays
4. **No Conversion Time**: Can be read continuously at maximum bus speed

### RS485 Bus Timing

When using multiple devices on the bus:

1. **Inter-Frame Delay**: 3.5 character times (~3.6ms at 9600 baud)
2. **Bus Settling**: 50ms recommended between device initializations
3. **Recommended Timeout**: 200ms (accounts for bus overhead)

## Testing with mbpoll

### Read Temperature
```bash
# Using 1-based addressing (mbpoll default)
mbpoll -a 4 -r 51 -t 3 -c 1 -b 9600 -P none /dev/ttyUSB0

# Expected output:
# [51]: 264  (26.4°C)
```

### Scan Registers
```bash
# Scan registers 0-100 (quick scan)
./scan_registers_quick.sh /dev/ttyUSB0 4

# Full scan 0-255
./scan_all_registers.sh /dev/ttyUSB0 4
```

### Stress Test
```bash
# Test 100 reads to verify reliability
./test_andrtf3_timing.sh /dev/ttyUSB0 4

# Expected: 100% success rate
```

## Common Issues and Solutions

### Issue: Timeout Errors (2% failure rate)

**Symptom**: Intermittent timeouts when using library (but mbpoll works 100%)

**Root Cause**: Library overhead (QueuedModbusDevice + FreeRTOS) requires more time than sensor response time alone.

**Solution**: Increase timeout from 100ms to 200ms
```cpp
// In ANDRTF3.cpp line 313
ANDRTF3::Config ANDRTF3::getDefaultConfig() {
    return {
        3,        // address
        200,      // timeout (was 100) ← FIXED
        3         // retries
    };
}
```

### Issue: Boot Initialization Failures

**Symptom**: Device fails to initialize during boot (intermittent)

**Root Cause**: Insufficient RS485 bus settling time between device initializations

**Solution**: Add 50ms delay between device initializations
```cpp
// After initializing previous device (e.g., MB8ART)
vTaskDelay(pdMS_TO_TICKS(50));  // RS485 bus settle time
// Now initialize ANDRTF3
```

### Issue: Reading 0°C (0x0000)

**Symptom**: Temperature reads as 0.0°C

**Root Cause**:
1. Sensor error returning 0x0000 (not actual 0°C)
2. Wrong register address
3. Communication failure

**Solution**:
- Library v1.0.2+ correctly rejects 0x0000 as error
- Verify register 51 (mbpoll) = PDU 50
- Check wiring and power supply

### Issue: No Communication

**Checklist**:
1. Verify device address (DIP switch S2)
2. Check baud rate matches (DIP switch S1.1, S1.2)
3. Verify RS485 wiring (A to A, B to B)
4. Check termination resistors (120Ω at both bus ends)
5. Verify power supply (20-34V, sufficient current)
6. Test with mbpoll to isolate hardware vs. software issue

## Performance Specifications

| Metric | Value | Notes |
|--------|-------|-------|
| Response Time | 60-90ms | Measured with mbpoll at 9600 baud |
| Success Rate | 100% | With 200ms timeout, proper bus settling |
| Max Poll Rate | ~10 Hz | Limited by response time, no sensor delays |
| Accuracy | ±0.2K ±1% FS | After 60min stabilization |
| Resolution | 0.1°C | Deci-degree format |
| Range | -40°C to +125°C | Per PT1000 sensor spec |
| Startup Time | 60 minutes | For full accuracy stabilization |

## Library Configuration Recommendations

### Optimal Configuration

```cpp
ANDRTF3::Config config;
config.address = 4;      // Match hardware DIP switch
config.timeout = 200;    // Adequate for library overhead
config.retries = 3;      // Standard retry count

sensor.setConfig(config);
```

### Multi-Device Bus Configuration

When using ANDRTF3 with other devices (MB8ART, RYN4, etc.):

```cpp
// Initialize devices with settling delays
initMB8ART();
vTaskDelay(pdMS_TO_TICKS(50));  // RS485 bus settle

initANDRTF3();
vTaskDelay(pdMS_TO_TICKS(50));  // RS485 bus settle

initRYN4();
```

### Polling Strategy

For continuous monitoring:

```cpp
// No delays needed - sensor handles continuous polling
while (true) {
    if (sensor.readTemperature()) {
        int16_t temp = sensor.getTemperature();
        // Process temperature (value is in tenths: 264 = 26.4°C)
    }

    // Can poll immediately or add application-level delay
    vTaskDelay(pdMS_TO_TICKS(100));  // Optional: 100ms update rate
}
```

## Compliance and Certifications

- **Sensor Standard**: DIN EN 60751 (PT1000 Class B)
- **EMC**: Must comply with EMC directives
- **Safety**: Installation by qualified personnel only
- **Region**: Designed for EU market

## Mounting and Installation Guidelines

### Location Selection
1. ✅ Mount on wall opposite to radiator
2. ✅ Height: 1.5m above floor
3. ✅ Ensure free air circulation
4. ❌ Avoid direct sunlight/insolation
5. ❌ Avoid proximity to heat sources
6. ❌ Avoid locations with poor air flow

### Mechanical Installation
1. Mount with convection holes at bottom
2. Use included screws and dowels
3. Ensure level mounting for aesthetic appearance
4. Leave access to DIP switches for configuration

### Electrical Installation
1. Power off before connecting
2. Use shielded twisted pair for RS485 (A, B, SHD)
3. Connect shield (pin 5) to earth/ground
4. Add 120Ω termination at both bus ends
5. Verify polarity (A to A, B to B)
6. Apply power (20-34V AC/DC)

### Commissioning
1. Configure device address (DIP S2)
2. Configure communication settings (DIP S1)
3. Power on and wait 5 seconds for boot
4. Test communication with mbpoll
5. Allow 60 minutes for temperature stabilization
6. Verify accuracy with reference thermometer

## Technical Support

**Manufacturer**: Andivi d.o.o.
**Address**: Zagrebska cesta 102, 2000 Maribor, Slovenia, EU
**Phone**: +386 (0) 2 450 31 08
**Email**: info@andivi.com
**Web**: www.andivi.com

**Library Support**: See GitHub issues at repository

## References

- [Datasheet PDF](https://www.andivi.com/wp-content/uploads/2024/09/ANDRTF3-MD-Datasheet-%E2%80%93-Andivi.pdf)
- [Product Page](https://www.andivi.com/sensors/modbus-sensors/modbus-temperature-sensors/modbus-indoor-temperature-sensor-surface-mounted/)
- [ANDRTF3_REGISTERS.md](./ANDRTF3_REGISTERS.md) - Detailed register specifications
- [../FINAL_ANALYSIS.md](../FINAL_ANALYSIS.md) - Timeout issue analysis and fix
- [../TESTING.md](../TESTING.md) - Testing procedures and tools
