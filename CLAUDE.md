# ANDRTF3 Library - CLAUDE.md

## Overview
Driver for the Andivi ANDRTF3/MD wall-mount RS485 temperature sensor, built on
ESP32-ModbusDevice (`QueuedModbusDevice`). Single register, single value.

## Modbus Details
- Function code 0x04 (Read Input Registers), register 50 (0x0032)
- Signed 16-bit, deci-degrees Celsius (261 = 26.1°C), range -40.0..125.0°C
- Factory default: address 3, 9600 baud, **8E1 (even parity)** - a wrong parity
  setting was the root cause of the intermittent 0x0000 errors (Dec 2025)

## Architecture
```
IModbusDevice
    └── ModbusDevice
            └── QueuedModbusDevice
                    └── ANDRTF3
```
- Every read is one blocking transaction via `readInputRegistersWithPriority()`
  (SENSOR priority), serialized by the ModbusDevice bus mutex.
- `requestTemperature()` / `isReadComplete()` / `getAsyncResult()` are a
  compatibility API: the read completes synchronously inside `requestTemperature()`.
- `onAsyncResponse()` is only reached if the application calls `enableAsync()`.
- All paths funnel through one validation function, `processRegisterValue()`,
  and store results via `recordSuccess()` / `recordFailure()`.

## Open firmware (all opt-in)
Sensors can run the open firmware (https://github.com/packerlschupfer/ANDRTF3-firmware): reg 2 has bit 15 set, regs 51..77
carry status and diagnostics. The default path never touches them.
- `detectFirmware()`: FC04 reg 2, explicit only (no init reads). OPEN re-arms the status mode,
  ORIGINAL switches it off.
- `Config.useStatusRegister`: FC04 50..51 count 2. Status bit0 = valid (also 0.0°C); bit0 clear ->
  SENSOR_FAULT via the same grace counter as 0x0000 (no separate ReadError kind - the boiler pins
  ReadError::COUNT with a static_assert; if ever needed, APPEND kinds after OTHER).
- bit5 (held, firmware bridges at most 2 readings) -> valid AND timestamp advances;
  getLastGoodReadAgeMs() = age of the last FRESH measurement (_lastFreshMs), frozen during a bridge.
  The boiler feeds timestamp into its staleness checks and the age into MQTT stats.
- Fallback to count 1, permanent for the object: exception 02 only on the count-2 read (that reading is
  then taken with count 1 at once), or `isPlausibleStatus()` false (original answers reg 51 = 0;
  that response's reg 50 is judged the default way). Bits above 0x20 are ignored by the check.
- Every read checks the reply length (base layer matches by FC only); detectFirmware() belongs in
  init, before polling starts (a late reg-2 reply of 7 would otherwise read as 0.7°C). Re-armed by detectFirmware() finding OPEN or
  setConfig() switching the flag on. No other retries anywhere.
- `readDiagnostics()`: FC04 50..77; does not touch results/counters/pointers;
  no transaction at all once detectFirmware() found ORIGINAL. Decode = decodeDiagnostics() in
  ANDRTF3Registers.h, tested against a real bench dump.
- `readIdentity()`: FC04 78..83 (UID) + FC03 532..539 (asset, label); exact
  lengths checked; no transaction unless detectFirmware() found OPEN. Decode = decodeIdentity(), tested with bench values.
- Agreed with the boiler project (Boiler controller diagnostics chat) before the installed sensor's
  swap; ping it for review before tagging a release.

## Result semantics (consumers depend on these - announce changes first)
- 0x0000 and 0xFFFF are sensor faults -> invalid (`ReadError::SENSOR_FAULT`);
  a genuine 0.0°C therefore reads as invalid (default mode; the status mode trusts bit 0 instead).
- Out of range -> invalid (`OUT_OF_RANGE`).
- `isConnected()`: false after any failed read, except SENSOR_FAULT keeps the
  current state until the 3rd consecutive one.
- `TemperatureData.celsius` / `.timestamp` keep the last GOOD value on failure;
  `getLastGoodReadAgeMs()` returns UINT32_MAX before the first good read.
- `getReadErrorCount(kind)` counts every read outcome per kind (NONE = success);
  the boiler publishes these to MQTT for post-mortems.
- `ReadError::BUS_BUSY` = bus mutex timeout, nothing was sent (contention, not
  a sensor problem).
- Bound pointers (`bindTemperaturePointers`) are written on every read: raw
  value + valid=true on success, valid=false on failure. No debouncing.
- `Config.timeout` / `Config.retries` are reserved and not applied.

## Consumers
- esp32-boiler-controller (ships env `prod_ota_debug_selective`) (`src/hal/ANDRTF3TemperatureSensor.cpp`,
  `src/modules/tasks/ANDRTF3Task.cpp`). Binds nullptr on purpose; applies its
  own offset, 3-strike debounce and hold-last-value policy on top. Validity feeds
  both a burner-stop fallback and frost protection that starts the burner, so
  any change to validity semantics needs a heads-up to that project first.

## Build
```bash
pio run -d examples/basic
pio test -e native          # src/ANDRTF3Registers.h (pure, no Arduino)
```
CI compiles every example against pioarduino / Arduino core 3.3.x.
