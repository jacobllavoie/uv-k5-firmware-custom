/* Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */

#ifndef CW_H
#define CW_H

#include <stdbool.h>
#include <stdint.h>
#include "app.h"

// Using the EEPROM space from the disabled DTMF contacts
#define CW_SETTINGS_EEPROM_ADDR 0x1C00

typedef enum {
    CW_TX_MODE_FM = 0,
    CW_TX_MODE_CW,
} CW_TX_Mode_t;

typedef enum {
    CW_BANDWIDTH_WIDE = 0,
    CW_BANDWIDTH_NARROW,
} CW_Bandwidth_t;

typedef struct {
    // 2 messages, 15 chars each + null terminator
    char     messages[2][16];    // 32 bytes

    // 16 bytes for other settings
    // Other Settings
    bool     enabled;            // 1 byte
    uint8_t  wpm;                // 1 byte (Words Per Minute, 0-99)
    uint16_t tone_hz;            // 2 bytes (Tone frequency in Hz, e.g., 700)
    uint8_t  mode;               // 1 byte
    
    char callsign[16];
    char grid_square[16];
    CW_TX_Mode_t tx_mode;
    CW_Bandwidth_t bandwidth;
    bool     fox_hunt_enabled;
    uint8_t  pip_count;
    uint8_t  pip_interval;
    uint8_t  id_interval;
    bool     sos_mode_enabled;
    uint8_t  sos_duty_cycle;
    bool     eot_enabled;

} CW_Settings_t;

extern CW_Settings_t gCWSettings;

// Load CW settings from EEPROM
void CW_LoadSettings(void);

// Save CW settings to EEPROM
void CW_SaveSettings(void);

void CW_HandleAutomaticTransmission(void);

void CW_Transmit_String(const char* str, uint8_t wpm);

#endif // CW_H
