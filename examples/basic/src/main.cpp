/**
 * ANDRTF3 Basic Example
 * 
 * This example demonstrates basic usage of the ANDRTF3 temperature sensor
 * connected via RS485/Modbus RTU.
 * 
 * Hardware connections (ESPlan board):
 * - RX: GPIO36 (RX1)
 * - TX: GPIO4 (TX1)
 * - Auto-direction RS485 transceiver (no RTS needed)
 * 
 * Sensor configuration:
 * - Default address: 3
 * - Baud rate: 9600
 * - Data format: 8N1
 */

#include <Arduino.h>
#include <esp32ModbusRTU.h>
#include <ModbusDevice.h>
#include <ANDRTF3.h>

// Use the andrtf3 namespace
using namespace andrtf3;

// RS485 pins for ESPlan board
#define RS485_RX_PIN 36  // RX1 - GPIO36 (input only)
#define RS485_TX_PIN 4   // TX1 - GPIO4
#define RS485_RTS_PIN -1 // No RTS needed - auto-direction transceiver

// Global objects
esp32ModbusRTU* modbusRTU = nullptr;
ANDRTF3* sensor = nullptr;

// Timing
unsigned long lastReadTime = 0;
const unsigned long READ_INTERVAL = 5000;  // Read every 5 seconds
bool useAsyncMode = false;  // Toggle between sync and async modes

void setup() {
    // Initialize serial
    Serial.begin(115200);
    while (!Serial && millis() < 5000) delay(10);
    
    Serial.println("\n=== ANDRTF3 Basic Example ===");
    Serial.printf("Free heap: %d bytes\n\n", ESP.getFreeHeap());
    
    // Configure RS485 serial
    Serial.println("Configuring RS485...");
    Serial2.begin(9600, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
    
    // Create Modbus RTU instance
    modbusRTU = new esp32ModbusRTU(&Serial2, RS485_RTS_PIN);
    if (!modbusRTU) {
        Serial.println("ERROR: Failed to create Modbus RTU instance!");
        while (1) delay(1000);
    }
    
    // Initialize and configure Modbus
    modbusRTU->begin();
    setGlobalModbusRTU(modbusRTU);
    
    // Register callbacks for ModbusDevice framework
    modbusRTU->onData(mainHandleData);
    modbusRTU->onError([](esp32Modbus::Error error) {
        handleError(0, error);  // RTU doesn't provide address
    });
    
    // Wait for Modbus initialization
    delay(500);
    
    // Create ANDRTF3 sensor instance
    sensor = new ANDRTF3(4);  // Address 3
    if (!sensor) {
        Serial.println("ERROR: Failed to create sensor instance!");
        while (1) delay(1000);
    }
    
    // No explicit initialization needed in simplified version
    
    // Configure sensor
    ANDRTF3::Config config = sensor->getConfig();
    config.timeout = 1000;    // 1 second timeout
    config.retries = 3;       // 3 retries
    sensor->setConfig(config);
    
    // No callbacks in simplified version - we'll poll for data
    
    // Test connection with a simple read
    Serial.println("\nTesting sensor connection...");
    if (sensor->readTemperature()) {
        Serial.println("✓ Sensor connected");
        int16_t temp = sensor->getTemperature();
        Serial.printf("  Temperature: %d.%d°C\n", temp / 10, abs(temp % 10));
    } else {
        Serial.println("✗ Sensor not responding");
        Serial.println("  Check wiring and sensor address");
    }
    
    Serial.println("\nSetup complete. Reading every 5 seconds.");
    Serial.println("Press 's' for status, 'a' to toggle async mode\n");
}

void loop() {
    // Periodic temperature reading
    if (millis() - lastReadTime >= READ_INTERVAL) {
        lastReadTime = millis();
        
        if (useAsyncMode) {
            // Async mode
            if (sensor->requestTemperature()) {
                Serial.printf("[%lu] Async request sent...", millis() / 1000);
                
                // Wait for completion (with timeout)
                uint32_t startTime = millis();
                while (!sensor->isReadComplete() && (millis() - startTime) < 200) {
                    delay(10);
                    sensor->process();  // Process any queued responses
                }
                
                // Get the result
                ANDRTF3::TemperatureData data;
                if (sensor->getAsyncResult(data)) {
                    if (data.valid) {
                        Serial.printf(" %d.%d°C\n", 
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
            // Sync mode
            if (sensor->readTemperature()) {
                auto data = sensor->getTemperatureData();
                Serial.printf("[%lu] Sync: %d.%d°C\n", 
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
        while (Serial.available()) Serial.read();  // Clear buffer
        
        switch (cmd) {
            case 's':
            case 'S': {
                // Show status
                Serial.println("\n--- Status ---");
                Serial.printf("Connected: %s\n", sensor->isConnected() ? "Yes" : "No");
                int16_t temp = sensor->getTemperature();
                Serial.printf("Temperature: %d.%d°C\n", temp / 10, abs(temp % 10));
                Serial.printf("Last update: %lu ms ago\n", 
                             millis() - sensor->getTemperatureData().timestamp);
                break;
            }
            
            case 'a':
            case 'A': {
                // Toggle async mode
                useAsyncMode = !useAsyncMode;
                Serial.printf("\nAsync mode: %s\n", useAsyncMode ? "ON" : "OFF");
                break;
            }
            
            default:
                Serial.println("\nCommands: s=status, a=toggle async");
                break;
        }
    }
    
    delay(10);
}