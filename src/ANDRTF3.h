#ifndef ANDRTF3_H
#define ANDRTF3_H

#include <Arduino.h>
#include <QueuedModbusDevice.h>
#include <atomic>

// Import specific types from modbus namespace
using modbus::QueuedModbusDevice;
using modbus::ModbusError;
using modbus::ModbusResult;

namespace andrtf3 {

/**
 * ANDRTF3/MD Temperature Sensor Driver
 * 
 * Simple driver for wall-mount RS485 temperature sensor from Andivi
 * - Temperature range: -40°C to +125°C
 * - Resolution: 0.1°C
 * - Modbus RTU communication
 * - Default: 9600 baud, 8N1
 * 
 * Register Map:
 * - 0x0032 (50): Temperature in deci-degrees Celsius
 */
class ANDRTF3 : public QueuedModbusDevice {
public:
    // Configuration structure
    struct Config {
        uint8_t address;           // Modbus address (1-247, default: 3)
        uint16_t timeout;          // Response timeout in ms (default: 100)
        uint8_t retries;           // Number of retries (default: 3)
    };

    // Temperature data (fixed-point format: value * 10)
    struct TemperatureData {
        int16_t celsius;           // Temperature * 10 (261 = 26.1°C)
        uint32_t timestamp;        // millis() when read
        bool valid;
        String error;
    };

    // Constructor/Destructor
    explicit ANDRTF3(uint8_t address = 3);
    virtual ~ANDRTF3() = default;

    // Configuration
    void setConfig(const Config& config) { _config = config; }
    [[nodiscard]] Config getConfig() const noexcept { return _config; }

    // Device identification
    [[nodiscard]] uint8_t getDeviceAddress() const { return getServerAddress(); }

    // Temperature reading (returns temperature * 10)
    [[nodiscard]] bool readTemperature();
    [[nodiscard]] int16_t getTemperature() const noexcept;
    [[nodiscard]] TemperatureData getTemperatureData() const noexcept { return _lastReading; }

    // Async reading
    [[nodiscard]] bool requestTemperature();
    [[nodiscard]] bool isReadComplete() const noexcept;
    [[nodiscard]] bool getAsyncResult(TemperatureData& data);

    // Status
    [[nodiscard]] bool isConnected() const noexcept { return _connected; }

    // Process queued operations
    void process();

    /**
     * @brief Bind temperature data pointers (unified mapping API)
     *
     * This method accepts pointers to application temperature and validity variables.
     * When temperature readings are updated, the library will update these variables directly.
     *
     * @param tempPtr Pointer to int16_t for temperature value (in tenths of degrees Celsius)
     * @param validPtr Pointer to bool for validity flag
     *
     * Example usage:
     * @code
     * int16_t insideTemp = 0;  // tenths of degrees (261 = 26.1°C)
     * bool isTempValid = false;
     * andrtf3->bindTemperaturePointers(&insideTemp, &isTempValid);
     * @endcode
     */
    void bindTemperaturePointers(int16_t* tempPtr, bool* validPtr);

    // Static utility method
    static Config getDefaultConfig();

protected:
    // QueuedModbusDevice interface
    void onAsyncResponse(uint8_t functionCode, uint16_t address,
                        const uint8_t* data, size_t length) override;

private:
    Config _config;
    TemperatureData _lastReading;
    bool _connected;
    std::atomic<bool> _asyncPending{false};
    uint32_t _asyncStartTime;

    // Unified mapping architecture (simple binding)
    int16_t* _temperaturePtr;  // Pointer to tenths of degrees (Temperature_t)
    bool* _validityPtr;

    // Error tracking for smart retry
    uint8_t _consecutive0x0000Errors;
    uint32_t _lastErrorTime;

    // Internal methods
    bool performRead();
    void handleReadComplete(bool success, int16_t value);
    
    // Constants
    static constexpr uint16_t TEMP_REGISTER = 50;      // Temperature register (0-based)
    static constexpr uint8_t FUNCTION_CODE = 0x04;     // Read Input Registers
    static constexpr uint16_t REGISTER_COUNT = 1;      // Single register
    static constexpr int16_t TEMP_MIN = -400;          // -40.0°C
    static constexpr int16_t TEMP_MAX = 1250;          // +125.0°C
};

} // namespace andrtf3

#endif // ANDRTF3_H