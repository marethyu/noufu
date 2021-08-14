#include <iostream>

#include <cstdio>

#include "BitMagic.h"
#include "CPU.h"
#include "Emulator.h"
#include "OpcodeInfo.h"
#include "Util.h"

const uint8_t int_vectors[5] = {
    0x40,
    0x48,
    0x50,
    0x58,
    0x60
};

CPU::CPU(Emulator *emu)
{
    m_Emulator = emu;
}

CPU::~CPU()
{
    
}

void CPU::Init()
{
    ei_delay_cnt = 0;
    PC = 0x0;
    bHalted = false;
    haltBug = false;
}

void CPU::Reset()
{
    reg8[0] = 19;
    reg8[1] = 0;
    reg8[2] = 216;
    reg8[3] = 0;
    reg8[4] = 77;
    reg8[5] = 1;
    reg8[6] = 176;
    reg8[7] = 1;
    reg8[8] = 254;
    reg8[9] = 255;
    ei_delay_cnt = 0;
    PC = 0x100;
    bHalted = false;
    haltBug = false;
}

void CPU::HandleInterrupt(int i)
{
    // https://gbdev.io/pandocs/Interrupts.html#interrupt-handling
    // 5 cycles (+1 if halt)
    RES_BIT(m_Emulator->m_IntManager->IF, i);
    m_Emulator->m_IntManager->DisableIME();
    m_Emulator->Tick();
    m_Emulator->Tick();
    CPU::Call(int_vectors[i]);
    m_Emulator->Tick();
    if (bHalted) m_Emulator->Tick(); // refer to 4.9 in https://github.com/AntonioND/giibiiadvance/blob/master/docs/TCAGBD.pdf
}

bool CPU::isHalted()
{
    return bHalted;
}

void CPU::Resume()
{
    bHalted = false;
}

void CPU::Debug_PrintStatus()
{
    std::cout << "*CPU STATUS*" << std::endl;
    std::cout << "8-bit registers:" << std::endl;
    std::cout << "A(7)=" << int_to_hex(+reg8[7], 2) << std::endl;
    std::cout << "B(1)=" << int_to_hex(+reg8[1], 2) << std::endl;
    std::cout << "C(0)=" << int_to_hex(+reg8[0], 2) << std::endl;
    std::cout << "D(3)=" << int_to_hex(+reg8[3], 2) << std::endl;
    std::cout << "E(2)=" << int_to_hex(+reg8[2], 2) << std::endl;
    std::cout << "H(5)=" << int_to_hex(+reg8[5], 2) << std::endl;
    std::cout << "L(4)=" << int_to_hex(+reg8[4], 2) << std::endl;
    std::cout << std::endl;
    std::cout << "Flags:" << std::endl;
    std::cout << "Z(7)=" << CPU::IsFlagSet(FLAG_Z) << " "
              << "N(6)=" << CPU::IsFlagSet(FLAG_N) << " "
              << "H(5)=" << CPU::IsFlagSet(FLAG_H) << " "
              << "C(4)=" << CPU::IsFlagSet(FLAG_C) << std::endl;
    std::cout << std::endl;
    std::cout << "16-bit registers:" << std::endl;
    std::cout << "AF(3)=" << int_to_hex(reg16[3], 4) << std::endl;
    std::cout << "BC(0)=" << int_to_hex(reg16[0], 4) << std::endl;
    std::cout << "DE(1)=" << int_to_hex(reg16[1], 4) << std::endl;
    std::cout << "HL(2)=" << int_to_hex(HL, 4) << std::endl;
    std::cout << "SP(4)=" << int_to_hex(SP, 4) << std::endl;
    std::cout << "PC(5)=" << int_to_hex(PC, 4) << std::endl;
    std::cout << std::endl;
}

void CPU::Debug_PrintCurrentInstruction()
{
    uint16_t addr = PC;
    uint8_t temp = m_Emulator->m_MemControl->ReadByte(addr++);
    uint8_t opcode;
    bool cb_prefixed = temp == 0xCB;
    int arg = 0;

    if (cb_prefixed) opcode = m_Emulator->m_MemControl->ReadByte(addr++);
    else opcode = temp;

    OpcodeInfo info = cb_prefixed ? cb_opcode_info[opcode] : opcode_info[opcode];

    switch (info.arg_type)
    {
    case ARG_U8:
    case ARG_I8:
        arg = m_Emulator->m_MemControl->ReadByte(addr);
        break;
    case ARG_U16:
        arg = m_Emulator->m_MemControl->ReadWord(addr);
        break;
    }

    printf("%s (%02X)\t", cb_prefixed ? "PREFIX_CB" : "NO_PREFIX", opcode);
    printf(info.fmt_str.c_str(), arg);
    printf("\n");
}

void CPU::Debug_EditRegister(int reg, int val, bool is8Bit)
{
    if (is8Bit)
    {
        reg8[reg] = (uint8_t) val;
        std::cout << "the value of " << reg8_str[reg] << " is now set to " << val << std::endl;
    }
    else
    {
        if (reg == 5)
        {
            PC = (uint16_t) val;
        }
        else
        {
            reg16[reg] = (uint16_t) val;
        }

        std::cout << "the value of " << reg16_str[reg] << " is now set to " << val << std::endl;
    }
}

void CPU::Debug_EditFlag(int flag, int val)
{
    CPU::ModifyFlag(flag, val);
    std::cout << "flag " << flag_str[flag - 4] << " is now set to " << val << std::endl;
}

/*
void CPU::Debug_LogState()
{
    fout << "A: " << int_to_hex(+A, 2, false) << " "
         << "F: " << int_to_hex(+F, 2, false) << " "
         << "B: " << int_to_hex(+reg8[1], 2, false) << " "
         << "C: " << int_to_hex(+reg8[0], 2, false) << " "
         << "D: " << int_to_hex(+reg8[3], 2, false) << " "
         << "E: " << int_to_hex(+reg8[2], 2, false) << " "
         << "H: " << int_to_hex(+reg8[5], 2, false) << " "
         << "L: " << int_to_hex(+reg8[4], 2, false) << " "
         << "SP: " << int_to_hex(SP, -1, false) << " "
         << "PC: " << "00:" << int_to_hex(PC, -1, false) << " "
         << "("
         << int_to_hex(+m_Emulator->m_MemControl->ReadByte(PC), 2, false) << " "
         << int_to_hex(+m_Emulator->m_MemControl->ReadByte(PC + 1), 2, false) << " "
         << int_to_hex(+m_Emulator->m_MemControl->ReadByte(PC + 2), 2, false) << " "
         << int_to_hex(+m_Emulator->m_MemControl->ReadByte(PC + 3), 2, false) << ")"
         << std::endl;
}
*/