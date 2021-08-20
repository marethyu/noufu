#include <stdexcept>

#include "BitMagic.h"
#include "CPU.h"
#include "Emulator.h"

static const int HL_add[] = {0, 0, 1, -1};
static const int RST_ADDR[] = {0x00, 0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38};

#define HC8(A, B, cy) ((((A) & 0x0F) + ((B) & 0x0F) + (cy)) > 0x0F)
#define CARRY8(A, B, cy) ((((A) & 0xFF) + ((B) & 0xFF) + (cy)) >= 0x100)
/* Read this https://stackoverflow.com/questions/57958631/ for information on 16-bit HC */
#define HC16(A, B) ((((A) & 0xFFF) + ((B) & 0xFFF)) > 0xFFF)
#define CARRY16(A, B) (((A) + (B)) >= 0x10000)
#define HC8_SUB(A, B, cy) ((((A) & 0x0F) - ((B) & 0x0F) - (cy)) < 0)
#define CARRY8_SUB(A, B, cy) ((((A) & 0xFF) - ((B) & 0xFF) - (cy)) < 0)

uint8_t CPU::ReadByte(uint16_t address) const
{
    m_Emulator->Tick();

    if (address >= 0x8000 && address < 0xA000) // VRAM
    {
        return m_Emulator->m_PPU->CPUReadVRAM(address);
    }
    else if (address >= 0xFE00 && address < 0xFEA0) // OAM
    {
        return m_Emulator->m_PPU->CPUReadOAM(address);
    }

    return m_Emulator->m_MemControl->ReadByte(address);
}

uint16_t CPU::ReadWord(uint16_t address) const
{
    uint8_t low = CPU::ReadByte(address);
    uint8_t high = CPU::ReadByte(address + 1);
    return (high << 8) | low;
}

void CPU::WriteByte(uint16_t address, uint8_t data)
{
    m_Emulator->Tick();

    if (address >= 0x8000 && address < 0xA000) // VRAM
    {
        m_Emulator->m_PPU->CPUWriteVRAM(address, data);
    }
    else if (address >= 0xFE00 && address < 0xFEA0) // OAM
    {
        m_Emulator->m_PPU->CPUWriteOAM(address, data);
    }

    m_Emulator->m_MemControl->WriteByte(address, data);
}

void CPU::WriteWord(uint16_t address, uint16_t data)
{
    uint8_t high = data >> 8;
    uint8_t low = data & 0xFF;
    CPU::WriteByte(address, low);
    CPU::WriteByte(address + 1, high);
}

uint8_t CPU::NextByte()
{
    return CPU::ReadByte(PC++);
}

uint16_t CPU::NextWord()
{
    uint16_t word = CPU::ReadWord(PC);
    PC += 2;
    return word;
}

bool CPU::IsFlagSet(int flag)
{
    return GET_BIT(F, flag);
}

void CPU::ModifyFlag(int flag, int value)
{
    F = (F & ~(1 << flag)) | (value << flag);
}

void CPU::Push(uint16_t data)
{
    CPU::WriteByte(--SP, data >> 8); // hi
    CPU::WriteByte(--SP, data & 0xFF); // lo
}

uint16_t CPU::Pop()
{
    uint16_t data = CPU::ReadWord(SP);
    SP += 2;
    return data;
}

bool CPU::Condition(int i)
{
    bool result = false;

    switch (i)
    {
    case 0: result = !CPU::IsFlagSet(FLAG_Z); break;
    case 1: result =  CPU::IsFlagSet(FLAG_Z); break;
    case 2: result = !CPU::IsFlagSet(FLAG_C); break;
    case 3: result =  CPU::IsFlagSet(FLAG_C); break;
    }

    if (result) m_Emulator->Tick();

    return result;
}

void CPU::Call(uint16_t addr)
{
    CPU::Push(PC);
    PC = addr;
}

void CPU::RotateBitsLeft(uint8_t &n, bool circular, bool resetZ)
{
    bool bIsBit7Set = GET_BIT(n, 7);
    n <<= 1;
    if (circular ? bIsBit7Set : CPU::IsFlagSet(FLAG_C)) SET_BIT(n, 0);
    CPU::ModifyFlag(FLAG_Z, resetZ ? 0 : n == 0);
    CPU::ModifyFlag(FLAG_N, 0);
    CPU::ModifyFlag(FLAG_H, 0);
    CPU::ModifyFlag(FLAG_C, bIsBit7Set);
}

void CPU::RotateBitsRight(uint8_t &n, bool circular, bool resetZ)
{
    bool bIsBit0Set = GET_BIT(n, 0);
    n >>= 1;
    if (circular ? bIsBit0Set : CPU::IsFlagSet(FLAG_C)) SET_BIT(n, 7);
    CPU::ModifyFlag(FLAG_Z, resetZ ? 0 : n == 0);
    CPU::ModifyFlag(FLAG_N, 0);
    CPU::ModifyFlag(FLAG_H, 0);
    CPU::ModifyFlag(FLAG_C, bIsBit0Set);
}

void CPU::ShiftBitsLeft(uint8_t &n)
{
    bool bIsBit7Set = GET_BIT(n, 7);
    n <<= 1;
    CPU::ModifyFlag(FLAG_Z, n == 0);
    CPU::ModifyFlag(FLAG_N, 0);
    CPU::ModifyFlag(FLAG_H, 0);
    CPU::ModifyFlag(FLAG_C, bIsBit7Set);
}

void CPU::ShiftBitsRight(uint8_t &n, bool logical)
{
    bool bIsBit0Set = GET_BIT(n, 0);
    bool bIsBit7Set = GET_BIT(n, 7);
    n >>= 1;
    if (!logical && bIsBit7Set) SET_BIT(n, 7); // The content of bit 7 is unchanged. (see the gameboy programming manual)
    CPU::ModifyFlag(FLAG_Z, n == 0);
    CPU::ModifyFlag(FLAG_N, 0);
    CPU::ModifyFlag(FLAG_H, 0);
    CPU::ModifyFlag(FLAG_C, bIsBit0Set);
}

void CPU::HandlePrefixCB()
{
    uint8_t opcode;

    switch (opcode = CPU::NextByte())
    {
    /* rl(c) r8 */
    case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x07: // rlc r8
    case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x17: // rl r8
    {
        CPU::RotateBitsLeft(*reg8_group[opcode & 7], !((opcode >> 3) & 7));
        break;
    }
    /* rl(c) (HL) */
    case 0x06: case 0x16:
    {
        uint8_t hl = CPU::ReadByte(HL);
        CPU::RotateBitsLeft(hl, opcode == 0x06);
        CPU::WriteByte(HL, hl);
        break;
    }
    /* rr(c) r8 */
    case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D: case 0x0F: // rrc r8
    case 0x18: case 0x19: case 0x1A: case 0x1B: case 0x1C: case 0x1D: case 0x1F: // rr r8
    {
        CPU::RotateBitsRight(*reg8_group[(opcode & 0x0F) - 0x08], !((opcode >> 4) & 0x0F));
        break;
    }
    /* rr(c) (HL) */
    case 0x0E: case 0x1E:
    {
        uint8_t hl = CPU::ReadByte(HL);
        CPU::RotateBitsRight(hl, opcode == 0x0E);
        CPU::WriteByte(HL, hl);
        break;
    }
    /* sla r8 */
    case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x27: // sla r8
    {
        CPU::ShiftBitsLeft(*reg8_group[opcode - 0x20]);
        break;
    }
    /* sla (HL) */
    case 0x26:
    {
        uint8_t hl = CPU::ReadByte(HL);
        CPU::ShiftBitsLeft(hl);
        CPU::WriteByte(HL, hl);
        break;
    }
    /* swap r8 */
    case 0x30: case 0x31: case 0x32: case 0x33: case 0x34: case 0x35: case 0x37: // swap r8
    {
        int idx = opcode - 0x30;
        uint8_t n = *reg8_group[idx];
        SWAP_NIBBLES8(n);
        CPU::ModifyFlag(FLAG_Z, n == 0);
        CPU::ModifyFlag(FLAG_N, 0);
        CPU::ModifyFlag(FLAG_H, 0);
        CPU::ModifyFlag(FLAG_C, 0);
        *reg8_group[idx] = n;
        break;
    }
    /* swap (HL) */
    case 0x36:
    {
        uint8_t hl = CPU::ReadByte(HL);
        SWAP_NIBBLES8(hl);
        CPU::ModifyFlag(FLAG_Z, hl == 0);
        CPU::ModifyFlag(FLAG_N, 0);
        CPU::ModifyFlag(FLAG_H, 0);
        CPU::ModifyFlag(FLAG_C, 0);
        CPU::WriteByte(HL, hl);
        break;
    }
    /* sra r8 */
    case 0x28: case 0x29: case 0x2A: case 0x2B: case 0x2C: case 0x2D: case 0x2F: // sra r8
    {
        CPU::ShiftBitsRight(*reg8_group[opcode - 0x28]);
        break;
    }
    /* sra (HL) */
    case 0x2E:
    {
        uint8_t hl = CPU::ReadByte(HL);
        CPU::ShiftBitsRight(hl);
        CPU::WriteByte(HL, hl);
        break;
    }
    /* srl r8 */
    case 0x38: case 0x39: case 0x3A: case 0x3B: case 0x3C: case 0x3D: case 0x3F: // srl r8
    {
        CPU::ShiftBitsRight(*reg8_group[opcode - 0x38], true);
        break;
    }
    /* srl (HL) */
    case 0x3E:
    {
        uint8_t hl = CPU::ReadByte(HL);
        CPU::ShiftBitsRight(hl, true);
        CPU::WriteByte(HL, hl);
        break;
    }
    /* bit n,r8 */
    case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x47: // bit 0,r8
    case 0x48: case 0x49: case 0x4A: case 0x4B: case 0x4C: case 0x4D: case 0x4F: // bit 1,r8
    case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x57: // bit 2,r8
    case 0x58: case 0x59: case 0x5A: case 0x5B: case 0x5C: case 0x5D: case 0x5F: // bit 3,r8
    case 0x60: case 0x61: case 0x62: case 0x63: case 0x64: case 0x65: case 0x67: // bit 4,r8
    case 0x68: case 0x69: case 0x6A: case 0x6B: case 0x6C: case 0x6D: case 0x6F: // bit 5,r8
    case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x77: // bit 6,r8
    case 0x78: case 0x79: case 0x7A: case 0x7B: case 0x7C: case 0x7D: case 0x7F: // bit 7,r8
    {
        CPU::ModifyFlag(FLAG_Z, !GET_BIT(*reg8_group[opcode & 7], opcode >> 3 & 7));
        CPU::ModifyFlag(FLAG_N, 0);
        CPU::ModifyFlag(FLAG_H, 1);
        break;
    }
    /* bit n,(HL) */
    case 0x46: case 0x4E: case 0x56: case 0x5E: case 0x66: case 0x6E: case 0x76: case 0x7E:
    {
        uint8_t hl = CPU::ReadByte(HL);
        CPU::ModifyFlag(FLAG_Z, !GET_BIT(hl, (opcode - 0x46) >> 3));
        CPU::ModifyFlag(FLAG_N, 0);
        CPU::ModifyFlag(FLAG_H, 1);
        break;
    }
    /* set n,r8 */
    case 0xC0: case 0xC1: case 0xC2: case 0xC3: case 0xC4: case 0xC5: case 0xC7: // set 0,r8
    case 0xC8: case 0xC9: case 0xCA: case 0xCB: case 0xCC: case 0xCD: case 0xCF: // set 1,r8
    case 0xD0: case 0xD1: case 0xD2: case 0xD3: case 0xD4: case 0xD5: case 0xD7: // set 2,r8
    case 0xD8: case 0xD9: case 0xDA: case 0xDB: case 0xDC: case 0xDD: case 0xDF: // set 3,r8
    case 0xE0: case 0xE1: case 0xE2: case 0xE3: case 0xE4: case 0xE5: case 0xE7: // set 4,r8
    case 0xE8: case 0xE9: case 0xEA: case 0xEB: case 0xEC: case 0xED: case 0xEF: // set 5,r8
    case 0xF0: case 0xF1: case 0xF2: case 0xF3: case 0xF4: case 0xF5: case 0xF7: // set 6,r8
    case 0xF8: case 0xF9: case 0xFA: case 0xFB: case 0xFC: case 0xFD: case 0xFF: // set 7,r8
    {
        int idx = opcode & 7;
        uint8_t n = *reg8_group[idx];
        SET_BIT(n, opcode >> 3 & 7);
        *reg8_group[idx] = n;
        break;
    }
    /* set n,(HL) */
    case 0xC6: case 0xCE: case 0xD6: case 0xDE: case 0xE6: case 0xEE: case 0xF6: case 0xFE:
    {
        uint8_t hl = CPU::ReadByte(HL);
        SET_BIT(hl, (opcode - 0xC6) >> 3);
        CPU::WriteByte(HL, hl);
        break;
    }
    /* res n,r8 */
    case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: case 0x87: // res 0,r8
    case 0x88: case 0x89: case 0x8A: case 0x8B: case 0x8C: case 0x8D: case 0x8F: // res 1,r8
    case 0x90: case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x97: // res 2,r8
    case 0x98: case 0x99: case 0x9A: case 0x9B: case 0x9C: case 0x9D: case 0x9F: // res 3,r8
    case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA4: case 0xA5: case 0xA7: // res 4,r8
    case 0xA8: case 0xA9: case 0xAA: case 0xAB: case 0xAC: case 0xAD: case 0xAF: // res 5,r8
    case 0xB0: case 0xB1: case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB7: // res 6,r8
    case 0xB8: case 0xB9: case 0xBA: case 0xBB: case 0xBC: case 0xBD: case 0xBF: // res 7,r8
    {
        int idx = opcode & 7;
        uint8_t n = *reg8_group[idx];
        RES_BIT(n, opcode >> 3 & 7);
        *reg8_group[idx] = n;
        break;
    }
    /* res n,(HL) */
    case 0x86: case 0x8E: case 0x96: case 0x9E: case 0xA6: case 0xAE: case 0xB6: case 0xBE:
    {
        uint8_t hl = CPU::ReadByte(HL);
        RES_BIT(hl, (opcode - 0x86) >> 3);
        CPU::WriteByte(HL, hl);
        break;
    }
    }
}

void CPU::Step()
{
    // https://gbdev.io/pandocs/CPU_Instruction_Set.html
    // https://izik1.github.io/gbops/
    // http://marc.rawer.de/Gameboy/Docs/GBCPUman.pdf
    //
    // 頭いてェー
    uint8_t opcode;
    uint16_t old_pc;
    bool lock = false; // for halt bug

    m_Emulator->m_IntManager->InterruptRoutine();

    if (bLoggingEnabled)
    {
        CPU::Debug_LogState();
    }

    if (bHalted)
    {
        m_Emulator->Tick();
        return;
    }

    if (haltBug)
    {
        old_pc = PC;
    }
    
    if (ei_delay_cnt > 0)
    {
        if (--ei_delay_cnt == 0)
        {
            m_Emulator->m_IntManager->EnableIME();
        }
    }

    switch (opcode = CPU::NextByte())
    {
    /**
     *
     * 8-bit Loads
     *
     **/
    /* ld r8,r8 ; 4c */
    case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x47: // ld b,r8
    case 0x48: case 0x49: case 0x4A: case 0x4B: case 0x4C: case 0x4D: case 0x4F: // ld c,r8
    case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x57: // ld d,r8
    case 0x58: case 0x59: case 0x5A: case 0x5B: case 0x5C: case 0x5D: case 0x5F: // ld e,r8
    case 0x60: case 0x61: case 0x62: case 0x63: case 0x64: case 0x65: case 0x67: // ld h,r8
    case 0x68: case 0x69: case 0x6A: case 0x6B: case 0x6C: case 0x6D: case 0x6F: // ld l,r8
    case 0x78: case 0x79: case 0x7A: case 0x7B: case 0x7C: case 0x7D: case 0x7F: // ld a,r8
    {
        *reg8_group[opcode >> 3 & 7] = *reg8_group[opcode & 7];
        break;
    }
    /* ld r8,n ; 8c */
    case 0x06: case 0x0E: case 0x16: case 0x1E: case 0x26: case 0x2E: case 0x3E: // ld r8,n
    {
        *reg8_group[(opcode - 6) >> 3] = CPU::NextByte();
        break;
    }
    /* ld r8,(HL) ; 8c */
    case 0x46: case 0x4E: case 0x56: case 0x5E: case 0x66: case 0x6E: case 0x7E: // ld r8,(HL)
    {
        *reg8_group[(opcode - 70) >> 3] = CPU::ReadByte(HL);
        break;
    }
    /* ld (HL),r8 ; 8c */
    case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x77: // ld (HL),r8
    {
        CPU::WriteByte(HL, *reg8_group[opcode - 0x70]);
        break;
    }
    /* ld (HL),n ; 12c */
    case 0x36:
    {
        CPU::WriteByte(HL, CPU::NextByte());
        break;
    }
    /* ld A,(r16-g2) ; 8c */
    case 0x0A: case 0x1A: case 0x2A: case 0x3A: // ld A,(r16-g2)
    {
        int idx = (opcode - 0x0A) >> 4;
        A = CPU::ReadByte(*reg16_group2[idx]);
        HL += HL_add[idx];
        break;
    }
    /* ld A,(nn) ; 16c */
    case 0xFA:
    {
        A = CPU::ReadByte(CPU::NextWord());
        break;
    }
    /* ld (r16-g2),A ; 8c */
    case 0x02: case 0x12: case 0x22: case 0x32: // ld (r16-g2),A
    {
        int idx = (opcode - 0x02) >> 4;
        CPU::WriteByte(*reg16_group2[idx], A);
        HL += HL_add[idx];
        break;
    }
    /* ld (nn),A ; 16c */
    case 0xEA:
    {
        CPU::WriteByte(CPU::NextWord(), A);
        break;
    }
    /* ld A,(FF00+n) ; 12c */
    case 0xF0:
    {
        A = CPU::ReadByte(0xFF00 + CPU::NextByte());
        break;
    }
    /* ld (FF00+n),A ; 12c */
    case 0xE0:
    {
        CPU::WriteByte(0xFF00 + CPU::NextByte(), A);
        break;
    }
    /* ld A,(FF00+C) ; 8c */
    case 0xF2:
    {
        A = CPU::ReadByte(0xFF00 + reg8[0]);
        break;
    }
    /* ld (FF00+C),A ; 8c */
    case 0xE2:
    {
        CPU::WriteByte(0xFF00 + reg8[0], A);
        break;
    }
    /**
     *
     * 16-bit Loads
     *
     **/
    /* ld r16-g1,nn ; 12c */
    case 0x01: case 0x11: case 0x21: case 0x31: // ld r16-g1,nn
    {
        *reg16_group1[(opcode - 0x01) >> 4] = CPU::NextWord();
        break;
    }
    /* ld (nn),SP ; 20c */
    case 0x08:
    {
        CPU::WriteWord(CPU::NextWord(), SP);
        break;
    }
    /* ld SP,HL ; 8c */
    case 0xF9:
    {
        SP = HL;
        m_Emulator->Tick(); // internal
        break;
    }
    /* push r16-g3 ; 16c */
    case 0xC5: case 0xD5: case 0xE5: case 0xF5: // push r16-g3
    {
        CPU::Push(*reg16_group3[(opcode - 0xC5) >> 4]);
        m_Emulator->Tick(); // internal
        break;
    }
    /* pop r16-g3 ; 12c */
    case 0xC1: case 0xD1: case 0xE1: case 0xF1: // pop r16-g3
    {
        *reg16_group3[(opcode - 0xC1) >> 4] = CPU::Pop();
        if (opcode == 0xF1) F &= 0xF0; // the lower nibble of the flag register should stay untouched after popping
        break;
    }
    /**
     *
     * 8-bit Arithmetic/Logic instructions
     *
     **/
    /* (add / adc) A,r8 ; 4c */
    case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: case 0x87: // add A,r8
    case 0x88: case 0x89: case 0x8A: case 0x8B: case 0x8C: case 0x8D: case 0x8F: // adc A,r8
    {
        uint8_t r8 = *reg8_group[opcode - (opcode <= 0x87 ? 0x80 : 0x88)];
        int cy = opcode > 0x87 ? CPU::IsFlagSet(FLAG_C) : 0;
        CPU::ModifyFlag(FLAG_H, HC8(A, r8, cy));
        CPU::ModifyFlag(FLAG_C, CARRY8(A, r8, cy));
        A += r8 + cy;
        CPU::ModifyFlag(FLAG_Z, A == 0);
        CPU::ModifyFlag(FLAG_N, 0);
        break;
    }
    /* (add / adc) A,n ; 8c */
    case 0xC6: case 0xCE:
    {
        uint8_t n = CPU::NextByte();
        int cy = opcode == 0xCE ? CPU::IsFlagSet(FLAG_C) : 0;
        CPU::ModifyFlag(FLAG_H, HC8(A, n, cy));
        CPU::ModifyFlag(FLAG_C, CARRY8(A, n, cy));
        A += n + cy;
        CPU::ModifyFlag(FLAG_Z, A == 0);
        CPU::ModifyFlag(FLAG_N, 0);
        break;
    }
    /* (add / adc) A,(HL) ; 8c */
    case 0x86: case 0x8E:
    {
        uint8_t hl = CPU::ReadByte(HL);
        int cy = opcode == 0x8E ? CPU::IsFlagSet(FLAG_C) : 0;
        CPU::ModifyFlag(FLAG_H, HC8(A, hl, cy));
        CPU::ModifyFlag(FLAG_C, CARRY8(A, hl, cy));
        A += hl + cy;
        CPU::ModifyFlag(FLAG_Z, A == 0);
        CPU::ModifyFlag(FLAG_N, 0);
        break;
    }
    /* (sub / sbc) A,r8 ; 4c */
    case 0x90: case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x97: // sub r8
    case 0x98: case 0x99: case 0x9A: case 0x9B: case 0x9C: case 0x9D: case 0x9F: // sbc A,r8
    {
        uint8_t r8 = *reg8_group[opcode - (opcode <= 0x97 ? 0x90 : 0x98)];
        int cy = opcode > 0x97 ? CPU::IsFlagSet(FLAG_C) : 0;
        CPU::ModifyFlag(FLAG_H, HC8_SUB(A, r8, cy));
        CPU::ModifyFlag(FLAG_C, CARRY8_SUB(A, r8, cy));
        A -= r8 + cy;
        CPU::ModifyFlag(FLAG_Z, A == 0);
        CPU::ModifyFlag(FLAG_N, 1);
        break;
    }
    /* (sub / sbc) A,n ; 8c */
    case 0xD6: case 0xDE:
    {
        uint8_t n = CPU::NextByte();
        int cy = opcode == 0xDE ? CPU::IsFlagSet(FLAG_C) : 0;
        CPU::ModifyFlag(FLAG_H, HC8_SUB(A, n, cy));
        CPU::ModifyFlag(FLAG_C, CARRY8_SUB(A, n, cy));
        A -= n + cy;
        CPU::ModifyFlag(FLAG_Z, A == 0);
        CPU::ModifyFlag(FLAG_N, 1);
        break;
    }
    /* (sub / sbc) A,(HL) ; 8c */
    case 0x96: case 0x9E:
    {
        uint8_t hl = CPU::ReadByte(HL);
        int cy = opcode == 0x9E ? CPU::IsFlagSet(FLAG_C) : 0;
        CPU::ModifyFlag(FLAG_H, HC8_SUB(A, hl, cy));
        CPU::ModifyFlag(FLAG_C, CARRY8_SUB(A, hl, cy));
        A -= hl + cy;
        CPU::ModifyFlag(FLAG_Z, A == 0);
        CPU::ModifyFlag(FLAG_N, 1);
        break;
    }
    /* and r8 ; 4c */
    case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA4: case 0xA5: case 0xA7: // and r8
    {
        A &= *reg8_group[opcode - 0xA0];
        CPU::ModifyFlag(FLAG_Z, A == 0);
        CPU::ModifyFlag(FLAG_N, 0);
        CPU::ModifyFlag(FLAG_H, 1);
        CPU::ModifyFlag(FLAG_C, 0);
        break;
    }
    /* and n ; 8c */
    case 0xE6:
    {
        A &= CPU::NextByte();
        CPU::ModifyFlag(FLAG_Z, A == 0);
        CPU::ModifyFlag(FLAG_N, 0);
        CPU::ModifyFlag(FLAG_H, 1);
        CPU::ModifyFlag(FLAG_C, 0);
        break;
    }
    /* and (HL) ; 8c */
    case 0xA6:
    {
        A &= CPU::ReadByte(HL);
        CPU::ModifyFlag(FLAG_Z, A == 0);
        CPU::ModifyFlag(FLAG_N, 0);
        CPU::ModifyFlag(FLAG_H, 1);
        CPU::ModifyFlag(FLAG_C, 0);
        break;
    }
    /* xor r8 ; 4c */
    case 0xA8: case 0xA9: case 0xAA: case 0xAB: case 0xAC: case 0xAD: case 0xAF: // xor r8
    {
        A ^= *reg8_group[opcode - 0xA8];
        CPU::ModifyFlag(FLAG_Z, A == 0);
        CPU::ModifyFlag(FLAG_N, 0);
        CPU::ModifyFlag(FLAG_H, 0);
        CPU::ModifyFlag(FLAG_C, 0);
        break;
    }
    /* xor n ; 8c */
    case 0xEE:
    {
        A ^= CPU::NextByte();
        CPU::ModifyFlag(FLAG_Z, A == 0);
        CPU::ModifyFlag(FLAG_N, 0);
        CPU::ModifyFlag(FLAG_H, 0);
        CPU::ModifyFlag(FLAG_C, 0);
        break;
    }
    /* xor (HL) ; 8c */
    case 0xAE:
    {
        A ^= CPU::ReadByte(HL);
        CPU::ModifyFlag(FLAG_Z, A == 0);
        CPU::ModifyFlag(FLAG_N, 0);
        CPU::ModifyFlag(FLAG_H, 0);
        CPU::ModifyFlag(FLAG_C, 0);
        break;
    }
    /* or r8 ; 4c */
    case 0xB0: case 0xB1: case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB7: // or r8
    {
        A |= *reg8_group[opcode - 0xB0];
        CPU::ModifyFlag(FLAG_Z, A == 0);
        CPU::ModifyFlag(FLAG_N, 0);
        CPU::ModifyFlag(FLAG_H, 0);
        CPU::ModifyFlag(FLAG_C, 0);
        break;
    }
    /* or n ; 8c */
    case 0xF6:
    {
        A |= CPU::NextByte();
        CPU::ModifyFlag(FLAG_Z, A == 0);
        CPU::ModifyFlag(FLAG_N, 0);
        CPU::ModifyFlag(FLAG_H, 0);
        CPU::ModifyFlag(FLAG_C, 0);
        break;
    }
    /* or (HL) ; 8c */
    case 0xB6:
    {
        A |= CPU::ReadByte(HL);
        CPU::ModifyFlag(FLAG_Z, A == 0);
        CPU::ModifyFlag(FLAG_N, 0);
        CPU::ModifyFlag(FLAG_H, 0);
        CPU::ModifyFlag(FLAG_C, 0);
        break;
    }
    /* cp r8 ; 4c */
    case 0xB8: case 0xB9: case 0xBA: case 0xBB: case 0xBC: case 0xBD: case 0xBF: // cp r8
    {
        uint8_t r8 = *reg8_group[opcode - 0xB8];
        CPU::ModifyFlag(FLAG_H, HC8_SUB(A, r8, 0));
        CPU::ModifyFlag(FLAG_C, CARRY8_SUB(A, r8, 0));
        CPU::ModifyFlag(FLAG_Z, A == r8);
        CPU::ModifyFlag(FLAG_N, 1);
        break;
    }
    /* cp n ; 8c */
    case 0xFE:
    {
        uint8_t n = CPU::NextByte();
        CPU::ModifyFlag(FLAG_H, HC8_SUB(A, n, 0));
        CPU::ModifyFlag(FLAG_C, CARRY8_SUB(A, n, 0));
        CPU::ModifyFlag(FLAG_Z, A == n);
        CPU::ModifyFlag(FLAG_N, 1);
        break;
    }
    /* cp (HL) ; 8c */
    case 0xBE:
    {
        uint8_t hl = CPU::ReadByte(HL);
        CPU::ModifyFlag(FLAG_H, HC8_SUB(A, hl, 0));
        CPU::ModifyFlag(FLAG_C, CARRY8_SUB(A, hl, 0));
        CPU::ModifyFlag(FLAG_Z, A == hl);
        CPU::ModifyFlag(FLAG_N, 1);
        break;
    }
    /* inc r8 ; 4c */
    case 0x04: case 0x0C: case 0x14: case 0x1C: case 0x24: case 0x2C: case 0x3C: // inc r8
    {
        int idx = (opcode - 0x04) >> 3;
        uint8_t val = *reg8_group[idx];
        CPU::ModifyFlag(FLAG_H, HC8(val, 1, 0));
        val += 1;
        CPU::ModifyFlag(FLAG_Z, val == 0);
        *reg8_group[idx] = val;
        CPU::ModifyFlag(FLAG_N, 0);
        break;
    }
    /* inc (HL) ; 12c */
    case 0x34:
    {
        uint8_t hl = CPU::ReadByte(HL);
        CPU::ModifyFlag(FLAG_H, HC8(hl, 1, 0));
        hl += 1;
        CPU::ModifyFlag(FLAG_Z, hl == 0);
        CPU::WriteByte(HL, hl);
        CPU::ModifyFlag(FLAG_N, 0);
        break;
    }
    /* dec r8 ; 4c */
    case 0x05: case 0x0D: case 0x15: case 0x1D: case 0x25: case 0x2D: case 0x3D: // dec r8
    {
        int idx = (opcode - 0x05) >> 3;
        uint8_t val = *reg8_group[idx];
        CPU::ModifyFlag(FLAG_H, HC8_SUB(val, 1, 0));
        val -= 1;
        CPU::ModifyFlag(FLAG_Z, val == 0);
        *reg8_group[idx] = val;
        CPU::ModifyFlag(FLAG_N, 1);
        break;
    }
    /* dec (HL) ; 12c */
    case 0x35:
    {
        uint8_t hl = CPU::ReadByte(HL);
        CPU::ModifyFlag(FLAG_H, HC8_SUB(hl, 1, 0));
        hl -= 1;
        CPU::ModifyFlag(FLAG_Z, hl == 0);
        CPU::WriteByte(HL, hl);
        CPU::ModifyFlag(FLAG_N, 1);
        break;
    }
    /* daa ; 4c */
    case 0x27: // You should try implementing yourself without copying other people's code...
    {
        if (!CPU::IsFlagSet(FLAG_N))
        {
            // after an addition, adjust if (half-)carry occurred or if result is out of bounds
            if (CPU::IsFlagSet(FLAG_C) || A > 0x99)
            {
                A += 0x60;
                CPU::ModifyFlag(FLAG_C, 1);
            }

            if (CPU::IsFlagSet(FLAG_H) || (A & 0xF) > 0x9)
            {
                A += 0x6;
            }
        }
        else
        {
            // after a subtraction, only adjust if (half-)carry occurred
            if (CPU::IsFlagSet(FLAG_C))
            {
                A -= 0x60;
                CPU::ModifyFlag(FLAG_C, 1);
            }

            if (CPU::IsFlagSet(FLAG_H))
            {
                A -= 0x6;
            }
        }

        CPU::ModifyFlag(FLAG_Z, A == 0);
        CPU::ModifyFlag(FLAG_H, 0);

        break;
    }
    /* cpl ; 4c */
    case 0x2F:
    {
        A ^= 0xFF;
        CPU::ModifyFlag(FLAG_N, 1);
        CPU::ModifyFlag(FLAG_H, 1);
        break;
    }
    /**
     *
     * 16-bit Arithmetic/Logic instructions
     *
     **/
    /* add HL,r16-g1 ; 8c */
    case 0x09: case 0x19: case 0x29: case 0x39: // add HL,r16-g1
    {
        uint16_t val = *reg16_group1[(opcode - 0x09) >> 4];
        CPU::ModifyFlag(FLAG_H, HC16(HL, val));
        CPU::ModifyFlag(FLAG_C, CARRY16(HL, val));
        HL += val;
        CPU::ModifyFlag(FLAG_N, 0);
        m_Emulator->Tick(); // internal
        break;
    }
    /* inc r16-g1 ; 8c */
    case 0x03: case 0x13: case 0x23: case 0x33: // inc r16-g1
    {
        *reg16_group1[(opcode - 0x03) >> 4] += 1;
        m_Emulator->Tick(); // internal
        break;
    }
    /* dec r16-g1 ; 8c */
    case 0x0B: case 0x1B: case 0x2B: case 0x3B:
    {
        *reg16_group1[(opcode - 0x0B) >> 4] -= 1;
        m_Emulator->Tick(); // internal
        break;
    }
    /* add SP,dd ; 16c */
    case 0xE8:
    {
        int8_t dd = static_cast<int8_t>(CPU::NextByte());
        CPU::ModifyFlag(FLAG_H, HC8(SP, dd, 0));
        CPU::ModifyFlag(FLAG_C, CARRY8(SP, dd, 0));
        SP += dd;
        CPU::ModifyFlag(FLAG_Z, 0);
        CPU::ModifyFlag(FLAG_N, 0);
        m_Emulator->Tick(); // internal
        m_Emulator->Tick(); // write
        break;
    }
    /* ld HL,SP+dd ; 12c */
    case 0xF8:
    {
        int8_t dd = static_cast<int8_t>(CPU::NextByte());
        CPU::ModifyFlag(FLAG_H, HC8(SP, dd, 0));
        CPU::ModifyFlag(FLAG_C, CARRY8(SP, dd, 0));
        HL = SP + dd;
        CPU::ModifyFlag(FLAG_Z, 0);
        CPU::ModifyFlag(FLAG_N, 0);
        m_Emulator->Tick(); // internal
        break;
    }
    /**
     *
     * Rotate and Shift instructions
     *
     **/
    /* rl(c)a ; 4c */
    case 0x07: case 0x17:
    {
        CPU::RotateBitsLeft(A, opcode == 0x07, true);
        break;
    }
    /* rr(c)a ; 4c */
    case 0x0F: case 0x1F:
    {
        CPU::RotateBitsRight(A, opcode == 0x0F, true);
        break;
    }
    case 0xCB:
    {
        CPU::HandlePrefixCB();
        break;
    }
    /**
     *
     * CPU Control instructions
     *
     **/
    /* ccf ; 4c */
    case 0x3F:
    {
        CPU::ModifyFlag(FLAG_C, !CPU::IsFlagSet(FLAG_C));
        CPU::ModifyFlag(FLAG_N, 0);
        CPU::ModifyFlag(FLAG_H, 0);
        break;
    }
    /* scf ; 4c */
    case 0x37:
    {
        CPU::ModifyFlag(FLAG_C, 1);
        CPU::ModifyFlag(FLAG_N, 0);
        CPU::ModifyFlag(FLAG_H, 0);
        break;
    }
    /* nop ; 4c */
    case 0x0: break;
    /* halt ; N*4c */
    case 0x76:
    {
        if (m_Emulator->m_IntManager->SetHaltMode())
        {
            bHalted = true;
        }
        else
        {
            haltBug = true;
            lock = true;
        }
        break;
    }
    /* stop ; ??c */
    case 0x10: // TODO
    /* di ; 4c */
    case 0xF3:
    {
        if (ei_delay_cnt > 0)
        {
            ei_delay_cnt = 0;
        }
        else
        {
            m_Emulator->m_IntManager->DisableIME();
        }
        break;
    }
    /* ei ; 4c */
    case 0xFB:
    {
        if (!m_Emulator->m_IntManager->GetIME() && ei_delay_cnt == 0)
            ei_delay_cnt = 2; // delayed by one instruction (will be 0 in next two cpu steps)
        break;
    }
    /**
     *
     * Jump instructions
     *
     **/
    /* jp nn ; 16c */
    case 0xC3:
    {
        PC = CPU::NextWord();
        m_Emulator->Tick(); // internal
        break;
    }
    /* jp HL ; 4c */
    case 0xE9:
    {
        PC = HL;
        break;
    }
    /* jp f,nn ; 16c/12c */
    case 0xC2: case 0xCA: case 0xD2: case 0xDA: // jp f,nn
    {
        uint16_t nn = CPU::NextWord();
        if (CPU::Condition((opcode - 0xC2) >> 3))
        {
            PC = nn;
        }
        break;
    }
    /* jr PC+dd ; 12c */
    case 0x18:
    {
        int8_t dd = static_cast<int8_t>(CPU::NextByte());
        PC += dd;
        m_Emulator->Tick(); // internal
        break;
    }
    /* jr f,PC+dd ; 12c/8c */
    case 0x20: case 0x28: case 0x30: case 0x38: // jr f,PC+dd
    {
        int8_t dd = static_cast<int8_t>(CPU::NextByte());
        if (CPU::Condition((opcode - 0x20) >> 3))
        {
            PC += dd;
        }
        break;
    }
    /* call nn ; 24c */
    case 0xCD:
    {
        CPU::Call(CPU::NextWord());
        m_Emulator->Tick(); // internal
        break;
    }
    /* call f,nn ; 24c/12c */
    case 0xC4: case 0xCC: case 0xD4: case 0xDC: // call f,nn
    {
        uint16_t nn = CPU::NextWord();
        if (CPU::Condition((opcode - 0xC4) >> 3))
        {
            CPU::Call(nn);
        }
        break;
    }
    /* ret ; 16c */
    case 0xC9:
    {
        PC = CPU::Pop();
        m_Emulator->Tick(); // internal
        break;
    }
    /* ret f ; 20c/8c */
    case 0xC0: case 0xC8: case 0xD0: case 0xD8: // ret f
    {
        if (CPU::Condition((opcode - 0xC0) >> 3))
        {
            PC = CPU::Pop();
            m_Emulator->Tick(); // set
        }
        else
        {
            m_Emulator->Tick(); // internal
        }
        break;
    }
    /* reti ; 16c */
    case 0xD9:
    {
        PC = CPU::Pop(); /* RET */
        m_Emulator->Tick(); // internal
        m_Emulator->m_IntManager->EnableIME(); /* EI */
        break;
    }
    /* rst n ; 16c */
    case 0xC7: case 0xCF: case 0xD7: case 0xDF: case 0xE7: case 0xEF: case 0xF7: case 0xFF:
    {
        CPU::Call(RST_ADDR[(opcode - 0xC7) >> 3]);
        m_Emulator->Tick(); // internal
        break;
    }
    default:
    {
        m_Emulator->m_EmulatorLogger->DoLog(LOG_ERROR, "CPU::Step", "Unknown opcode: ${0:02X}", opcode);
        throw std::runtime_error("Opcode unimplemented!");
    }
    }

    if (!lock && haltBug)
    {
        PC = old_pc;
        haltBug = false;
        m_Emulator->m_IntManager->ResetHaltMode();
    }
}