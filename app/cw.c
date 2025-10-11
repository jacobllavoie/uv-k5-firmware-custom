#include <string.h>
#include "cw.h"
#include "../driver/eeprom.h"
#include "../driver/bk4819.h"
#include "../driver/system.h"
#include "radio.h"
#include "app.h"
#include "misc.h"

// The global instance of the CW settings
CW_Settings_t gCWSettings;
void CW_LoadSettings(void)
{
    // Read the CW settings from the dedicated EEPROM address
    EEPROM_ReadBuffer(CW_SETTINGS_EEPROM_ADDR, &gCWSettings, sizeof(CW_Settings_t));

    // If settings appear uninitialized (e.g., WPM is out of range), apply defaults.
    if (gCWSettings.wpm > 99 || gCWSettings.tone_hz > 2000 || gCWSettings.tone_hz < 1) {
        gCWSettings.enabled = false;
        gCWSettings.wpm = 12;
        gCWSettings.tone_hz = 600;
        gCWSettings.mode = 0; // Default mode
        strcpy(gCWSettings.messages[0], "CQ CQ CQ");
        strcpy(gCWSettings.messages[1], "DE KO6DSS");
        
        strcpy(gCWSettings.callsign, "N0CALL");
        strcpy(gCWSettings.grid_square, "N0GRID");
        gCWSettings.tx_mode = CW_TX_MODE_FM;
        gCWSettings.bandwidth = CW_BANDWIDTH_NARROW;
        gCWSettings.fox_hunt_enabled = false;
        gCWSettings.pip_count = 5;
        gCWSettings.pip_interval = 15;
        gCWSettings.id_interval = 10;
        gCWSettings.sos_mode_enabled = false;
        gCWSettings.sos_duty_cycle = 20;
        gCWSettings.eot_enabled = false;

        // Save the new default settings back to EEPROM
        CW_SaveSettings();
    }
}

void CW_SaveSettings(void)
{
    // Write the current CW settings to EEPROM
    uint8_t  buffer[sizeof(CW_Settings_t)];
    
    memset(buffer, 0xFF, sizeof(buffer));
    memcpy(buffer, &gCWSettings, sizeof(gCWSettings));

    for (unsigned int i = 0; i < sizeof(buffer); i += 8) {
        EEPROM_WriteBuffer(CW_SETTINGS_EEPROM_ADDR + i, buffer + i);
    }
}

static void CW_Transmit_Pips(uint8_t count)
{
    if (!gCWSettings.enabled || count == 0)
        return;

    // Set up the transmitter for sending a tone
    BK4819_EnterTxMute();
    BK4819_SetAF(BK4819_AF_MUTE);
    BK4819_WriteRegister(BK4819_REG_70, BK4819_REG_70_ENABLE_TONE1 | (66u << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN));
    BK4819_WriteRegister(BK4819_REG_71, (((uint32_t)gCWSettings.tone_hz * 1353245u) + (1u << 16)) >> 17);
    BK4819_EnableTXLink();
    SYSTEM_DelayMs(50);

    uint32_t dot_duration = 1200 / gCWSettings.wpm;

    for (uint8_t i = 0; i < count; i++) {
        BK4819_ExitTxMute();
        SYSTEM_DelayMs(dot_duration * 3); // Dash duration
        BK4819_EnterTxMute();
        SYSTEM_DelayMs(dot_duration * 3); // space between pips, same as a dash
    }

    // Stop the tone generator
    BK4819_WriteRegister(BK4819_REG_70, 0);
}


static uint32_t CW_Calculate_String_OnAir_ms(const char *str, uint8_t wpm) {
    if (wpm == 0) wpm = 12;
    uint32_t dot_duration_ms = 1200 / wpm;
    uint32_t on_air_duration_ms = 0;

    for (size_t i = 0; str[i] != '\0'; i++)
    {
        char c = str[i];
        if (c == ' ') {
            continue;
        }

        int code_char = get_morse_code_char(c);
        if (code_char != -1)
        {
            const char *code = morse_code[code_char];
            for (size_t j = 0; code[j] != '\0'; j++)
            {
                if (code[j] == '.')
                {
                    on_air_duration_ms += dot_duration_ms;
                }
                else // '-'
                {
                    on_air_duration_ms += dot_duration_ms * 3;
                }
            }
        }
    }
    return on_air_duration_ms;
}

static uint32_t CW_Calculate_String_Duration_ms(const char *str, uint8_t wpm) {
    if (wpm == 0) wpm = 12;
    uint32_t dot_duration_ms = 1200 / wpm;
    uint32_t total_duration_ms = 0;

    for (size_t i = 0; str[i] != '\0'; i++)
    {
        char c = str[i];
        if (c == ' ') {
            total_duration_ms += dot_duration_ms * 4; // from transmit function
            continue;
        }

        int code_char = get_morse_code_char(c);
        if (code_char != -1)
        {
            const char *code = morse_code[code_char];
            for (size_t j = 0; code[j] != '\0'; j++)
            {
                if (code[j] == '.')
                {
                    total_duration_ms += dot_duration_ms;
                }
                else // '-'
                {
                    total_duration_ms += dot_duration_ms * 3;
                }
                total_duration_ms += dot_duration_ms; // Inter-element gap
            }
            total_duration_ms += dot_duration_ms * 2; // Inter-letter space
        }
    }
    return total_duration_ms;
}

void CW_HandleAutomaticTransmission(void)
{
	static uint32_t pip_countdown = 0;
	static uint32_t id_countdown = 0;
#ifdef ENABLE_SOS
	static uint32_t sos_countdown = 0;
#endif
	static bool transmitting = false;

	if (transmitting) {
		return;
	}

	if (gCWSettings.fox_hunt_enabled) {
		if (pip_countdown == 0) {
			if (gCWSettings.pip_count > 0) {
				transmitting = true;
				RADIO_PrepareTX();
				CW_Transmit_Pips(gCWSettings.pip_count);
				APP_EndTransmission();
				FUNCTION_Select(FUNCTION_FOREGROUND);
				gFlagEndTransmission = false;
				transmitting = false;
			}
			pip_countdown = (uint32_t)gCWSettings.pip_interval * 100;
		} else {
			pip_countdown--;
		}
		
		if (id_countdown == 0) {
			if (strlen(gCWSettings.callsign) > 0) {
				transmitting = true;
				RADIO_PrepareTX();
				CW_Transmit_String(gCWSettings.callsign, gCWSettings.wpm);
				APP_EndTransmission();
				FUNCTION_Select(FUNCTION_FOREGROUND);
				gFlagEndTransmission = false;
				transmitting = false;
			}
			id_countdown = (uint32_t)gCWSettings.id_interval * 60 * 100;
		} else {
			id_countdown--;
		}
	} else {
		pip_countdown = 0;
		id_countdown = 0;
	}

#ifdef ENABLE_SOS
    if (gCWSettings.sos_mode_enabled) {
        if (sos_countdown == 0) {
            transmitting = true;

            char sos_message[32] = "SOS";
            if (strlen(gCWSettings.grid_square) > 0) {
                sprintf(sos_message, "SOS %s", gCWSettings.grid_square);
            }

            RADIO_PrepareTX();
            CW_Transmit_String(sos_message, 10);
            APP_EndTransmission();
            FUNCTION_Select(FUNCTION_FOREGROUND);
            gFlagEndTransmission = false;
            transmitting = false;

            // Calculate the delay needed for the desired duty cycle
            uint32_t sos_on_time_ms = CW_Calculate_String_OnAir_ms(sos_message, 10);
            uint32_t sos_total_tx_time_ms = CW_Calculate_String_Duration_ms(sos_message, 10);

            if (gCWSettings.sos_duty_cycle > 0 && gCWSettings.sos_duty_cycle <= 50) {
                uint32_t total_period_ms = (sos_on_time_ms * 100) / gCWSettings.sos_duty_cycle;
                uint32_t pause_duration_ms = total_period_ms > sos_total_tx_time_ms ? total_period_ms - sos_total_tx_time_ms : 0;
                sos_countdown = pause_duration_ms / 10;
            } else {
                // Default to a reasonable pause if duty cycle is invalid (e.g. 5 seconds)
                sos_countdown = 500;
            }
        } else {
            sos_countdown--;
        }
    }
#endif
}

static const char *morse_code[] = {
    /* 0-9 */
    "-----", ".----", "..---", "...--", "....-", ".....", "-....", "--...", "---..", "----.",
    /* A-Z */
    ".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---", "-.- ", ".-..", "--", "-.", "---", ".--.", "--.-", ".-.", "...", "-", "..-", "...-", ".--", "-..-", "-.--", "--..",
    /* punctuation */
    ".-.-.-", /* . */
    "--..--", /* , */
    "..--..", /* ? */
    "-...-",  /* = */
    "-..-.",  /* / */
};

static int get_morse_code_char(const char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'Z')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 10;
    if (c == '.') return 36;
    if (c == ',') return 37;
    if (c == '?') return 38;
    if (c == '=') return 39;
    if (c == '/') return 40;
    return -1;
}

void CW_Transmit_String(const char *str, uint8_t wpm)
{
    if (!gCWSettings.enabled)
        return;

    if (wpm == 0) {
        wpm = 12;
    }

    uint32_t dot_duration = 1200 / wpm;

    // Set up the transmitter for sending a tone
    BK4819_EnterTxMute();
    BK4819_SetAF(BK4819_AF_MUTE);
    BK4819_WriteRegister(BK4819_REG_70, BK4819_REG_70_ENABLE_TONE1 | (66u << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN));
    BK4819_WriteRegister(BK4819_REG_71, (((uint32_t)gCWSettings.tone_hz * 1353245u) + (1u << 16)) >> 17);
    BK4819_EnableTXLink();
    SYSTEM_DelayMs(50);

    for (size_t i = 0; str[i] != '\0'; i++)
    {
        char c = str[i];
        if (c == ' ') {
            SYSTEM_DelayMs(dot_duration * 4); // Word space (7 units) - inter-letter (3) = 4
            continue;
        }

        int code_char = get_morse_code_char(c);
        if (code_char != -1)
        {
            const char *code = morse_code[code_char];
            for (size_t j = 0; code[j] != '\0'; j++)
            {
                BK4819_ExitTxMute(); // Start transmitting tone
                if (code[j] == '.')
                {
                    SYSTEM_DelayMs(dot_duration);
                }
                else
                {
                    SYSTEM_DelayMs(dot_duration * 3);
                }
                BK4819_EnterTxMute(); // Stop transmitting tone
                SYSTEM_DelayMs(dot_duration); // Inter-element gap
            }
            SYSTEM_DelayMs(dot_duration * 2); // Inter-letter space (3 units) - inter-element (1) = 2
        }
    }

    // Stop the tone generator
    BK4819_WriteRegister(BK4819_REG_70, 0);
}