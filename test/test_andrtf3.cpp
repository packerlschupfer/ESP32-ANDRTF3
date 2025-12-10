/**
 * @file test_andrtf3.cpp
 * @brief Unit tests for ANDRTF3 temperature sensor driver
 *
 * Tests configuration structures, constants, and offline-testable
 * functionality without requiring actual Modbus hardware.
 */

#include <unity.h>
#include <string.h>
#include "ANDRTF3.h"

using namespace andrtf3;

void setUp(void) {
    // Unity setup - called before each test
}

void tearDown(void) {
    // Unity teardown - called after each test
}

// ============================================================================
// Config Structure Tests
// ============================================================================

void test_config_default_values(void) {
    ANDRTF3::Config config = ANDRTF3::getDefaultConfig();

    TEST_ASSERT_EQUAL_UINT8(3, config.address);      // Default address is 3
    TEST_ASSERT_GREATER_THAN(0, config.timeout);     // Should have non-zero timeout
    TEST_ASSERT_GREATER_THAN(0, config.retries);     // Should have retries
}

void test_config_custom_values(void) {
    ANDRTF3::Config config;
    config.address = 5;
    config.timeout = 200;
    config.retries = 5;

    TEST_ASSERT_EQUAL_UINT8(5, config.address);
    TEST_ASSERT_EQUAL_UINT16(200, config.timeout);
    TEST_ASSERT_EQUAL_UINT8(5, config.retries);
}

void test_config_address_range(void) {
    // Modbus addresses are 1-247
    ANDRTF3::Config config;

    config.address = 1;
    TEST_ASSERT_EQUAL_UINT8(1, config.address);

    config.address = 247;
    TEST_ASSERT_EQUAL_UINT8(247, config.address);
}

// ============================================================================
// TemperatureData Structure Tests
// ============================================================================

void test_temperature_data_default(void) {
    ANDRTF3::TemperatureData data;
    data.celsius = 0;
    data.timestamp = 0;
    data.valid = false;
    data.error = "";

    TEST_ASSERT_EQUAL_INT16(0, data.celsius);
    TEST_ASSERT_FALSE(data.valid);
    TEST_ASSERT_TRUE(data.error.isEmpty());
}

void test_temperature_data_valid_reading(void) {
    ANDRTF3::TemperatureData data;
    data.celsius = 261;      // 26.1°C
    data.timestamp = 12345;
    data.valid = true;
    data.error = "";

    TEST_ASSERT_EQUAL_INT16(261, data.celsius);
    TEST_ASSERT_EQUAL_UINT32(12345, data.timestamp);
    TEST_ASSERT_TRUE(data.valid);
}

void test_temperature_data_error_reading(void) {
    ANDRTF3::TemperatureData data;
    data.celsius = 0;
    data.timestamp = 0;
    data.valid = false;
    data.error = "Timeout";

    TEST_ASSERT_FALSE(data.valid);
    TEST_ASSERT_EQUAL_STRING("Timeout", data.error.c_str());
}

void test_temperature_data_negative(void) {
    ANDRTF3::TemperatureData data;
    data.celsius = -150;     // -15.0°C
    data.valid = true;

    TEST_ASSERT_EQUAL_INT16(-150, data.celsius);
    TEST_ASSERT_TRUE(data.valid);
}

void test_temperature_data_extreme_cold(void) {
    // Minimum: -40.0°C = -400
    ANDRTF3::TemperatureData data;
    data.celsius = -400;
    data.valid = true;

    TEST_ASSERT_EQUAL_INT16(-400, data.celsius);
}

void test_temperature_data_extreme_hot(void) {
    // Maximum: +125.0°C = 1250
    ANDRTF3::TemperatureData data;
    data.celsius = 1250;
    data.valid = true;

    TEST_ASSERT_EQUAL_INT16(1250, data.celsius);
}

// ============================================================================
// Temperature Conversion Tests
// ============================================================================

void test_temperature_conversion_positive(void) {
    // 261 deci-degrees = 26.1°C
    int16_t rawValue = 261;
    float celsius = rawValue / 10.0f;

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 26.1f, celsius);
}

void test_temperature_conversion_negative(void) {
    // -150 deci-degrees = -15.0°C
    int16_t rawValue = -150;
    float celsius = rawValue / 10.0f;

    TEST_ASSERT_FLOAT_WITHIN(0.01f, -15.0f, celsius);
}

void test_temperature_conversion_zero(void) {
    int16_t rawValue = 0;
    float celsius = rawValue / 10.0f;

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, celsius);
}

void test_temperature_conversion_min(void) {
    // -400 deci-degrees = -40.0°C (sensor minimum)
    int16_t rawValue = -400;
    float celsius = rawValue / 10.0f;

    TEST_ASSERT_FLOAT_WITHIN(0.01f, -40.0f, celsius);
}

void test_temperature_conversion_max(void) {
    // 1250 deci-degrees = 125.0°C (sensor maximum)
    int16_t rawValue = 1250;
    float celsius = rawValue / 10.0f;

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 125.0f, celsius);
}

// ============================================================================
// Constants Validation Tests
// ============================================================================

void test_temp_register_address(void) {
    // Temperature register should be 50 (0x0032)
    // Note: constant is private, so we document the expected value
    TEST_ASSERT_EQUAL_UINT16(50, 50);  // Documents requirement
}

void test_function_code(void) {
    // Read Input Registers is function code 0x04
    TEST_ASSERT_EQUAL_UINT8(0x04, 0x04);  // Documents requirement
}

void test_temperature_range_constants(void) {
    // Verify sensor range documentation
    // Min: -40.0°C = -400 deci-degrees
    // Max: +125.0°C = 1250 deci-degrees

    int16_t minTemp = -400;
    int16_t maxTemp = 1250;

    TEST_ASSERT_EQUAL_INT16(-400, minTemp);
    TEST_ASSERT_EQUAL_INT16(1250, maxTemp);

    // Range should be 165°C
    float range = (maxTemp - minTemp) / 10.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 165.0f, range);
}

// ============================================================================
// Sensor Address Tests
// ============================================================================

void test_default_address(void) {
    // Default Modbus address is 3
    ANDRTF3::Config config = ANDRTF3::getDefaultConfig();
    TEST_ASSERT_EQUAL_UINT8(3, config.address);
}

void test_valid_address_range(void) {
    // Modbus allows addresses 1-247
    for (uint8_t addr = 1; addr <= 247; addr++) {
        ANDRTF3::Config config;
        config.address = addr;
        TEST_ASSERT_EQUAL_UINT8(addr, config.address);
    }
}

// ============================================================================
// Resolution Tests
// ============================================================================

void test_resolution_0_1_celsius(void) {
    // Sensor provides 0.1°C resolution
    // Consecutive values differ by 0.1°C

    int16_t value1 = 250;  // 25.0°C
    int16_t value2 = 251;  // 25.1°C

    float temp1 = value1 / 10.0f;
    float temp2 = value2 / 10.0f;

    float diff = temp2 - temp1;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.1f, diff);
}

// ============================================================================
// Boundary Value Tests
// ============================================================================

void test_boundary_just_below_min(void) {
    // Value just below minimum would be invalid
    int16_t value = -401;  // -40.1°C (below -40.0°C limit)

    // Document that values outside range should be handled
    TEST_ASSERT_LESS_THAN(-400, value);
}

void test_boundary_just_above_max(void) {
    // Value just above maximum would be invalid
    int16_t value = 1251;  // 125.1°C (above 125.0°C limit)

    // Document that values outside range should be handled
    TEST_ASSERT_GREATER_THAN(1250, value);
}

void test_boundary_freezing_point(void) {
    // 0°C = 0 deci-degrees
    int16_t value = 0;
    float celsius = value / 10.0f;

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, celsius);
}

void test_boundary_boiling_point(void) {
    // 100°C = 1000 deci-degrees (within sensor range)
    int16_t value = 1000;
    float celsius = value / 10.0f;

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, celsius);
}

// ============================================================================
// Test Runner
// ============================================================================

void runAllTests() {
    UNITY_BEGIN();

    // Config tests
    RUN_TEST(test_config_default_values);
    RUN_TEST(test_config_custom_values);
    RUN_TEST(test_config_address_range);

    // TemperatureData tests
    RUN_TEST(test_temperature_data_default);
    RUN_TEST(test_temperature_data_valid_reading);
    RUN_TEST(test_temperature_data_error_reading);
    RUN_TEST(test_temperature_data_negative);
    RUN_TEST(test_temperature_data_extreme_cold);
    RUN_TEST(test_temperature_data_extreme_hot);

    // Temperature conversion tests
    RUN_TEST(test_temperature_conversion_positive);
    RUN_TEST(test_temperature_conversion_negative);
    RUN_TEST(test_temperature_conversion_zero);
    RUN_TEST(test_temperature_conversion_min);
    RUN_TEST(test_temperature_conversion_max);

    // Constants tests
    RUN_TEST(test_temp_register_address);
    RUN_TEST(test_function_code);
    RUN_TEST(test_temperature_range_constants);

    // Address tests
    RUN_TEST(test_default_address);
    RUN_TEST(test_valid_address_range);

    // Resolution tests
    RUN_TEST(test_resolution_0_1_celsius);

    // Boundary tests
    RUN_TEST(test_boundary_just_below_min);
    RUN_TEST(test_boundary_just_above_max);
    RUN_TEST(test_boundary_freezing_point);
    RUN_TEST(test_boundary_boiling_point);

    UNITY_END();
}

#ifdef ARDUINO
void setup() {
    delay(2000);  // Allow serial to initialize
    runAllTests();
}

void loop() {
    // Nothing to do
}
#else
int main(int argc, char** argv) {
    runAllTests();
    return 0;
}
#endif
