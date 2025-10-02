#include "app/cw.h"
#include "driver/bk4819.h"
#include "driver/system.h"
#include "radio.h"
#include "misc.h"
#include "settings.h"
#include <string.h>
#include <ctype.h>

// Morse code lookup table
const char* morse_code[128] = {
    ['A'] = ".-",   ['B'] = "-...", ['C'] = "-.-.", ['D'] = "-..",
    ['E'] = ".",    ['F'] = "..-.", ['G'] = "--.",  ['H'] = "....",
    ['I'] = "..",   ['J'] = ".---", ['K'] = "-.",  ['L'] = ".-..",
    ['M'] = "--",   ['N'] = "-.",   ['O'] = "---",  ['P'] = ".--.",
    ['Q'] = "--.-", ['R'] = ".-.",  ['S'] = "...",  ['T'] = "-",
    ['U'] = "..-",  ['V'] = "...-", ['W'] = ".--",  ['X'] = "-..-",
    ['Y'] = "-.--", ['Z'] = "--..",
    ['0'] = "-----",['1'] = ".----",['2'] = "..---",['3'] = "...--",
    ['4'] = "....-",['5'] = ".....",['6'] = "-....",['7'] = "--...",
    ['8'] = "---..",['9'] = "----.",
    ['/'] = "-..-.",['='] = "-...-",['?'] = "..--..",['.'] = ".-.-.-",
    [','] = "--..--",['+'] = ".-.-.",['@'] = ".--.-.",
};

// CW State Variables
volatile CW_State_t gCW_State = CW_IDLE;
static const char*  gCW_String;
static const char*  gCW_Code;
static uint16_t     gCW_Countdown_10ms;

bool CW_IsIdle(void)
{
	return (gCW_State == CW_IDLE);
}

void CW_Start(const char* str)
{
    if (gCW_State != CW_IDLE || str == NULL || *str == '\0') {
        return;
    }

    gCW_String = str;
    gCW_Code = NULL;
    RADIO_PrepareTX(); // Key up the radio
    gCW_State = CW_KEY_UP; // Start with a gap
    gCW_Countdown_10ms = 1; // Start almost immediately
}

void CW_Transmit_String(const char* str)
{
    CW_Start(str);
    while(!CW_IsIdle()) {
        SYSTEM_DelayMs(10);
    }
}


// This function is called every 10ms by a timer
void CW_Update(void)
{
    if (gCW_State == CW_IDLE) {
        return;
    }

    if (gCW_Countdown_10ms > 0) {
        gCW_Countdown_10ms--;
        if (gCW_Countdown_10ms > 0) {
            return;
        }
    }

    // Timings are now based on 10ms ticks
    const uint16_t dot_length = (120 / gEeprom.CW_WPM); // 1200ms / WPM / 10ms ticks
    
    switch (gCW_State)
    {
        case CW_KEY_DOWN:
            BK4819_ExitTxMute(); // Stop tone
            gCW_State = CW_KEY_UP;
            gCW_Countdown_10ms = dot_length; // Symbol gap
            break;

        case CW_KEY_UP:
            if (*gCW_String == '\0') { // End of string
                gCW_State = CW_IDLE;
                RADIO_SendEndOfTransmission();
                return;
            }

            if (*gCW_String == ' ') { // Word gap
                gCW_Countdown_10ms = 7 * dot_length; // 7 units for word gap
                gCW_String++;
                gCW_Code = NULL;
                break;
            }

            if (!gCW_Code) { // Find the code for the new character
                gCW_Code = morse_code[(int)toupper((int)*gCW_String)];
            }

            if (gCW_Code && *gCW_Code) { // If there's a code and we're not at the end of it
                if (*gCW_Code == '.') {
                    gCW_Countdown_10ms = dot_length; // 1 unit for dot
                } else { // '-'
                    gCW_Countdown_10ms = 3 * dot_length; // 3 units for dash
                }
                BK4819_TransmitTone(true, gEeprom.CW_TONE_HZ);
                gCW_State = CW_KEY_DOWN;
                gCW_Code++; // Move to the next symbol in the character's code
            } else { // End of character code or invalid character
                gCW_Countdown_10ms = 3 * dot_length; // 3 units for inter-character gap
                gCW_String++; // Move to the next character in the string
                gCW_Code = NULL; // Reset for the next character
            }
            break;

        case CW_IDLE:
            break; // Should not happen
    }
}