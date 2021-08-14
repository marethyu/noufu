#ifndef _INTERRUPT_MANAGER_H_
#define _INTERRUPT_MANAGER_H_

#include "GBComponent.h"

enum
{
    INT_VBLANK = 0,
    INT_LCD_STAT,
    INT_TIMER,
    INT_SERIAL,
    INT_JOYPAD
};

class InterruptManager : public GBComponent
{
private:
    bool m_IME;
    int haltMode;

    Emulator *m_Emulator;
public:
    InterruptManager(Emulator *emu);
    ~InterruptManager();

    void Init();
    void Reset();
    bool GetIME();
    void EnableIME();
    void DisableIME();
    bool SetHaltMode(); // sets halt mode and determines if halt should be executed or not
    void ResetHaltMode();
    void RequestInterrupt(int i);
    void InterruptRoutine();

    uint8_t &IF;
    uint8_t &IE;

    void Debug_PrintStatus();
};

#endif