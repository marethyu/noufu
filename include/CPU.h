#ifndef _CPU_H_
#define _CPU_H_

#include <memory>

#include "GBComponent.h"

const int FLAG_Z = 7;
const int FLAG_N = 6;
const int FLAG_H = 5;
const int FLAG_C = 4;

class CPU : public GBComponent
{
private:
    // reg8 is arranged in this order (works for little-endian machines):
    // register - index
    // C        - 0
    // B        - 1
    // E        - 2
    // D        - 3
    // L        - 4
    // H        - 5
    // F        - 6
    // A        - 7
    // SP Low   - 8
    // SP High  - 9
    uint8_t reg8[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    uint8_t &A = reg8[7];
    uint8_t &F = reg8[6];
    uint8_t *reg8_group[8] = {reg8 + 1, reg8, reg8 + 3, reg8 + 2, reg8 + 5, reg8 + 4, &F, &A}; // order: B, C, D, E, H, L, F, A

    uint16_t *reg16 = (uint16_t *) reg8;
    uint16_t &HL = reg16[2];
    uint16_t &SP = reg16[4];
    uint16_t PC;
    uint16_t *reg16_group1[4] = {reg16, reg16 + 1, &HL, &SP};      // order: BC, DE, HL,  SP
    uint16_t *reg16_group2[4] = {reg16, reg16 + 1, &HL, &HL};      // order: BC, DE, HL+, HL-
    uint16_t *reg16_group3[4] = {reg16, reg16 + 1, &HL, reg16 + 3};// order: BC, DE, HL,  AF

    int ei_delay_cnt; // counter for ei instruction delay

    bool bHalted;
    bool haltBug;

    Emulator *m_Emulator;

    bool bLoggingEnabled;

    uint8_t ReadByte(uint16_t address) const;
    uint16_t ReadWord(uint16_t address) const;
    void WriteByte(uint16_t address, uint8_t data);
    void WriteWord(uint16_t address, uint16_t data);
    uint8_t NextByte();
    uint16_t NextWord();

    bool IsFlagSet(int flag);
    void ModifyFlag(int flag, int value); // value is either 1 or 0
    void Push(uint16_t data);
    uint16_t Pop();
    bool Condition(int i);
    void Call(uint16_t addr);
    // See page 109 in https://ia803208.us.archive.org/9/items/GameBoyProgManVer1.1/GameBoyProgManVer1.1.pdf for more info
    // TL;DR:
    // - RLC = bit 7 goes into both bit 0 (hence circular) and carry-out, and the old carry-in value is ignored completely
    // - RL = bit 7 goes into carry-out, carry-in goes into bit 0. If you separate carry in/out, it's effectively just a big left shift
    void RotateBitsLeft(uint8_t &n, bool circular=false, bool resetZ=false);
    void RotateBitsRight(uint8_t &n, bool circular=false, bool resetZ=false);
    void ShiftBitsLeft(uint8_t &n);
    void ShiftBitsRight(uint8_t &n, bool logical=false);
    void HandlePrefixCB();
public:
    CPU(Emulator *emu);
    ~CPU();

    void Init();
    void Reset();
    void Step(); // step a M-cycle
    void HandleInterrupt(int i);
    bool isHalted();
    void Resume();
    uint16_t GetPC() { return PC; }

    void Debug_PrintStatus();
    void Debug_PrintCurrentInstruction();
    void Debug_EditRegister(int reg, int val, bool is8Bit);
    void Debug_EditFlag(int flag, int val);

    // https://github.com/wheremyfoodat/Gameboy-logs用
    // Format: [registers] (mem[pc] mem[pc+1] mem[pc+2] mem[pc+3])
    // Ex: A: 01 F: B0 B: 00 C: 13 D: 00 E: D8 H: 01 L: 4D SP: FFFE PC: 00:0100 (00 C3 13 02)
    void Debug_LogState();
};

#endif