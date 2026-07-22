# ESP32-ANDRTF3 Usage Guide

## Quick Start

### Basic Setup with Simple Pointer Binding

```cpp
#include <ANDRTF3.h>

// Your application temperature data (Temperature_t = int16_t tenths of degrees)
int16_t insideTemp = 0;       // 245 = 24.5°C
bool isTempValid = false;

void setup() {
    // 1. Create ANDRTF3 instance (Modbus address 3)
    andrtf3::ANDRTF3 sensor(3);

    // 2. Bind temperature pointers (library writes directly)
    sensor.bindTemperaturePointers(&insideTemp, &isTempValid);

    Serial.println("ANDRTF3 ready!");
}

void loop() {
    // Request temperature reading
    sensor.requestTemperature();

    // Temperature automatically updated via bound pointer!
    if (isTempValid) {
        Serial.printf("Inside Temperature: %d.%d°C\n",
                     insideTemp / 10,
                     abs(insideTemp % 10));
    } else {
        Serial.println("Temperature invalid");
    }

    delay(5000);  // Read every 5 seconds
}
```

## Advanced Usage

### Without Pointer Binding (Traditional)

```cpp
andrtf3::ANDRTF3 sensor(3);

// Read temperature synchronously
if (sensor.readTemperature()) {
    int16_t temp = sensor.getTemperature();
    Serial.printf("Temp: %d.%d°C\n", temp / 10, abs(temp % 10));
}

// Or async
sensor.requestTemperature();
// ... do other work ...
if (sensor.isReadComplete()) {
    andrtf3::ANDRTF3::TemperatureData data;
    sensor.getAsyncResult(data);
    Serial.printf("Temp: %d.%d°C\n", data.celsius / 10, abs(data.celsius % 10));
}
```

## Temperature Format

### Temperature_t (int16_t)
- **Format**: Tenths of degrees Celsius
- **Range**: -3276.8°C to +3276.7°C
- **ANDRTF3 Range**: -40.0°C to +125.0°C
- **Examples**:
  - `245` = 24.5°C
  - `-150` = -15.0°C
  - `0` = 0.0°C

### Display Helper
```cpp
void printTemp(int16_t temp) {
    Serial.printf("%d.%d°C\n", temp / 10, abs(temp % 10));
}
```

## Features

### Simple & Lightweight
- No IDeviceInstance interface overhead
- Just 2 pointers (8 bytes RAM)
- Perfect for single-sensor applications

### QueuedModbusDevice Base
- Async operation support
- Automatic queue management
- Non-blocking reads

## Configuration

```cpp
andrtf3::ANDRTF3::Config config;
config.address = 3;
config.timeout = 1000;  // ms
config.retries = 3;
sensor.setConfig(config);
```

## Modbus Details

- **Function Code**: 0x04 (Read Input Registers)
- **Register**: 50 (0x0032)
- **Data Format**: Signed 16-bit, deci-degrees Celsius
- **Default Settings**: 9600 baud, 8E1
