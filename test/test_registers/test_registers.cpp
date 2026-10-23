/**
 * @file test_registers.cpp
 * @brief Host tests for the open-firmware register decoding (pio test -e native)
 */

#include <unity.h>
#include "ANDRTF3Registers.h"

using namespace andrtf3;

void setUp(void) {}
void tearDown(void) {}

void test_status_plausible_open_firmware_values(void) {
    TEST_ASSERT_TRUE(isPlausibleStatus(STATUS_VALID));
    // Bridged: the reason bit describes the last RAW reading, reg 50 is still the last good value
    TEST_ASSERT_TRUE(isPlausibleStatus(STATUS_VALID | STATUS_HELD | STATUS_TIMEOUT));
    TEST_ASSERT_TRUE(isPlausibleStatus(STATUS_VALID | STATUS_HELD | STATUS_RANGE));
    TEST_ASSERT_TRUE(isPlausibleStatus(STATUS_RANGE));
    TEST_ASSERT_TRUE(isPlausibleStatus(STATUS_NEVER));
    TEST_ASSERT_TRUE(isPlausibleStatus(STATUS_NEVER | STATUS_TIMEOUT));
}

void test_status_implausible_values(void) {
    TEST_ASSERT_FALSE(isPlausibleStatus(0x0000));    // original firmware answers reg 51 with 0
    TEST_ASSERT_FALSE(isPlausibleStatus(0xFFFF));
    TEST_ASSERT_FALSE(isPlausibleStatus(0x0040));    // only an unknown bit: no valid, no reason
    TEST_ASSERT_FALSE(isPlausibleStatus(STATUS_VALID | STATUS_NEVER));
    TEST_ASSERT_FALSE(isPlausibleStatus(STATUS_HELD | STATUS_RANGE));
    TEST_ASSERT_FALSE(isPlausibleStatus(STATUS_HELD));
}

void test_status_unknown_high_bits_ignored(void) {
    // A later firmware may define bits above HELD; that must not switch the library off
    TEST_ASSERT_TRUE(isPlausibleStatus(STATUS_VALID | 0x0040));
    TEST_ASSERT_TRUE(isPlausibleStatus(STATUS_RANGE | 0x8000));
    TEST_ASSERT_FALSE(isPlausibleStatus(STATUS_HELD | 0x0100));    // known bits still checked
}

void test_status_plausible_is_constexpr(void) {
    static_assert(isPlausibleStatus(STATUS_VALID), "compile-time use");
    static_assert(!isPlausibleStatus(0), "compile-time use");
    TEST_PASS();
}

// FC04 50..77 from the bench sensor (slot B) on 2026-10-24
static const uint16_t BENCH[DIAG_COUNT] = {
    221, 1, 10859, 0, 10149, 0, 0, 2206, 0, 10154, 0, 0, 0, 0, 20,
    1027, 1027, 0, 0, 0, 0, 0, 0, 10, 1, 65281, 0, 1};

void test_decode_bench_dump(void) {
    Diagnostics d{};
    TEST_ASSERT_TRUE(decodeDiagnostics(BENCH, DIAG_COUNT, d));
    TEST_ASSERT_EQUAL_INT16(221, d.temperature);
    TEST_ASSERT_EQUAL_HEX16(STATUS_VALID, d.status);
    TEST_ASSERT_EQUAL_UINT16(10859, d.resistance);        // 1085.9 Ohm = PT1000 at ~22 C
    TEST_ASSERT_EQUAL_UINT16(0, d.secondsSinceGood);
    TEST_ASSERT_EQUAL_UINT16(10149, d.goodReadings);
    TEST_ASSERT_EQUAL_UINT16(0, d.failedReadings);
    TEST_ASSERT_EQUAL_INT16(2206, d.temperatureCenti);
    TEST_ASSERT_EQUAL_UINT32(10154, d.uptimeSeconds);
    TEST_ASSERT_EQUAL_HEX16(0x14, d.resetCause);           // soft | pin
    TEST_ASSERT_EQUAL_UINT16(1027, d.busFrames);
    TEST_ASSERT_EQUAL_UINT16(1027, d.busHandled);
    TEST_ASSERT_EQUAL_UINT16(10, d.busExceptions);
    TEST_ASSERT_EQUAL_UINT16(1, d.runningSlot);
    TEST_ASSERT_EQUAL_HEX16(0xFF01, d.bootRecord);         // active B, no trial
    TEST_ASSERT_EQUAL_UINT16(0, d.trialAttempts);
    TEST_ASSERT_EQUAL_HEX16(0x0001, d.bootloaderVersion);
}

void test_decode_field_positions(void) {
    // Every register carries its own address: a shifted index shows up at once
    uint16_t r[DIAG_COUNT];
    for (size_t i = 0; i < DIAG_COUNT; i++) {
        r[i] = static_cast<uint16_t>(DIAG_FIRST + i);
    }
    Diagnostics d{};
    TEST_ASSERT_TRUE(decodeDiagnostics(r, DIAG_COUNT, d));
    TEST_ASSERT_EQUAL(50, d.temperature);
    TEST_ASSERT_EQUAL(51, d.status);
    TEST_ASSERT_EQUAL(52, d.resistance);
    TEST_ASSERT_EQUAL(53, d.secondsSinceGood);
    TEST_ASSERT_EQUAL(54, d.goodReadings);
    TEST_ASSERT_EQUAL(55, d.failedReadings);
    TEST_ASSERT_EQUAL(56, d.lastFailReason);
    TEST_ASSERT_EQUAL(57, d.temperatureCenti);
    TEST_ASSERT_EQUAL_UINT32((58UL << 16) | 59UL, d.uptimeSeconds);   // 58 high, 59 low
    TEST_ASSERT_EQUAL(60, d.adcTimeouts);
    TEST_ASSERT_EQUAL(61, d.rangeErrors);
    TEST_ASSERT_EQUAL(62, d.adcOverflows);
    TEST_ASSERT_EQUAL(63, d.settingsFlags);
    TEST_ASSERT_EQUAL(64, d.resetCause);
    TEST_ASSERT_EQUAL(65, d.busFrames);
    TEST_ASSERT_EQUAL(66, d.busHandled);
    TEST_ASSERT_EQUAL(67, d.busCrcErrors);
    TEST_ASSERT_EQUAL(68, d.busBadLength);
    TEST_ASSERT_EQUAL(69, d.busFramingErrors);
    TEST_ASSERT_EQUAL(70, d.busParityErrors);
    TEST_ASSERT_EQUAL(71, d.busNoiseErrors);
    TEST_ASSERT_EQUAL(72, d.busOverruns);
    TEST_ASSERT_EQUAL(73, d.busExceptions);
    TEST_ASSERT_EQUAL(74, d.runningSlot);
    TEST_ASSERT_EQUAL(75, d.bootRecord);
    TEST_ASSERT_EQUAL(76, d.trialAttempts);
    TEST_ASSERT_EQUAL(77, d.bootloaderVersion);
}

void test_decode_rejects_short_or_null(void) {
    Diagnostics d{};
    TEST_ASSERT_FALSE(decodeDiagnostics(BENCH, DIAG_COUNT - 1, d));
    TEST_ASSERT_FALSE(decodeDiagnostics(nullptr, DIAG_COUNT, d));
}

void test_decode_negative_temperatures(void) {
    uint16_t r[DIAG_COUNT] = {};
    r[0] = static_cast<uint16_t>(-25);
    r[7] = static_cast<uint16_t>(-250);
    Diagnostics d{};
    TEST_ASSERT_TRUE(decodeDiagnostics(r, DIAG_COUNT, d));
    TEST_ASSERT_EQUAL_INT16(-25, d.temperature);
    TEST_ASSERT_EQUAL_INT16(-250, d.temperatureCenti);
}

// Bench unit 2026-10-24: FC04 78..83 and FC03 532..539 after `mbident.py --asset 1 --label Bench`
void test_decode_identity_bench(void) {
    const uint16_t uid[UID_COUNT] = {0x0034, 0x0021, 0x3034, 0x3038, 0x0147, 0x3331};
    const uint16_t ident[IDENT_COUNT] = {1, 0x4265, 0x6E63, 0x6800, 0, 0, 0, 0};    // "Bench"
    Identity id{};
    TEST_ASSERT_TRUE(decodeIdentity(uid, ident, id));
    TEST_ASSERT_EQUAL_HEX32(0x00340021, id.uid[0]);     // same order as the firmware's banner
    TEST_ASSERT_EQUAL_HEX32(0x30343038, id.uid[1]);
    TEST_ASSERT_EQUAL_HEX32(0x01473331, id.uid[2]);
    TEST_ASSERT_EQUAL_UINT16(1, id.asset);
    TEST_ASSERT_EQUAL_STRING("Bench", id.label);
}

void test_decode_identity_full_and_bad_label(void) {
    const uint16_t uid[UID_COUNT] = {};
    const uint16_t full[IDENT_COUNT] = {7, 0x4142, 0x4344, 0x4546, 0x4748, 0x494A, 0x4B4C, 0x4D4E};
    Identity id{};
    TEST_ASSERT_TRUE(decodeIdentity(uid, full, id));
    TEST_ASSERT_EQUAL_STRING("ABCDEFGHIJKLMN", id.label);              // all 14, terminated
    const uint16_t bad[IDENT_COUNT] = {0, 0x410A, 0x4242, 0, 0, 0, 0, 0}; // 'A', LF
    TEST_ASSERT_TRUE(decodeIdentity(uid, bad, id));
    TEST_ASSERT_EQUAL_STRING("A", id.label);
    TEST_ASSERT_FALSE(decodeIdentity(nullptr, bad, id));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_status_plausible_open_firmware_values);
    RUN_TEST(test_status_implausible_values);
    RUN_TEST(test_status_unknown_high_bits_ignored);
    RUN_TEST(test_status_plausible_is_constexpr);
    RUN_TEST(test_decode_bench_dump);
    RUN_TEST(test_decode_field_positions);
    RUN_TEST(test_decode_rejects_short_or_null);
    RUN_TEST(test_decode_negative_temperatures);
    RUN_TEST(test_decode_identity_bench);
    RUN_TEST(test_decode_identity_full_and_bad_label);
    return UNITY_END();
}
