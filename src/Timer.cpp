#include <iostream>

#include "BitMagic.h"
#include "Emulator.h"
#include "Timer.h"
#include "Util.h"

Timer::Timer(Emulator *emu)
  : TIMA(emu->m_MemControl->m_IO[0x05]),
    TMA(emu->m_MemControl->m_IO[0x06]),
    TAC(emu->m_MemControl->m_IO[0x07])
{
    m_Emulator = emu;
}

Timer::~Timer()
{
    
}

void Timer::Init()
{
    DIV = 0x00;
    TIMA = 0x00;
    TMA = 0x00;
    TAC = 0xF8;
    m_Freq = 1024;
    m_Counter = 0;
}

void Timer::Reset()
{
    DIV = 0xABCC;
    TIMA = 0x00;
    TMA = 0x00;
    TAC = 0xF8;
    Timer::UpdateFreq();
    m_Counter = 0;
}

void Timer::Update(int cycles)
{
    DIV += cycles;

    if (Timer::TimerEnable())
    {
        m_Counter += cycles;

        Timer::UpdateFreq();

        while (m_Counter >= m_Freq)
        {
            if (TIMA < 0xFF)
            {
                TIMA++;
            }
            else
            {
                TIMA = TMA;
                m_Emulator->m_IntManager->RequestInterrupt(INT_TIMER);
            }

            m_Counter -= m_Freq;
        }
    }
}

void Timer::UpdateFreq()
{
    switch (Timer::ClockSelect())
    {
    case 0: m_Freq = 1024; break;
    case 1: m_Freq = 16;   break;
    case 2: m_Freq = 64;   break;
    case 3: m_Freq = 256;  break;
    }
}

int Timer::TimerEnable()
{
    return TEST_BIT(TAC, 2);
}

int Timer::ClockSelect()
{
    return TAC & 0x3;
}

void Timer::Debug_PrintStatus()
{
    std::cout << "*TIMER STATUS*" << std::endl;
    std::cout << "DIV=" << int_to_hex(DIV, 4) << " "
              << "TIMA=" << int_to_hex(+TIMA, 2) << " "
              << "TMA=" << int_to_hex(+TMA, 2) << " "
              << "TAC=" << int_to_bin8(TAC) << std::endl;
    std::cout << std::endl;
}