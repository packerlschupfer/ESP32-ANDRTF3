/**
 * ANDRTF3 Basic Example
 *
 * Demonstrates basic usage of the ANDRTF3/MD wall-mount RS485 temperature
 * sensor (Andivi) over Modbus RTU on an ESP32.
 *
 * Hardware:
 * - ESP32 development board
 * - RS485 transceiver (e.g. MAX485) wired to a UART
 * - ANDRTF3/MD sensor (default address 3, 9600 baud, 8E1)
 *
 * Wiring (adjust pins for your board):
 * - RX: GPIO16 (RO of transceiver)
 * - TX: GPIO17 (DI of transceiver)
 */

#include <Arduino.h>
#include <esp32ModbusRTU.h>
#include <ModbusDevice.h>
#include <ANDRTF3.h>

using namespace andrtf3;

// =============================================================================
// Configuration
// =============================================================================

#define RS485_RX_PIN     16     // RO pin of transceiver
#define RS485_TX_PIN     17     // DI pin of transceiver
#define RS485_BAUD       9600
#define RS485_CONFIG     SERIAL_8E1   // ANDRTF3 factory default: even parity

#define SENSOR_ADDRESS   3      // ANDRTF3 default Modbus address

const unsigned long READ_INTERVAL = 5000;  // Read every 5 seconds

// =============================================================================
// Global Objects
// =============================================================================

esp32ModbusRTU modbusMaster(&Serial1);
ANDRTF3* sensor = nullptr;

unsigned long lastReadTime = 0;

// Response routing helpers provided by ModbusDevice.h
extern void mainHandleData(uint8_t serverAddress, esp32Modbus::FunctionCode fc,
                           uint16_t startingAddress, const uint8_t* data, size_t length);
extern void handleError(uint8_t serverAddress, esp32Modbus::Error error);

// =============================================================================
// Setup
// =============================================================================

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000) delay(10);

    Serial.println("\n=== ANDRTF3 Basic Example ===");
    Serial.printf("Free heap: %u bytes\n\n", ESP.getFreeHeap());

    // Initialize Modbus RTU on Serial1
    Serial.println("Configuring RS485 / Modbus RTU...");
    Serial1.begin(RS485_BAUD, RS485_CONFIG, RS485_RX_PIN, RS485_TX_PIN);

    // Route Modbus responses to the ModbusDevice framework
    modbusMaster.onData([](uint8_t serverAddress, esp32Modbus::FunctionCode fc,
                           uint16_t address, const uint8_t* data, size_t length) {
        mainHandleData(serverAddress, fc, address, data, length);
    });
    // Pass the server address: handleError() routes the error to the device by
    // address, otherwise exceptions and CRC errors end as timeouts
    modbusMaster.onError([](uint16_t serverAddress, esp32Modbus::Error error) {
        handleError(static_cast<uint8_t>(serverAddress), error);
    });

    modbusMaster.begin(1);  // run Modbus task on core 1
    Serial.println("Modbus RTU initialized (9600 baud, 8E1)");

    // Create the ANDRTF3 sensor instance (registers itself with the framework)
    sensor = new ANDRTF3(SENSOR_ADDRESS);
    if (!sensor) {
        Serial.println("ERROR: Failed to create sensor instance!");
        while (1) delay(1000);
    }

    // Test connection with a synchronous read
    Serial.println("\nTesting sensor connection...");
    if (sensor->readTemperature()) {
        Serial.println("Sensor connected");
        int16_t temp = sensor->getTemperature();
        Serial.printf("  Temperature: %d.%d C\n", temp / 10, abs(temp % 10));
    } else {
        Serial.println("Sensor not responding (check wiring and address)");
    }

    // Open firmware (github.com/packerlschupfer/ANDRTF3-firmware): let the
    // sensor's status register decide validity. Harmless on the original
    // firmware - the library notices and keeps reading register 50 only.
    if (sensor->detectFirmware() && sensor->getFirmware() == ANDRTF3::Firmware::OPEN) {
        ANDRTF3::Config config = sensor->getConfig();
        config.useStatusRegister = true;
        sensor->setConfig(config);
        Serial.printf("Open firmware 0x%04X: status register on\n", sensor->getFirmwareVersion());
    }

    Serial.println("\nSetup complete. Reading every 5 seconds.");
    Serial.println("Press 's' for status, 'd' for diagnostics, 'i' for identity (open firmware)\n");
}

// =============================================================================
// Main Loop
// =============================================================================

void loop() {
    if (millis() - lastReadTime >= READ_INTERVAL) {
        lastReadTime = millis();

        if (sensor->readTemperature()) {
            int16_t temp = sensor->getTemperature();
            Serial.printf("[%lu] %d.%d C\n", millis() / 1000, temp / 10, abs(temp % 10));
        } else {
            ANDRTF3::TemperatureData data = sensor->getTemperatureData();
            Serial.printf("[%lu] Read failed: %s (%s)\n", millis() / 1000,
                          ANDRTF3::readErrorToString(data.errorKind), data.error.c_str());
        }
    }

    // Handle serial commands
    if (Serial.available()) {
        char cmd = Serial.read();
        while (Serial.available()) Serial.read();

        switch (cmd) {
            case 's':
            case 'S': {
                Serial.println("\n--- Status ---");
                Serial.printf("Connected: %s\n", sensor->isConnected() ? "Yes" : "No");
                int16_t temp = sensor->getTemperature();
                Serial.printf("Temperature: %d.%d C\n", temp / 10, abs(temp % 10));
                uint32_t age = sensor->getLastGoodReadAgeMs();
                if (age == UINT32_MAX) {
                    Serial.println("Last good read: never");
                } else {
                    Serial.printf("Last good read: %lu ms ago\n", (unsigned long)age);
                }
                Serial.printf("Last error: %s\n",
                              ANDRTF3::readErrorToString(sensor->getLastReadError()));
                for (uint8_t k = 0; k < static_cast<uint8_t>(ANDRTF3::ReadError::COUNT); k++) {
                    auto kind = static_cast<ANDRTF3::ReadError>(k);
                    Serial.printf("  %-16s %lu\n", ANDRTF3::readErrorToString(kind),
                                  (unsigned long)sensor->getReadErrorCount(kind));
                }
                Serial.printf("Status register: %s (%s), last 0x%04X\n",
                              sensor->isStatusRegisterActive() ? "active" : "off",
                              ANDRTF3::statusFallbackToString(sensor->getStatusFallbackReason()),
                              sensor->getLastStatus());
                break;
            }
            case 'd':
            case 'D': {
                ANDRTF3::Diagnostics d;
                if (!sensor->readDiagnostics(d)) {
                    Serial.println("\nNo diagnostics (original firmware or no answer)");
                    break;
                }
                Serial.println("\n--- Diagnostics ---");
                Serial.printf("T %s%d.%02d C, R %u.%u Ohm, status 0x%04X, last good %u s ago\n",
                              d.temperatureCenti < 0 ? "-" : "",
                              abs(d.temperatureCenti / 100), abs(d.temperatureCenti % 100),
                              d.resistance / 10, d.resistance % 10, d.status, d.secondsSinceGood);
                Serial.printf("Readings good %u failed %u (timeout %u range %u overflow %u)\n",
                              d.goodReadings, d.failedReadings, d.adcTimeouts, d.rangeErrors,
                              d.adcOverflows);
                Serial.printf("Uptime %lu s, reset cause 0x%02X\n",
                              (unsigned long)d.uptimeSeconds, d.resetCause);
                Serial.printf("Bus frames %u handled %u crc %u badlen %u fe %u pe %u ne %u ore %u exc %u\n",
                              d.busFrames, d.busHandled, d.busCrcErrors, d.busBadLength,
                              d.busFramingErrors, d.busParityErrors, d.busNoiseErrors,
                              d.busOverruns, d.busExceptions);
                Serial.printf("Slot %c, boot record 0x%04X, bootloader 0x%04X\n",
                              d.runningSlot ? 'B' : 'A', d.bootRecord, d.bootloaderVersion);
                break;
            }
            case 'i':
            case 'I': {
                ANDRTF3::Identity id;
                if (!sensor->readIdentity(id)) {
                    Serial.println("\nNo identity (needs the open firmware)");
                    break;
                }
                Serial.printf("\nUID %08lX%08lX%08lX  asset %u  label \"%s\"\n",
                              (unsigned long)id.uid[0], (unsigned long)id.uid[1],
                              (unsigned long)id.uid[2], id.asset, id.label);
                break;
            }
            default:
                Serial.println("\nCommands: s=status, d=diagnostics, i=identity");
                break;
        }
    }

    delay(10);
}
