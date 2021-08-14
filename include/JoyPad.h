#ifndef _JOYPAD_H_
#define _JOYPAD_H_

#include "GBComponent.h"

enum
{
    JOYPAD_RIGHT = 0,
    JOYPAD_LEFT,
    JOYPAD_UP,
    JOYPAD_DOWN,
    JOYPAD_A,
    JOYPAD_B,
    JOYPAD_SELECT,
    JOYPAD_START
};

// good explanation here https://github.com/gbdev/pandocs/blob/be820673f71c8ca514bab4d390a3004c8273f988/historical/1995-Jan-28-GAMEBOY.txt#L165
class JoyPad : public GBComponent
{
private:
    Emulator *m_Emulator;

    uint8_t &P1;

    // upper 4 bits - standard buttons
    // lower 4 bits - directional buttons
    // see JOYPAD_ enum for more info
    uint8_t joypad_state;
public:
    JoyPad(Emulator *emu);
    ~JoyPad();

    void Init();
    void Reset();

    void PressButton(int button);
    void ReleaseButton(int button);
    uint8_t ReadP1();

    void Debug_PrintStatus();
};

#endif