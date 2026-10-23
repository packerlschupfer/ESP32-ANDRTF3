/*
 * ANDRTF3Registers.h - part of the ESP32-ANDRTF3 library
 *
 * Copyright (C) 2025-2026 packerlschupfer
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ANDRTF3_REGISTERS_H
#define ANDRTF3_REGISTERS_H

// Open-firmware register decoding. Pure functions, no Arduino or Modbus
// dependency, so the native test env can check them on the host.
// Register map: https://github.com/packerlschupfer/ANDRTF3-firmware (firmware/include/regs.h)

#include <stddef.h>
#include <stdint.h>

namespace andrtf3 {

// Register 51 bits
constexpr uint16_t STATUS_VALID = 0x0001;     // reg 50 trustworthy (fresh or bridged)
constexpr uint16_t STATUS_TIMEOUT = 0x0002;   // last raw reading: ADC not ready
constexpr uint16_t STATUS_OVERFLOW = 0x0004;  // last raw reading: ADC overflow
constexpr uint16_t STATUS_RANGE = 0x0008;     // last raw reading: R outside 500..3000 Ohm
constexpr uint16_t STATUS_NEVER = 0x0010;     // no good reading since boot
constexpr uint16_t STATUS_HELD = 0x0020;      // reg 50 bridged by the filter (max. 2 readings)

constexpr uint16_t STATUS_KNOWN = STATUS_VALID | STATUS_TIMEOUT | STATUS_OVERFLOW |
                                 STATUS_RANGE | STATUS_NEVER | STATUS_HELD;

/**
 * true if the open firmware can report this reg 51 value.
 *
 * Bits above STATUS_HELD are ignored, so a later firmware may define more
 * without switching the library off. 0 (what the original firmware answers)
 * and impossible combinations of the known bits are rejected.
 *
 * Note VALID|HELD together with a reason bit is plausible: the reason bits
 * describe the last RAW reading, while the bridged value in reg 50 is still
 * the last good one.
 */
constexpr bool isPlausibleStatus(uint16_t raw) {
    return (raw & STATUS_VALID)
               ? !(raw & STATUS_NEVER)                                         // valid needs a reading
               : ((raw & (STATUS_TIMEOUT | STATUS_OVERFLOW | STATUS_RANGE | STATUS_NEVER)) != 0 &&
                  !(raw & STATUS_HELD));       // invalid always has a reason; only valid is bridged
}

// Input registers 50..77 of the open firmware
struct Diagnostics {
    int16_t temperature;          // 50: 0.1°C (0 when not trustworthy)
    uint16_t status;              // 51: STATUS_* bits
    uint16_t resistance;          // 52: PT1000, 0.1 Ohm (0 = not measurable)
    uint16_t secondsSinceGood;    // 53: age of the last good reading (0xFFFF = never / saturated)
    uint16_t goodReadings;        // 54: counter, wraps
    uint16_t failedReadings;      // 55: counter, wraps
    uint16_t lastFailReason;      // 56: 1 ADC timeout, 2 overflow, 3 range
    int16_t temperatureCenti;     // 57: 0.01°C, last good, unfiltered
    uint32_t uptimeSeconds;       // 58 (high) / 59 (low)
    uint16_t adcTimeouts;         // 60
    uint16_t rangeErrors;         // 61
    uint16_t adcOverflows;        // 62
    uint16_t settingsFlags;       // 63: bit0 loaded from EEPROM, bit1 unsaved changes
    uint16_t resetCause;          // 64: RCC_CSR 31..24 (0x20 IWDG, 0x10 soft, 0x0C power-on, 0x04 pin)
    uint16_t busFrames;           // 65: frames seen on the bus (any address)
    uint16_t busHandled;          // 66: frames for this sensor with a good CRC
    uint16_t busCrcErrors;        // 67: includes foreign request+reply pairs seen as one frame, not a line metric
    uint16_t busBadLength;        // 68
    uint16_t busFramingErrors;    // 69
    uint16_t busParityErrors;     // 70
    uint16_t busNoiseErrors;      // 71
    uint16_t busOverruns;         // 72
    uint16_t busExceptions;       // 73: exception responses sent
    uint16_t runningSlot;         // 74: 0 = A, 1 = B
    uint16_t bootRecord;          // 75: active | trial << 8 (0xFF = none), 0xFFFF = no record
    uint16_t trialAttempts;       // 76
    uint16_t bootloaderVersion;   // 77: 0 = no bootloader
};

constexpr uint16_t DIAG_FIRST = 50;
constexpr size_t DIAG_COUNT = 28;         // 50..77

/**
 * Decode registers 50..77 (r[0] = reg 50). @return false if n < DIAG_COUNT
 */
inline bool decodeDiagnostics(const uint16_t* r, size_t n, Diagnostics& out) {
    if (r == nullptr || n < DIAG_COUNT) {
        return false;
    }
    out.temperature = static_cast<int16_t>(r[0]);
    out.status = r[1];
    out.resistance = r[2];
    out.secondsSinceGood = r[3];
    out.goodReadings = r[4];
    out.failedReadings = r[5];
    out.lastFailReason = r[6];
    out.temperatureCenti = static_cast<int16_t>(r[7]);
    out.uptimeSeconds = (static_cast<uint32_t>(r[8]) << 16) | r[9];
    out.adcTimeouts = r[10];
    out.rangeErrors = r[11];
    out.adcOverflows = r[12];
    out.settingsFlags = r[13];
    out.resetCause = r[14];
    out.busFrames = r[15];
    out.busHandled = r[16];
    out.busCrcErrors = r[17];
    out.busBadLength = r[18];
    out.busFramingErrors = r[19];
    out.busParityErrors = r[20];
    out.busNoiseErrors = r[21];
    out.busOverruns = r[22];
    out.busExceptions = r[23];
    out.runningSlot = r[24];
    out.bootRecord = r[25];
    out.trialAttempts = r[26];
    out.bootloaderVersion = r[27];
    return true;
}

// Identity: factory unique ID (input 78..83) + user asset/label (holding 532..539)
constexpr uint16_t UID_FIRST = 78;
constexpr size_t UID_COUNT = 6;
constexpr uint16_t IDENT_FIRST = 532;
constexpr size_t IDENT_COUNT = 8;         // asset + 7 label registers
constexpr size_t LABEL_CHARS = 14;

struct Identity {
    uint32_t uid[3];                      // 96-bit factory ID, uid[0] = bits 95..64 (as printed by the firmware)
    uint16_t asset;                       // user number, 0 = not set
    char label[LABEL_CHARS + 1];          // user label, NUL terminated
};

/**
 * Decode input 78..83 (uid) and holding 532..539 (ident). Label bytes that are
 * not printable ASCII end the label. @return false on a null pointer
 */
inline bool decodeIdentity(const uint16_t* uid, const uint16_t* ident, Identity& out) {
    if (uid == nullptr || ident == nullptr) {
        return false;
    }
    for (size_t i = 0; i < 3; i++) {
        out.uid[i] = (static_cast<uint32_t>(uid[2 * i]) << 16) | uid[2 * i + 1];
    }
    out.asset = ident[0];
    size_t n = 0;
    for (size_t i = 0; i < LABEL_CHARS; i++) {
        uint16_t w = ident[1 + i / 2];
        char c = static_cast<char>((i % 2 == 0) ? (w >> 8) : (w & 0xFF));
        if (c < 0x20 || c > 0x7E) {
            break;
        }
        out.label[n++] = c;
    }
    out.label[n] = '\0';
    return true;
}

} // namespace andrtf3

#endif // ANDRTF3_REGISTERS_H
