#ifndef APP_CW_H
#define APP_CW_H

#include <stdbool.h>

// Represents the current state of the CW transmission
typedef enum {
    CW_IDLE,
    CW_KEY_DOWN,
    CW_KEY_UP,
} CW_State_t;

extern volatile CW_State_t gCW_State;

// Call this from a periodic timer to update the CW state machine
void CW_Update(void);

// Starts the transmission of a CW string
void CW_Start(const char* str);

// Transmit a CW string, blocking until complete
void CW_Transmit_String(const char* str);

// Checks if the CW engine is currently idle
bool CW_IsIdle(void);

#endif // APP_CW_H
