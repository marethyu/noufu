#include <iostream>

#include "BitMagic.h"
#include "Emulator.h"

#include "cpu/CPU.h"
#include "cpu/CPUDebugInfo.h"

#define FMT_HEADER_ONLY
#include "3rdparty/fmt/format.h"

static const uint8_t int_vectors[5] = {
    0x40,
    0x48,
    0x50,
    0x58,
    0x60
};

CPU::CPU(Emulator *emu)
{
    m_Emulator = emu;
    bLoggingEnabled = emu->m_Config->GetValue("CPULogging") == "1";
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
    std::cout << fmt::format("A(7)=${0:02X}", reg8[7]) << std::endl;
    std::cout << fmt::format("B(1)=${0:02X}", reg8[1]) << std::endl;
    std::cout << fmt::format("C(0)=${0:02X}", reg8[0]) << std::endl;
    std::cout << fmt::format("D(3)=${0:02X}", reg8[3]) << std::endl;
    std::cout << fmt::format("E(2)=${0:02X}", reg8[2]) << std::endl;
    std::cout << fmt::format("H(5)=${0:02X}", reg8[5]) << std::endl;
    std::cout << fmt::format("L(4)=${0:02X}", reg8[4]) << std::endl;
    std::cout << std::endl;
    std::cout << "Flags:" << std::endl;
    std::cout << fmt::format("Z(7)={:d} N(6)={:d} H(5)={:d} C(4)={:d}",
                             CPU::IsFlagSet(FLAG_Z),
                             CPU::IsFlagSet(FLAG_N),
                             CPU::IsFlagSet(FLAG_H),
                             CPU::IsFlagSet(FLAG_C)) << std::endl;
    std::cout << std::endl;
    std::cout << "16-bit registers:" << std::endl;
    std::cout << fmt::format("AF(3)=${0:04X}", reg16[3]) << std::endl;
    std::cout << fmt::format("BC(0)=${0:04X}", reg16[0]) << std::endl;
    std::cout << fmt::format("DE(1)=${0:04X}", reg16[1]) << std::endl;
    std::cout << fmt::format("HL(2)=${0:04X}", reg16[2]) << std::endl;
    std::cout << fmt::format("SP(4)=${0:04X}", reg16[4]) << std::endl;
    std::cout << fmt::format("PC(5)=${0:04X}", reg16[5]) << std::endl;
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

    std::cout << fmt::format("{} ({0:02X})\t", cb_prefixed ? "PREFIX_CB" : "NO_PREFIX", opcode)
              << fmt::format(info.fmt_str, arg) << std::endl;
}

void CPU::Debug_EditRegister(int reg, int val, bool is8Bit)
{
    if (is8Bit)
    {
        reg8[reg] = (uint8_t) val;
        std::cout << fmt::format("the value of {} is now set to {}", reg8_str[reg], val) << std::endl;
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

        std::cout << fmt::format("the value of {} is now set to {}", reg16_str[reg], val) << std::endl;
    }
}

void CPU::Debug_EditFlag(int flag, int val)
{
    CPU::ModifyFlag(flag, val);
    std::cout << fmt::format("flag {} is now set to {}", flag_str[flag - 4], val) << std::endl;
}

void CPU::Debug_LogState()
{
    m_Emulator->m_EmulatorLogger->DoLog(LOG_INFO, "CPU", "A: {0:02X} F: {0:02X} B: {0:02X} C: {0:02X} D: {0:02X} E: {0:02X} H: {0:02X} L: {0:02X} SP: {0:04X} PC: {0:04X} ({0:02X} {0:02X} {0:02X} {0:02X})",
                                        A,
                                        F,
                                        reg8[1],
                                        reg8[0],
                                        reg8[3],
                                        reg8[2],
                                        reg8[5],
                                        reg8[4],
                                        SP,
                                        PC,
                                        m_Emulator->m_MemControl->ReadByte(PC),
                                        m_Emulator->m_MemControl->ReadByte(PC + 1),
                                        m_Emulator->m_MemControl->ReadByte(PC + 2),
                                        m_Emulator->m_MemControl->ReadByte(PC + 3));
}