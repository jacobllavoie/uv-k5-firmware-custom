#ifndef CW_H
#define CW_H

#include <stdbool.h>
#include <stdint.h>

// Using the EEPROM space from the disabled DTMF contacts
#define CW_SETTINGS_EEPROM_ADDR 0x1C00

typedef struct {
    // 2 messages, 15 chars each + null terminator
    char     messages[2][16];    // 32 bytes

    // 16 bytes for other settings
    // Other Settings
    bool     enabled;            // 1 byte
    uint8_t  wpm;                // 1 byte (Words Per Minute, 0-99)
    uint16_t tone_hz;            // 2 bytes (Tone frequency in Hz, e.g., 700)
    uint8_t  mode;               // 1 byte
    
    // 1 byte remaining for future use
    uint8_t  reserved[1];

} CW_Settings_t;

extern CW_Settings_t gCWSettings;

// Load CW settings from EEPROM
void CW_LoadSettings(void);

// Save CW settings to EEPROM
void CW_SaveSettings(void);

#endif // CW_H
