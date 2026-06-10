/**
 * ANDRTF3 Basic Example
 *
 * Demonstrates basic usage of the ANDRTF3/MD wall-mount RS485 temperature
 * sensor (Andivi) over Modbus RTU on an ESP32.
 *
 * Hardware:
 * - ESP32 development board
 * - RS485 transceiver (e.g. MAX485) wired to a UART
 * - ANDRTF3/MD sensor (default address 3, 9600 baud, 8N1)
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
#define RS485_CONFIG     SERIAL_8N1

#define SENSOR_ADDRESS   3      // ANDRTF3 default Modbus address

const unsigned long READ_INTERVAL = 5000;  // Read every 5 seconds

// =============================================================================
// Global Objects
// =============================================================================

esp32ModbusRTU modbusMaster(&Serial1);
ANDRTF3* sensor = nullptr;

unsigned long lastReadTime = 0;
bool useAsyncMode = false;

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
    modbusMaster.onError([](esp32Modbus::Error error) {
        handleError(0xFF, error);
    });

    modbusMaster.begin(1);  // run Modbus task on core 1
    Serial.println("Modbus RTU initialized (9600 baud, 8N1)");

    // Create the ANDRTF3 sensor instance (registers itself with the framework)
    sensor = new ANDRTF3(SENSOR_ADDRESS);
    if (!sensor) {
        Serial.println("ERROR: Failed to create sensor instance!");
        while (1) delay(1000);
    }

    // Tweak timeout / retries
    ANDRTF3::Config config = sensor->getConfig();
    config.timeout = 1000;   // 1 second timeout
    config.retries = 3;      // 3 retries
    sensor->setConfig(config);

    // Test connection with a synchronous read
    Serial.println("\nTesting sensor connection...");
    if (sensor->readTemperature()) {
        Serial.println("Sensor connected");
        int16_t temp = sensor->getTemperature();
        Serial.printf("  Temperature: %d.%d C\n", temp / 10, abs(temp % 10));
    } else {
        Serial.println("Sensor not responding (check wiring and address)");
    }

    Serial.println("\nSetup complete. Reading every 5 seconds.");
    Serial.println("Press 's' for status, 'a' to toggle async mode\n");
}

// =============================================================================
// Main Loop
// =============================================================================

void loop() {
    if (millis() - lastReadTime >= READ_INTERVAL) {
        lastReadTime = millis();

        if (useAsyncMode) {
            if (sensor->requestTemperature()) {
                Serial.printf("[%lu] Async request sent...", millis() / 1000);

                uint32_t startTime = millis();
                while (!sensor->isReadComplete() && (millis() - startTime) < 200) {
                    delay(10);
                    sensor->process();
                }

                ANDRTF3::TemperatureData data;
                if (sensor->getAsyncResult(data)) {
                    if (data.valid) {
                        Serial.printf(" %d.%d C\n",
                                      data.celsius / 10, abs(data.celsius % 10));
                    } else {
                        Serial.printf(" Error: %s\n", data.error.c_str());
                    }
                } else {
                    Serial.println(" Failed to get result");
                }
            } else {
                Serial.printf("[%lu] Async request failed\n", millis() / 1000);
            }
        } else {
            if (sensor->readTemperature()) {
                ANDRTF3::TemperatureData data = sensor->getTemperatureData();
                Serial.printf("[%lu] Sync: %d.%d C\n",
                              millis() / 1000,
                              data.celsius / 10, abs(data.celsius % 10));
            } else {
                Serial.printf("[%lu] Sync read failed\n", millis() / 1000);
            }
        }
    }

    // Process any queued operations
    sensor->process();

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
                Serial.printf("Last update: %lu ms ago\n",
                              millis() - sensor->getTemperatureData().timestamp);
                break;
            }
            case 'a':
            case 'A':
                useAsyncMode = !useAsyncMode;
                Serial.printf("\nAsync mode: %s\n", useAsyncMode ? "ON" : "OFF");
                break;
            default:
                Serial.println("\nCommands: s=status, a=toggle async");
                break;
        }
    }

    delay(10);
}
