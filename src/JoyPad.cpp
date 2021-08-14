#include <iostream>

#include "BitMagic.h"
#include "Emulator.h"
#include "JoyPad.h"
#include "Util.h"

JoyPad::JoyPad(Emulator *emu) : P1(emu->m_MemControl->m_IO[0x00])
{
    m_Emulator = emu;
}

JoyPad::~JoyPad()
{
    
}

void JoyPad::Init()
{
    joypad_state = 0xFF; // 0b11111111
    P1 = 0xC0; // 0b11000000
}

void JoyPad::Reset()
{
    joypad_state = 0xFF;
    P1 = 0xCF;
}

void JoyPad::PressButton(int button)
{
    bool pressed = !TEST_BIT(joypad_state, button);

    RES_BIT(joypad_state, button);

    if (((button > 3 && !TEST_BIT(P1, 5)) ||
         (button <= 3 && !TEST_BIT(P1, 4))) &&
        !pressed)
    {
        m_Emulator->m_IntManager->RequestInterrupt(INT_JOYPAD);
    }
}

void JoyPad::ReleaseButton(int button)
{
    SET_BIT(joypad_state, button);
}

uint8_t JoyPad::ReadP1()
{
    int jp_reg = P1;

    jp_reg |= 0xCF; // 0b11001111

    if (!TEST_BIT(jp_reg, 5)) // standard buttons
    {
        uint8_t hi = joypad_state >> 4; // the states of standard buttons are stored at upper 4 bits, remember?
        jp_reg &= 0xF0 | hi; // 0xF0 is necessary for bitwise & operation
    }

    if (!TEST_BIT(jp_reg, 4)) // directional buttons
    {
        uint8_t lo = joypad_state & 0x0F;
        jp_reg &= 0xF0 | lo;
    }

    return jp_reg;
}

void JoyPad::Debug_PrintStatus()
{
    std::cout << "*JoyPad STATUS*" << std::endl;
    std::cout << "P1=" << int_to_bin8(JoyPad::ReadP1()) << "\n"
              << "Right=" << GET_BIT(joypad_state, JOYPAD_RIGHT) << " "
              << "Left=" << GET_BIT(joypad_state, JOYPAD_LEFT) << " "
              << "Up=" << GET_BIT(joypad_state, JOYPAD_UP) << " "
              << "Down=" << GET_BIT(joypad_state, JOYPAD_DOWN) << " "
              << "A=" << GET_BIT(joypad_state, JOYPAD_A) << " "
              << "B=" << GET_BIT(joypad_state, JOYPAD_B) << " "
              << "Select=" << GET_BIT(joypad_state, JOYPAD_SELECT) << " "
              << "Start=" << GET_BIT(joypad_state, JOYPAD_START) << std::endl;
    std::cout << std::endl;
}