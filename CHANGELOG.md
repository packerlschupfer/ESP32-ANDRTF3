# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2025-12-04

### Added
- Initial public release
- ANDRTF3/MD wall-mount temperature sensor driver via Modbus RTU
- Temperature reading from register 50 (0x0032)
- Range: -40°C to +125°C with 0.1°C resolution
- Blocking reads (`readTemperature()`), plus request-style compatibility API
  (`requestTemperature()` / `isReadComplete()` / `getAsyncResult()`)
- Failure classification via `ReadError` (`getLastReadError()`,
  `TemperatureData::errorKind`): bus busy, timeout, CRC, device exception,
  bad response, sensor fault (0x0000/0xFFFF), out of range
- `getLastGoodReadAgeMs()` - age of the last successful (fresh) measurement
- `getReadErrorCount(ReadError)` - cumulative per-kind outcome counters
- Pointer binding (`bindTemperaturePointers`) updated on every read path
- ModbusErrorTracker integration
- Opt-in support for the [open ANDRTF3 firmware](https://github.com/packerlschupfer/ANDRTF3-firmware);
  without these calls the library only reads register 50 (FC04, count 1) and sends nothing else:
  - `detectFirmware()` / `getFirmware()` / `getFirmwareVersion()`: one read of input register 2
    (original firmware: 7, open firmware: bit 15 set)
  - `Config::useStatusRegister` (default false): read 50..51 in one transaction and let status
    bit 0 decide validity. A genuine 0.0°C is valid in this mode. Values bridged by the sensor's
    filter (bit 5, at most 2 readings) are valid and advance `timestamp`; `getLastGoodReadAgeMs()`
    keeps the age of the last fresh measurement.
  - Fallback to the count-1 read, for the life of the object, on exception 02 or a status
    word the open firmware never reports; `isStatusRegisterActive()`, `getStatusFallbackReason()`,
    `statusFallbackToString()`, `getLastStatus()`, `isPlausibleStatus()`, `STATUS_*` constants.
    `detectFirmware()` (open firmware found) or `setConfig()` (flag switched on) re-arms the mode.
  - `readDiagnostics(Diagnostics&)`: input registers 50..77,
    independent of the temperature result and counters
  - `ANDRTF3Registers.h`: Arduino-free status / diagnostics decoding, host tests in
    `test/test_registers` (`pio test -e native`, run by CI; includes a real bench dump)
  - `readIdentity(Identity&)`: factory unique ID (input 78..83) plus asset number and label
    (holding 532..539) (needs `detectFirmware()`, no bus traffic otherwise); `decodeIdentity()` in `ANDRTF3Registers.h`
  - Basic example: firmware detection at start-up, `d` diagnostics, `i` identity
- QueuedModbusDevice base class

Platform: ESP32 (Arduino framework)
Hardware: ANDRTF3/MD wall-mount temperature sensor (Modbus RTU, RS485)
License: GPL-3
Dependencies: ESP32-ModbusDevice

### Notes
- Production-tested for room temperature monitoring (5s read interval)
- Sensor factory default is 8E1 (even parity)
- A genuine 0.0°C reading is reported as SENSOR_FAULT (0x0000 is the fault value)
- Previous internal versions (v1.x-v2.x) not publicly released
- Reset to v0.1.0 for clean public release start
