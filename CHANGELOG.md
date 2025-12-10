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
- Synchronous and asynchronous temperature reading modes
- QueuedModbusDevice base class for async operations
- IDeviceInstance interface implementation
- Auto-read functionality with configurable intervals
- Statistics tracking (success rate, error counts)
- Multi-sensor support via ANDRTF3Manager class
- Thread-safe operations with mutex protection
- Event group integration for FreeRTOS synchronization
- Temperature compensation offset support
- Direct read fallback on coordinator timeout

Platform: ESP32 (Arduino framework)
Hardware: ANDRTF3/MD wall-mount temperature sensor (Modbus RTU, RS485)
License: GPL-3
Dependencies: ESP32-ModbusDevice, ESP32-IDeviceInstance, MutexGuard

### Notes
- Production-tested for room temperature monitoring (5s read interval)
- Direct read fallback after 30s coordinator timeout
- Previous internal versions (v1.x-v2.x) not publicly released
- Reset to v0.1.0 for clean public release start
