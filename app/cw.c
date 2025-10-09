#include <string.h>
#include "cw.h"
#include "../driver/eeprom.h"

// The global instance of the CW settings
CW_Settings_t gCWSettings;

void CW_LoadSettings(void)
{
    // Read the CW settings from the dedicated EEPROM address
    EEPROM_ReadBuffer(CW_SETTINGS_EEPROM_ADDR, &gCWSettings, sizeof(CW_Settings_t));

    // If settings appear uninitialized (e.g., WPM is out of range), apply defaults.
    if (gCWSettings.wpm > 99) {
        gCWSettings.enabled = false;
        gCWSettings.wpm = 12;
        gCWSettings.tone_hz = 600;
        gCWSettings.mode = 0; // Default mode
        strcpy(gCWSettings.messages[0], "CQ CQ");
        strcpy(gCWSettings.messages[1], "DE F4HWN");
        memset(gCWSettings.reserved, 0, sizeof(gCWSettings.reserved));
        
        // Save the new default settings back to EEPROM
        CW_SaveSettings();
    }
}

void CW_SaveSettings(void)
{
    // Write the current CW settings to EEPROM
    uint8_t buffer[40]; // sizeof(gCWSettings) is 38, round up to multiple of 8
    memcpy(buffer, &gCWSettings, sizeof(gCWSettings));
    memset(buffer + sizeof(gCWSettings), 0xFF, sizeof(buffer) - sizeof(gCWSettings));

    uint16_t i;
    for (i = 0; i < sizeof(buffer); i += 8) {
        EEPROM_WriteBuffer(CW_SETTINGS_EEPROM_ADDR + i, buffer + i);
    }
}
