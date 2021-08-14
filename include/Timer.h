#ifndef _TIMER_H_
#define _TIMER_H_

#include "GBComponent.h"

class Timer : public GBComponent
{
private:
    Emulator *m_Emulator;

    int m_Freq;
    int m_Counter;
public:
    Timer(Emulator *emu);
    ~Timer();

    void Init();
    void Reset();
    void Update(int cycles);
    void UpdateFreq();
    int TimerEnable();
    int ClockSelect();

    uint16_t DIV;
    uint8_t &TIMA;
    uint8_t &TMA;
    uint8_t &TAC;

    void Debug_PrintStatus();
};

#endif