#include <iostream>

#include "BitMagic.h"
#include "Emulator.h"
#include "InterruptManager.h"

#define FMT_HEADER_ONLY
#include "3rdparty/fmt/format.h"

// For halt behaviour, please read section 4.10. in https://github.com/AntonioND/giibiiadvance/blob/master/docs/TCAGBD.pdf
enum
{
    HALT_MODE0 = 0,
    HALT_MODE1,
    HALT_MODE2
};

static const std::string int_str[5] = {
    "VBLANK",
    "LCD_STAT",
    "TIMER",
    "SERIAL",
    "JOYPAD"
};

InterruptManager::InterruptManager(Emulator *emu)
  : IF(emu->m_MemControl->m_IO[0x0F]),
    IE(emu->m_MemControl->IE)
{
    m_Emulator = emu;
    bLoggingEnabled = emu->m_Config->GetValue("InterruptLogging") == "1";
}

InterruptManager::~InterruptManager()
{
    
}

void InterruptManager::Init()
{
    m_IME = false;
    haltMode = -1;
    IF = 0x0;
    IE = 0x0;
}

void InterruptManager::Reset()
{
    m_IME = false;
    haltMode = -1;
    IF = 0xE0;
    IE = 0;
}

bool InterruptManager::GetIME()
{
    return m_IME;
}

void InterruptManager::EnableIME()
{
    if (bLoggingEnabled)
    {
        m_Emulator->m_EmulatorLogger->DoLog(LOG_INFO, "InterruptManager::EnableIME", "EI");
    }

    m_IME = true;
}

void InterruptManager::DisableIME()
{
    if (bLoggingEnabled)
    {
        m_Emulator->m_EmulatorLogger->DoLog(LOG_INFO, "InterruptManager::DisableIME", "DI");
    }

    m_IME = false;
}

bool InterruptManager::SetHaltMode()
{
    if (m_IME)
    {
        haltMode = HALT_MODE0;
    }
    else if (!m_IME && ((IE & IF) == 0))
    {
        haltMode = HALT_MODE1;
    }
    else if (!m_IME && ((IE & IF) != 0))
    {
        haltMode = HALT_MODE2;
    }

    if (bLoggingEnabled)
    {
        m_Emulator->m_EmulatorLogger->DoLog(LOG_INFO, "InterruptManager::SetHaltMode", "HALT_MODE{}", haltMode);
    }

    return haltMode == HALT_MODE0 || haltMode == HALT_MODE1;
}

void InterruptManager::ResetHaltMode()
{
    if (bLoggingEnabled)
    {
        m_Emulator->m_EmulatorLogger->DoLog(LOG_INFO, "InterruptManager::ResetHaltMode", "-1");
    }

    haltMode = -1;
}

void InterruptManager::RequestInterrupt(int i)
{
    if (bLoggingEnabled)
    {
        m_Emulator->m_EmulatorLogger->DoLog(LOG_INFO, "InterruptManager::RequestInterrupt", "{} interrupt requested", int_str[i]);
    }

    SET_BIT(IF, i);
}

void InterruptManager::InterruptRoutine()
{
    // once you enter halt mode, the only thing you can do to get out of it is to receive an interrupt
    if (haltMode != HALT_MODE2 && ((IE & IF) != 0))
    {
        if (m_Emulator->m_CPU->isHalted())
        {
            m_Emulator->m_CPU->Resume();
            if (!m_IME) m_Emulator->Tick(); // 1 cycle increase (tick) for exiting halt mode when IME=0 and is halted ??
            InterruptManager::ResetHaltMode();
        }

        for (int i = 0; i < 5; ++i)
        {
            if (m_IME && GET_BIT(IF, i) && GET_BIT(IE, i))
            {
                if (bLoggingEnabled)
                {
                    m_Emulator->m_EmulatorLogger->DoLog(LOG_INFO, "InterruptManager::InterruptRoutine", "Now handling {}", int_str[i]);
                }

                m_Emulator->m_CPU->HandleInterrupt(i);
            }
        }
    }
}

void InterruptManager::Debug_PrintStatus()
{
    std::cout << "*INTERRUPT MANAGER STATUS*" << std::endl;
    std::cout << fmt::format("IME={:d}", m_IME) << std::endl;
    std::cout << fmt::format("IF={0:08b}b", IF) << std::endl;
    std::cout << fmt::format("IE={0:08b}b", IE) << std::endl;
    std::cout << std::endl;
}