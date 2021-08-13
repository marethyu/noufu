#include <algorithm>
#include <array>
#include <bitset>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <sstream>
#include <string>
#include <vector>

#include <cstdio>
#include <ctime>

#include <Windows.h>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#include "loguru.hpp"

#define SCREEN_WIDTH 160
#define SCREEN_HEIGHT 144

#define ID_LOAD_ROM 0
#define ID_RELOAD_ROM 1
#define ID_EXIT 2
#define ID_ABOUT 3

#define ID_TIMER 1

const std::string ROM_FILE_PATH = "../ROMS/blargg_tests/instr_timing/instr_timing.gb";

const std::string EMULATOR_NAME = "Noufu";

const std::string CONFIG_FILE_PATH = "config";

// Maximum number of cycles per update
const int MAX_CYCLES = 70224; // 154 scanlines * 456 cycles per frame = 70224

const int DOTS_PER_SCANLINE = 456;

// Delay between updates
const int UPDATE_INTERVAL = 13; // 1000 ms / 59.72 fps = 16.744 (adjusted for win32 timer)

const int HL_add[] = {0, 0, 1, -1};
const int RST_ADDR[] = {0x00, 0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38};

const int FLAG_Z = 7;
const int FLAG_N = 6;
const int FLAG_H = 5;
const int FLAG_C = 4;

#define HC8(A, B, cy) ((((A) & 0x0F) + ((B) & 0x0F) + (cy)) > 0x0F)
#define CARRY8(A, B, cy) ((((A) & 0xFF) + ((B) & 0xFF) + (cy)) >= 0x100)
/* Read this https://stackoverflow.com/questions/57958631/ for information on 16-bit HC */
#define HC16(A, B) ((((A) & 0xFFF) + ((B) & 0xFFF)) > 0xFFF)
#define CARRY16(A, B) (((A) + (B)) >= 0x10000)
#define HC8_SUB(A, B, cy) ((((A) & 0x0F) - ((B) & 0x0F) - (cy)) < 0)
#define CARRY8_SUB(A, B, cy) ((((A) & 0xFF) - ((B) & 0xFF) - (cy)) < 0)

#define TEST_BIT(n, p) (n & (1 << (p)))
#define SET_BIT(n, p) do { n |= (1 << (p)); } while (0)
#define RES_BIT(n, p) do { n &= ~(1 << (p)); } while (0)
#define SWAP_NIBBLES8(n) do { n = ((n & 0x0F) << 4) | (n >> 4); } while (0)
#define GET_BIT(n, p) (TEST_BIT(n, p) != 0)

const std::string reg8_str[8] = {
    "C",
    "B",
    "E",
    "D",
    "L",
    "H",
    "F",
    "A"
};

const std::string reg16_str[6] = {
    "BC",
    "DE",
    "HL",
    "AF",
    "SP",
    "PC"
};

const std::string flag_str[4] = {
    "C",
    "H",
    "N",
    "Z"
};

enum
{
    ARG_NONE = 0,
    ARG_U8,
    ARG_I8,
    ARG_U16
};

struct OpcodeInfo
{
    std::string fmt_str;
    int arg_type;
};

const OpcodeInfo opcode_info[256] = {
    {"NOP", ARG_NONE},
    {"LD BC,%04X", ARG_U16},
    {"LD (BC),A", ARG_NONE},
    {"INC BC", ARG_NONE},
    {"INC B", ARG_NONE},
    {"DEC B", ARG_NONE},
    {"LD B,%02X", ARG_U8},
    {"RLCA", ARG_NONE},
    {"LD (%04X),SP", ARG_U16},
    {"ADD HL,BC", ARG_NONE},
    {"LD A,(BC)", ARG_NONE},
    {"DEC BC", ARG_NONE},
    {"INC C", ARG_NONE},
    {"DEC C", ARG_NONE},
    {"LD C,%02X", ARG_U8},
    {"RRCA", ARG_NONE},
    {"STOP", ARG_NONE},
    {"LD DE,%04X", ARG_U16},
    {"LD (DE),A", ARG_NONE},
    {"INC DE", ARG_NONE},
    {"INC D", ARG_NONE},
    {"DEC D", ARG_NONE},
    {"LD D,%02X", ARG_U8},
    {"RLA", ARG_NONE},
    {"JR %02X", ARG_I8},
    {"ADD HL,DE", ARG_NONE},
    {"LD A,(DE)", ARG_NONE},
    {"DEC DE", ARG_NONE},
    {"INC E", ARG_NONE},
    {"DEC E", ARG_NONE},
    {"LD E,%02X", ARG_U8},
    {"RRA", ARG_NONE},
    {"JR NZ,%02X", ARG_I8},
    {"LD HL,%04X", ARG_U16},
    {"LD (HL+),A", ARG_NONE},
    {"INC HL", ARG_NONE},
    {"INC H", ARG_NONE},
    {"DEC H", ARG_NONE},
    {"LD H,%02X", ARG_U8},
    {"DAA", ARG_NONE},
    {"JR Z,%02X", ARG_I8},
    {"ADD HL,HL", ARG_NONE},
    {"LD A,(HL+)", ARG_NONE},
    {"DEC HL", ARG_NONE},
    {"INC L", ARG_NONE},
    {"DEC L", ARG_NONE},
    {"LD L,%02X", ARG_U8},
    {"CPL", ARG_NONE},
    {"JR NC,%02X", ARG_I8},
    {"LD SP,%04X", ARG_U16},
    {"LD (HL-),A", ARG_NONE},
    {"INC SP", ARG_NONE},
    {"INC (HL)", ARG_NONE},
    {"DEC (HL)", ARG_NONE},
    {"LD (HL),%02X", ARG_U8},
    {"SCF", ARG_NONE},
    {"JR C,%02X", ARG_I8},
    {"ADD HL,SP", ARG_NONE},
    {"LD A,(HL-)", ARG_NONE},
    {"DEC SP", ARG_NONE},
    {"INC A", ARG_NONE},
    {"DEC A", ARG_NONE},
    {"LD A,%02X", ARG_U8},
    {"CCF", ARG_NONE},
    {"LD B,B", ARG_NONE},
    {"LD B,C", ARG_NONE},
    {"LD B,D", ARG_NONE},
    {"LD B,E", ARG_NONE},
    {"LD B,H", ARG_NONE},
    {"LD B,L", ARG_NONE},
    {"LD B,(HL)", ARG_NONE},
    {"LD B,A", ARG_NONE},
    {"LD C,B", ARG_NONE},
    {"LD C,C", ARG_NONE},
    {"LD C,D", ARG_NONE},
    {"LD C,E", ARG_NONE},
    {"LD C,H", ARG_NONE},
    {"LD C,L", ARG_NONE},
    {"LD C,(HL)", ARG_NONE},
    {"LD C,A", ARG_NONE},
    {"LD D,B", ARG_NONE},
    {"LD D,C", ARG_NONE},
    {"LD D,D", ARG_NONE},
    {"LD D,E", ARG_NONE},
    {"LD D,H", ARG_NONE},
    {"LD D,L", ARG_NONE},
    {"LD D,(HL)", ARG_NONE},
    {"LD D,A", ARG_NONE},
    {"LD E,B", ARG_NONE},
    {"LD E,C", ARG_NONE},
    {"LD E,D", ARG_NONE},
    {"LD E,E", ARG_NONE},
    {"LD E,H", ARG_NONE},
    {"LD E,L", ARG_NONE},
    {"LD E,(HL)", ARG_NONE},
    {"LD E,A", ARG_NONE},
    {"LD H,B", ARG_NONE},
    {"LD H,C", ARG_NONE},
    {"LD H,D", ARG_NONE},
    {"LD H,E", ARG_NONE},
    {"LD H,H", ARG_NONE},
    {"LD H,L", ARG_NONE},
    {"LD H,(HL)", ARG_NONE},
    {"LD H,A", ARG_NONE},
    {"LD L,B", ARG_NONE},
    {"LD L,C", ARG_NONE},
    {"LD L,D", ARG_NONE},
    {"LD L,E", ARG_NONE},
    {"LD L,H", ARG_NONE},
    {"LD L,L", ARG_NONE},
    {"LD L,(HL)", ARG_NONE},
    {"LD L,A", ARG_NONE},
    {"LD (HL),B", ARG_NONE},
    {"LD (HL),C", ARG_NONE},
    {"LD (HL),D", ARG_NONE},
    {"LD (HL),E", ARG_NONE},
    {"LD (HL),H", ARG_NONE},
    {"LD (HL),L", ARG_NONE},
    {"HALT", ARG_NONE},
    {"LD (HL),A", ARG_NONE},
    {"LD A,B", ARG_NONE},
    {"LD A,C", ARG_NONE},
    {"LD A,D", ARG_NONE},
    {"LD A,E", ARG_NONE},
    {"LD A,H", ARG_NONE},
    {"LD A,L", ARG_NONE},
    {"LD A,(HL)", ARG_NONE},
    {"LD A,A", ARG_NONE},
    {"ADD A,B", ARG_NONE},
    {"ADD A,C", ARG_NONE},
    {"ADD A,D", ARG_NONE},
    {"ADD A,E", ARG_NONE},
    {"ADD A,H", ARG_NONE},
    {"ADD A,L", ARG_NONE},
    {"ADD A,(HL)", ARG_NONE},
    {"ADD A,A", ARG_NONE},
    {"ADC A,B", ARG_NONE},
    {"ADC A,C", ARG_NONE},
    {"ADC A,D", ARG_NONE},
    {"ADC A,E", ARG_NONE},
    {"ADC A,H", ARG_NONE},
    {"ADC A,L", ARG_NONE},
    {"ADC A,(HL)", ARG_NONE},
    {"ADC A,A", ARG_NONE},
    {"SUB A,B", ARG_NONE},
    {"SUB A,C", ARG_NONE},
    {"SUB A,D", ARG_NONE},
    {"SUB A,E", ARG_NONE},
    {"SUB A,H", ARG_NONE},
    {"SUB A,L", ARG_NONE},
    {"SUB A,(HL)", ARG_NONE},
    {"SUB A,A", ARG_NONE},
    {"SBC A,B", ARG_NONE},
    {"SBC A,C", ARG_NONE},
    {"SBC A,D", ARG_NONE},
    {"SBC A,E", ARG_NONE},
    {"SBC A,H", ARG_NONE},
    {"SBC A,L", ARG_NONE},
    {"SBC A,(HL)", ARG_NONE},
    {"SBC A,A", ARG_NONE},
    {"AND A,B", ARG_NONE},
    {"AND A,C", ARG_NONE},
    {"AND A,D", ARG_NONE},
    {"AND A,E", ARG_NONE},
    {"AND A,H", ARG_NONE},
    {"AND A,L", ARG_NONE},
    {"AND A,(HL)", ARG_NONE},
    {"AND A,A", ARG_NONE},
    {"XOR A,B", ARG_NONE},
    {"XOR A,C", ARG_NONE},
    {"XOR A,D", ARG_NONE},
    {"XOR A,E", ARG_NONE},
    {"XOR A,H", ARG_NONE},
    {"XOR A,L", ARG_NONE},
    {"XOR A,(HL)", ARG_NONE},
    {"XOR A,A", ARG_NONE},
    {"OR A,B", ARG_NONE},
    {"OR A,C", ARG_NONE},
    {"OR A,D", ARG_NONE},
    {"OR A,E", ARG_NONE},
    {"OR A,H", ARG_NONE},
    {"OR A,L", ARG_NONE},
    {"OR A,(HL)", ARG_NONE},
    {"OR A,A", ARG_NONE},
    {"CP A,B", ARG_NONE},
    {"CP A,C", ARG_NONE},
    {"CP A,D", ARG_NONE},
    {"CP A,E", ARG_NONE},
    {"CP A,H", ARG_NONE},
    {"CP A,L", ARG_NONE},
    {"CP A,(HL)", ARG_NONE},
    {"CP A,A", ARG_NONE},
    {"RET NZ", ARG_NONE},
    {"POP BC", ARG_NONE},
    {"JP NZ,%04X", ARG_U16},
    {"JP %04X", ARG_U16},
    {"CALL NZ,%04X", ARG_U16},
    {"PUSH BC", ARG_NONE},
    {"ADD A,%02X", ARG_U8},
    {"RST 00h", ARG_NONE},
    {"RET Z", ARG_NONE},
    {"RET", ARG_NONE},
    {"JP Z,%04X", ARG_U16},
    {"PREFIX CB", ARG_NONE},
    {"CALL Z,%04X", ARG_U16},
    {"CALL %04X", ARG_U16},
    {"ADC A,%02X", ARG_U8},
    {"RST 08h", ARG_NONE},
    {"RET NC", ARG_NONE},
    {"POP DE", ARG_NONE},
    {"JP NC,%04X", ARG_U16},
    {"UNUSED", ARG_NONE},
    {"CALL NC,%04X", ARG_U16},
    {"PUSH DE", ARG_NONE},
    {"SUB A,%02X", ARG_U8},
    {"RST 10h", ARG_NONE},
    {"RET C", ARG_NONE},
    {"RETI", ARG_NONE},
    {"JP C,%04X", ARG_U16},
    {"UNUSED", ARG_NONE},
    {"CALL C,%04X", ARG_U16},
    {"UNUSED", ARG_NONE},
    {"SBC A,%02X", ARG_U8},
    {"RST 18h", ARG_NONE},
    {"LD (FF00+%02X),A", ARG_U8},
    {"POP HL", ARG_NONE},
    {"LD (FF00+C),A", ARG_NONE},
    {"UNUSED", ARG_NONE},
    {"UNUSED", ARG_NONE},
    {"PUSH HL", ARG_NONE},
    {"AND A,%02X", ARG_U8},
    {"RST 20h", ARG_NONE},
    {"ADD SP,%02X", ARG_I8},
    {"JP HL", ARG_NONE},
    {"LD (%04X),A", ARG_U16},
    {"UNUSED", ARG_NONE},
    {"UNUSED", ARG_NONE},
    {"UNUSED", ARG_NONE},
    {"XOR A,%02X", ARG_U8},
    {"RST 28h", ARG_NONE},
    {"LD A,(FF00+%02X)", ARG_U8},
    {"POP AF", ARG_NONE},
    {"LD A,(FF00+C)", ARG_NONE},
    {"DI", ARG_NONE},
    {"UNUSED", ARG_NONE},
    {"PUSH AF", ARG_NONE},
    {"OR A,%02X", ARG_U8},
    {"RST 30h", ARG_NONE},
    {"LD HL,SP+%02X", ARG_I8},
    {"LD SP,HL", ARG_NONE},
    {"LD A,(%04X)", ARG_U16},
    {"EI", ARG_NONE},
    {"UNUSED", ARG_NONE},
    {"UNUSED", ARG_NONE},
    {"CP A,%02X", ARG_U8},
    {"RST 38h", ARG_NONE}
};

const OpcodeInfo cb_opcode_info[256] = {
    {"RLC B", ARG_NONE},
    {"RLC C", ARG_NONE},
    {"RLC D", ARG_NONE},
    {"RLC E", ARG_NONE},
    {"RLC H", ARG_NONE},
    {"RLC L", ARG_NONE},
    {"RLC (HL)", ARG_NONE},
    {"RLC A", ARG_NONE},
    {"RRC B", ARG_NONE},
    {"RRC C", ARG_NONE},
    {"RRC D", ARG_NONE},
    {"RRC E", ARG_NONE},
    {"RRC H", ARG_NONE},
    {"RRC L", ARG_NONE},
    {"RRC (HL)", ARG_NONE},
    {"RRC A", ARG_NONE},
    {"RL B", ARG_NONE},
    {"RL C", ARG_NONE},
    {"RL D", ARG_NONE},
    {"RL E", ARG_NONE},
    {"RL H", ARG_NONE},
    {"RL L", ARG_NONE},
    {"RL (HL)", ARG_NONE},
    {"RL A", ARG_NONE},
    {"RR B", ARG_NONE},
    {"RR C", ARG_NONE},
    {"RR D", ARG_NONE},
    {"RR E", ARG_NONE},
    {"RR H", ARG_NONE},
    {"RR L", ARG_NONE},
    {"RR (HL)", ARG_NONE},
    {"RR A", ARG_NONE},
    {"SLA B", ARG_NONE},
    {"SLA C", ARG_NONE},
    {"SLA D", ARG_NONE},
    {"SLA E", ARG_NONE},
    {"SLA H", ARG_NONE},
    {"SLA L", ARG_NONE},
    {"SLA (HL)", ARG_NONE},
    {"SLA A", ARG_NONE},
    {"SRA B", ARG_NONE},
    {"SRA C", ARG_NONE},
    {"SRA D", ARG_NONE},
    {"SRA E", ARG_NONE},
    {"SRA H", ARG_NONE},
    {"SRA L", ARG_NONE},
    {"SRA (HL)", ARG_NONE},
    {"SRA A", ARG_NONE},
    {"SWAP B", ARG_NONE},
    {"SWAP C", ARG_NONE},
    {"SWAP D", ARG_NONE},
    {"SWAP E", ARG_NONE},
    {"SWAP H", ARG_NONE},
    {"SWAP L", ARG_NONE},
    {"SWAP (HL)", ARG_NONE},
    {"SWAP A", ARG_NONE},
    {"SRL B", ARG_NONE},
    {"SRL C", ARG_NONE},
    {"SRL D", ARG_NONE},
    {"SRL E", ARG_NONE},
    {"SRL H", ARG_NONE},
    {"SRL L", ARG_NONE},
    {"SRL (HL)", ARG_NONE},
    {"SRL A", ARG_NONE},
    {"BIT 0,B", ARG_NONE},
    {"BIT 0,C", ARG_NONE},
    {"BIT 0,D", ARG_NONE},
    {"BIT 0,E", ARG_NONE},
    {"BIT 0,H", ARG_NONE},
    {"BIT 0,L", ARG_NONE},
    {"BIT 0,(HL)", ARG_NONE},
    {"BIT 0,A", ARG_NONE},
    {"BIT 1,B", ARG_NONE},
    {"BIT 1,C", ARG_NONE},
    {"BIT 1,D", ARG_NONE},
    {"BIT 1,E", ARG_NONE},
    {"BIT 1,H", ARG_NONE},
    {"BIT 1,L", ARG_NONE},
    {"BIT 1,(HL)", ARG_NONE},
    {"BIT 1,A", ARG_NONE},
    {"BIT 2,B", ARG_NONE},
    {"BIT 2,C", ARG_NONE},
    {"BIT 2,D", ARG_NONE},
    {"BIT 2,E", ARG_NONE},
    {"BIT 2,H", ARG_NONE},
    {"BIT 2,L", ARG_NONE},
    {"BIT 2,(HL)", ARG_NONE},
    {"BIT 2,A", ARG_NONE},
    {"BIT 3,B", ARG_NONE},
    {"BIT 3,C", ARG_NONE},
    {"BIT 3,D", ARG_NONE},
    {"BIT 3,E", ARG_NONE},
    {"BIT 3,H", ARG_NONE},
    {"BIT 3,L", ARG_NONE},
    {"BIT 3,(HL)", ARG_NONE},
    {"BIT 3,A", ARG_NONE},
    {"BIT 4,B", ARG_NONE},
    {"BIT 4,C", ARG_NONE},
    {"BIT 4,D", ARG_NONE},
    {"BIT 4,E", ARG_NONE},
    {"BIT 4,H", ARG_NONE},
    {"BIT 4,L", ARG_NONE},
    {"BIT 4,(HL)", ARG_NONE},
    {"BIT 4,A", ARG_NONE},
    {"BIT 5,B", ARG_NONE},
    {"BIT 5,C", ARG_NONE},
    {"BIT 5,D", ARG_NONE},
    {"BIT 5,E", ARG_NONE},
    {"BIT 5,H", ARG_NONE},
    {"BIT 5,L", ARG_NONE},
    {"BIT 5,(HL)", ARG_NONE},
    {"BIT 5,A", ARG_NONE},
    {"BIT 6,B", ARG_NONE},
    {"BIT 6,C", ARG_NONE},
    {"BIT 6,D", ARG_NONE},
    {"BIT 6,E", ARG_NONE},
    {"BIT 6,H", ARG_NONE},
    {"BIT 6,L", ARG_NONE},
    {"BIT 6,(HL)", ARG_NONE},
    {"BIT 6,A", ARG_NONE},
    {"BIT 7,B", ARG_NONE},
    {"BIT 7,C", ARG_NONE},
    {"BIT 7,D", ARG_NONE},
    {"BIT 7,E", ARG_NONE},
    {"BIT 7,H", ARG_NONE},
    {"BIT 7,L", ARG_NONE},
    {"BIT 7,(HL)", ARG_NONE},
    {"BIT 7,A", ARG_NONE},
    {"RES 0,B", ARG_NONE},
    {"RES 0,C", ARG_NONE},
    {"RES 0,D", ARG_NONE},
    {"RES 0,E", ARG_NONE},
    {"RES 0,H", ARG_NONE},
    {"RES 0,L", ARG_NONE},
    {"RES 0,(HL)", ARG_NONE},
    {"RES 0,A", ARG_NONE},
    {"RES 1,B", ARG_NONE},
    {"RES 1,C", ARG_NONE},
    {"RES 1,D", ARG_NONE},
    {"RES 1,E", ARG_NONE},
    {"RES 1,H", ARG_NONE},
    {"RES 1,L", ARG_NONE},
    {"RES 1,(HL)", ARG_NONE},
    {"RES 1,A", ARG_NONE},
    {"RES 2,B", ARG_NONE},
    {"RES 2,C", ARG_NONE},
    {"RES 2,D", ARG_NONE},
    {"RES 2,E", ARG_NONE},
    {"RES 2,H", ARG_NONE},
    {"RES 2,L", ARG_NONE},
    {"RES 2,(HL)", ARG_NONE},
    {"RES 2,A", ARG_NONE},
    {"RES 3,B", ARG_NONE},
    {"RES 3,C", ARG_NONE},
    {"RES 3,D", ARG_NONE},
    {"RES 3,E", ARG_NONE},
    {"RES 3,H", ARG_NONE},
    {"RES 3,L", ARG_NONE},
    {"RES 3,(HL)", ARG_NONE},
    {"RES 3,A", ARG_NONE},
    {"RES 4,B", ARG_NONE},
    {"RES 4,C", ARG_NONE},
    {"RES 4,D", ARG_NONE},
    {"RES 4,E", ARG_NONE},
    {"RES 4,H", ARG_NONE},
    {"RES 4,L", ARG_NONE},
    {"RES 4,(HL)", ARG_NONE},
    {"RES 4,A", ARG_NONE},
    {"RES 5,B", ARG_NONE},
    {"RES 5,C", ARG_NONE},
    {"RES 5,D", ARG_NONE},
    {"RES 5,E", ARG_NONE},
    {"RES 5,H", ARG_NONE},
    {"RES 5,L", ARG_NONE},
    {"RES 5,(HL)", ARG_NONE},
    {"RES 5,A", ARG_NONE},
    {"RES 6,B", ARG_NONE},
    {"RES 6,C", ARG_NONE},
    {"RES 6,D", ARG_NONE},
    {"RES 6,E", ARG_NONE},
    {"RES 6,H", ARG_NONE},
    {"RES 6,L", ARG_NONE},
    {"RES 6,(HL)", ARG_NONE},
    {"RES 6,A", ARG_NONE},
    {"RES 7,B", ARG_NONE},
    {"RES 7,C", ARG_NONE},
    {"RES 7,D", ARG_NONE},
    {"RES 7,E", ARG_NONE},
    {"RES 7,H", ARG_NONE},
    {"RES 7,L", ARG_NONE},
    {"RES 7,(HL)", ARG_NONE},
    {"RES 7,A", ARG_NONE},
    {"SET 0,B", ARG_NONE},
    {"SET 0,C", ARG_NONE},
    {"SET 0,D", ARG_NONE},
    {"SET 0,E", ARG_NONE},
    {"SET 0,H", ARG_NONE},
    {"SET 0,L", ARG_NONE},
    {"SET 0,(HL)", ARG_NONE},
    {"SET 0,A", ARG_NONE},
    {"SET 1,B", ARG_NONE},
    {"SET 1,C", ARG_NONE},
    {"SET 1,D", ARG_NONE},
    {"SET 1,E", ARG_NONE},
    {"SET 1,H", ARG_NONE},
    {"SET 1,L", ARG_NONE},
    {"SET 1,(HL)", ARG_NONE},
    {"SET 1,A", ARG_NONE},
    {"SET 2,B", ARG_NONE},
    {"SET 2,C", ARG_NONE},
    {"SET 2,D", ARG_NONE},
    {"SET 2,E", ARG_NONE},
    {"SET 2,H", ARG_NONE},
    {"SET 2,L", ARG_NONE},
    {"SET 2,(HL)", ARG_NONE},
    {"SET 2,A", ARG_NONE},
    {"SET 3,B", ARG_NONE},
    {"SET 3,C", ARG_NONE},
    {"SET 3,D", ARG_NONE},
    {"SET 3,E", ARG_NONE},
    {"SET 3,H", ARG_NONE},
    {"SET 3,L", ARG_NONE},
    {"SET 3,(HL)", ARG_NONE},
    {"SET 3,A", ARG_NONE},
    {"SET 4,B", ARG_NONE},
    {"SET 4,C", ARG_NONE},
    {"SET 4,D", ARG_NONE},
    {"SET 4,E", ARG_NONE},
    {"SET 4,H", ARG_NONE},
    {"SET 4,L", ARG_NONE},
    {"SET 4,(HL)", ARG_NONE},
    {"SET 4,A", ARG_NONE},
    {"SET 5,B", ARG_NONE},
    {"SET 5,C", ARG_NONE},
    {"SET 5,D", ARG_NONE},
    {"SET 5,E", ARG_NONE},
    {"SET 5,H", ARG_NONE},
    {"SET 5,L", ARG_NONE},
    {"SET 5,(HL)", ARG_NONE},
    {"SET 5,A", ARG_NONE},
    {"SET 6,B", ARG_NONE},
    {"SET 6,C", ARG_NONE},
    {"SET 6,D", ARG_NONE},
    {"SET 6,E", ARG_NONE},
    {"SET 6,H", ARG_NONE},
    {"SET 6,L", ARG_NONE},
    {"SET 6,(HL)", ARG_NONE},
    {"SET 6,A", ARG_NONE},
    {"SET 7,B", ARG_NONE},
    {"SET 7,C", ARG_NONE},
    {"SET 7,D", ARG_NONE},
    {"SET 7,E", ARG_NONE},
    {"SET 7,H", ARG_NONE},
    {"SET 7,L", ARG_NONE},
    {"SET 7,(HL)", ARG_NONE},
    {"SET 7,A", ARG_NONE}
};

enum
{
    INT_VBLANK = 0,
    INT_LCD_STAT,
    INT_TIMER,
    INT_SERIAL,
    INT_JOYPAD
};

const uint8_t int_vectors[5] = {
    0x40,
    0x48,
    0x50,
    0x58,
    0x60
};

// For halt behaviour, please read section 4.10. in https://github.com/AntonioND/giibiiadvance/blob/master/docs/TCAGBD.pdf
enum
{
    HALT_MODE0 = 0,
    HALT_MODE1,
    HALT_MODE2
};

enum
{
    MODE_HBLANK = 0,
    MODE_VBLANK,
    MODE_OAM_SEARCH,
    MODE_PIXEL_TRANSFER
};

enum
{
    SHADE0 = 0,
    SHADE1,
    SHADE2,
    SHADE3
};

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

std::ofstream fout;

int int_from_string(const std::string& s, bool isHex)
{
    int x;
    std::stringstream ss;
    if (isHex) ss << std::hex << s; else ss << s;
    ss >> x;
    return x;
}

template<typename T>
std::string int_to_hex(T i, int w=-1, bool prefixed=true)
{
    if (w == -1) w = sizeof(T) * 2;
    std::stringstream stream;
    stream << (prefixed ? "0x" : "")
           << std::setfill ('0') << std::setw(w)
           << std::uppercase << std::hex << i;
    return stream.str();
}

template<typename T>
std::string int_to_bin8(T i, bool prefixed=true)
{
    std::stringstream stream;
    stream << (prefixed ? "0b" : "") << std::bitset<8>(i);
    return stream.str();
}

int CDECL PopMessageBox(TCHAR *szTitle, UINT uType, const char* format, ...)
{
	char buffer[100];
	va_list vl;

	va_start(vl, format);
    vsprintf(buffer, format, vl);
	va_end(vl);

	return MessageBox(nullptr, buffer, szTitle, uType);
}

/* configurable variables */
int SCREEN_SCALE_FACTOR;
bool ENABLE_BOOT_ROM;
std::string BOOT_ROM_PATH;

void ConfigureEmulatorSettings()
{
    std::ifstream istream(CONFIG_FILE_PATH, std::ios::in);
    std::string line;
    std::string val;

    if (!istream)
    {
#ifdef GUI_MODE
        LOG_F(INFO, "config is not found, creating one with default settings...");
#else
        fout << "config is not found, creating one with default settings..." << std::endl;
#endif
        SCREEN_SCALE_FACTOR = 3;
        ENABLE_BOOT_ROM = false;
        BOOT_ROM_PATH = "";

        std::ofstream config(CONFIG_FILE_PATH);
        config << "SCREEN_SCALE_FACTOR=" << SCREEN_SCALE_FACTOR << std::endl;
        config << "ENABLE_BOOT_ROM=" << ENABLE_BOOT_ROM << std::endl;
        config << "BOOT_ROM_PATH=" << BOOT_ROM_PATH << std::endl;
        config.close();

        return;
    }

    std::getline(istream, line);
    SCREEN_SCALE_FACTOR = stoi(line.substr(line.find("=") + 1));

    std::getline(istream, line);
    ENABLE_BOOT_ROM = stoi(line.substr(line.find("=") + 1));

    std::getline(istream, line);
    BOOT_ROM_PATH = line.substr(line.find("=") + 1);

    istream.close();
}

struct rgb_tuple
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

// TODO make it modificable by config file
rgb_tuple gb_colours[4] = {
    {224, 248, 208},
    {136, 192, 112},
    {52, 104, 86},
    {8, 24, 32}
};

class GBComponent;
class CPU;
class InterruptManager;
class Timer;
class SimpleGPU;
class GPU; /* GPU with pixel FIFO */ // TODO
class Cartridge; // TODO
class MBCBase; /* abstract */ // TODO
class MBC1; // TODO
class MemoryController;
class JoyPad;
class Emulator;
#ifdef GUI_MODE
class GameBoyWindows;
#endif

class GBComponent
{
public:
    virtual void Init() = 0;
    virtual void Reset() = 0;
    virtual void Debug_PrintStatus() = 0;
protected:
    Emulator *m_Emulator;
};

class CPU : GBComponent
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

class InterruptManager : GBComponent
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

class Timer : GBComponent
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

class SimpleGPU : GBComponent
{
private:
    Emulator *m_Emulator;

    int m_Counter; // keep track of cycles in a current scanline

    // https://gbdev.io/pandocs/LCDC.html
    bool bLCDEnabled();
    bool bWindowEnable();
    bool bSpriteEnable();
    bool bBGAndWindowEnable();

    // https://gbdev.io/pandocs/STAT.html
    bool bCoincidenceStatInterrupt();
    bool bOAMStatInterrupt();
    bool bVBlankStatInterrupt();
    bool bHBlankStatInterrupt();
    void UpdateMode(int mode);

    void UpdateLCDStatus();
    void DrawCurrentLine();
    void RenderBackground();
    void RenderSprites();

    void RenderPixel(int row, int col, rgb_tuple colour, int attr);
    rgb_tuple GetColour(int colourNum, uint16_t address);
public:
    SimpleGPU(Emulator *emu);
    ~SimpleGPU();

    void Init();
    void Reset();
    void Update(int cycles);
    int GetMode();

    std::array<uint8_t, SCREEN_WIDTH * SCREEN_HEIGHT * 4> m_Pixels;

    uint8_t &LCDC;
    uint8_t &STAT;
    uint8_t &SCY;
    uint8_t &SCX;
    uint8_t &LY;
    uint8_t &LYC;
    uint8_t &BGP;
    uint8_t &OBP0;
    uint8_t &OBP1;
    uint8_t &WY;
    uint8_t &WX;

    void Debug_PrintStatus();
};

class MemoryController : GBComponent
{
private:
    // Please refer to:
    // - https://gbdev.io/pandocs/Memory_Map.html
    // - http://gameboy.mongenel.com/dmg/asmmemmap.html
    std::array<uint8_t, 0x8000> m_Cartridge; // Note for future implementation: cartridge size is dependent on MBC and create a seperate class when cartridges get more complicated
    std::array<uint8_t, 0x2000> m_VRAM; // Note: The PPU can access VRAM directly, but the CPU has to access VRAM through this class.
    std::array<uint8_t, 0x4000> m_WRAM;
    std::array<uint8_t, 0xA0> m_OAM;
    std::array<uint8_t, 0x7F> m_HRAM;

    Emulator *m_Emulator;

    bool inBootMode;
    std::array<uint8_t, 0x100> m_BootROM;

    void InitBootROM();
    void DMATransfer(uint8_t data);
public:
    MemoryController(Emulator *emu);
    ~MemoryController();

    void Init();
    void Reset() {} // nothing
    void LoadROM(const std::string& rom_file);
    void ReloadROM();
    uint8_t ReadByte(uint16_t address) const;
    uint16_t ReadWord(uint16_t address) const;
    void WriteByte(uint16_t address, uint8_t data);
    void WriteWord(uint16_t address, uint16_t data);

    void Debug_PrintStatus() {} // nothing
    void Debug_PrintMemory(uint16_t address);
    void Debug_EditMemory(uint16_t address, uint8_t data);
    void Debug_PrintMemoryRange(uint16_t start, uint16_t end);

    std::array<uint8_t, 0x80> m_IO;

    uint8_t IE;
};

// good explanation here https://github.com/gbdev/pandocs/blob/be820673f71c8ca514bab4d390a3004c8273f988/historical/1995-Jan-28-GAMEBOY.txt#L165
class JoyPad : GBComponent
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

class Emulator
{
public:
    Emulator();
    ~Emulator();

    void InitComponents();
    void ResetComponents();
    void Update();
    void Tick(); // + 1 M-cycle

    void Debug_Step(std::vector<char>& blargg_serial, int times);
    void Debug_StepTill(std::vector<char>& blargg_serial, uint16_t x);
    void Debug_PrintEmulatorStatus();

    // All components
    std::unique_ptr<CPU> m_CPU;
    std::unique_ptr<MemoryController> m_MemControl;
    std::unique_ptr<InterruptManager> m_IntManager;
    std::unique_ptr<Timer> m_Timer;
    std::unique_ptr<SimpleGPU> m_GPU;
    std::unique_ptr<JoyPad> m_JoyPad;

    int m_TotalCycles; // T-cycles
    int m_PrevTotalCycles;
};

#ifdef GUI_MODE
class GameBoyWindows
{
private:
    std::unique_ptr<Emulator> m_Emulator;

    SDL_Window *m_Window;
    SDL_Renderer *m_Renderer;
    SDL_Texture *m_Texture;
public:
    GameBoyWindows();
    ~GameBoyWindows();

    void Initialize();
    void LoadROM(const std::string& rom_file);
    void ReloadROM();

    void Create(HWND);
    void FixSize();
    void Update();
    void RenderGraphics();
    void HandleKeyDown(WPARAM wParam);
    void HandleKeyUp(WPARAM wParam);
    void CleanUp();
    void Destroy();
};
#endif

/* TODO prevent accesses to VRAM/OAM during certain PPU modes... maybe when accessing VRAM/OAM, do it thru PPU (PPU is responsible for VRAM/OAM accesses) */

uint8_t CPU::ReadByte(uint16_t address) const
{
    m_Emulator->Tick();
    return m_Emulator->m_MemControl->ReadByte(address);
}

uint16_t CPU::ReadWord(uint16_t address) const
{
    m_Emulator->Tick();
    m_Emulator->Tick();
    return m_Emulator->m_MemControl->ReadWord(address);
}

void CPU::WriteByte(uint16_t address, uint8_t data)
{
    m_Emulator->Tick();
    m_Emulator->m_MemControl->WriteByte(address, data);
}

void CPU::WriteWord(uint16_t address, uint16_t data)
{
    m_Emulator->Tick();
    m_Emulator->Tick();
    m_Emulator->m_MemControl->WriteWord(address, data);
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
    return TEST_BIT(F, flag);
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
    bool bIsBit7Set = TEST_BIT(n, 7);
    n <<= 1;
    if (circular ? bIsBit7Set : CPU::IsFlagSet(FLAG_C)) SET_BIT(n, 0);
    CPU::ModifyFlag(FLAG_Z, resetZ ? 0 : n == 0);
    CPU::ModifyFlag(FLAG_N, 0);
    CPU::ModifyFlag(FLAG_H, 0);
    CPU::ModifyFlag(FLAG_C, bIsBit7Set);
}

void CPU::RotateBitsRight(uint8_t &n, bool circular, bool resetZ)
{
    bool bIsBit0Set = TEST_BIT(n, 0);
    n >>= 1;
    if (circular ? bIsBit0Set : CPU::IsFlagSet(FLAG_C)) SET_BIT(n, 7);
    CPU::ModifyFlag(FLAG_Z, resetZ ? 0 : n == 0);
    CPU::ModifyFlag(FLAG_N, 0);
    CPU::ModifyFlag(FLAG_H, 0);
    CPU::ModifyFlag(FLAG_C, bIsBit0Set);
}

void CPU::ShiftBitsLeft(uint8_t &n)
{
    bool bIsBit7Set = TEST_BIT(n, 7);
    n <<= 1;
    CPU::ModifyFlag(FLAG_Z, n == 0);
    CPU::ModifyFlag(FLAG_N, 0);
    CPU::ModifyFlag(FLAG_H, 0);
    CPU::ModifyFlag(FLAG_C, bIsBit7Set);
}

void CPU::ShiftBitsRight(uint8_t &n, bool logical)
{
    bool bIsBit0Set = TEST_BIT(n, 0);
    bool bIsBit7Set = TEST_BIT(n, 7);
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
        CPU::ModifyFlag(FLAG_Z, !TEST_BIT(*reg8_group[opcode & 7], opcode >> 3 & 7));
        CPU::ModifyFlag(FLAG_N, 0);
        CPU::ModifyFlag(FLAG_H, 1);
        break;
    }
    /* bit n,(HL) */
    case 0x46: case 0x4E: case 0x56: case 0x5E: case 0x66: case 0x6E: case 0x76: case 0x7E:
    {
        uint8_t hl = CPU::ReadByte(HL);
        CPU::ModifyFlag(FLAG_Z, !TEST_BIT(hl, (opcode - 0x46) >> 3));
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
    default:
    {
#ifndef GUI_MODE
        std::cerr << "Opcode: " << int_to_hex(opcode) << std::endl;
        CPU::Debug_PrintStatus();
#else
        LOG_IF_F(ERROR, true, "Unknown opcode: %02X", opcode);
#endif
        throw std::runtime_error("Opcode unimplemented!");
        break;
    }
    }
}

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

#ifndef GUI_MODE
    CPU::Debug_LogState();
#endif

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
#ifndef GUI_MODE
        std::cerr << "Opcode: " << int_to_hex(opcode) << std::endl;
        CPU::Debug_PrintStatus();
#else
        LOG_IF_F(ERROR, true, "Unknown opcode: %02X", opcode);
#endif
        throw std::runtime_error("Opcode unimplemented!");
        break;
    }
    }

    if (!lock && haltBug)
    {
        PC = old_pc;
        haltBug = false;
        m_Emulator->m_IntManager->ResetHaltMode();
    }
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
    std::cout << "AF(3)=" << int_to_hex(reg16[3]) << std::endl;
    std::cout << "BC(0)=" << int_to_hex(reg16[0]) << std::endl;
    std::cout << "DE(1)=" << int_to_hex(reg16[1]) << std::endl;
    std::cout << "HL(2)=" << int_to_hex(HL) << std::endl;
    std::cout << "SP(4)=" << int_to_hex(SP) << std::endl;
    std::cout << "PC(5)=" << int_to_hex(PC) << std::endl;
    std::cout << std::endl;
}

void CPU::Debug_PrintCurrentInstruction()
{
    uint16_t addr = PC;
    uint8_t temp = m_Emulator->m_MemControl->ReadByte(addr++);
    uint8_t opcode;
    bool cb_prefixed = temp == 0xCB;
    uint8_t low;
    uint8_t high;
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

InterruptManager::InterruptManager(Emulator *emu)
  : IF(emu->m_MemControl->m_IO[0x0F]),
    IE(emu->m_MemControl->IE)
{
    m_Emulator = emu;
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
    m_IME = true;
}

void InterruptManager::DisableIME()
{
    m_IME = false;
}

bool InterruptManager::SetHaltMode()
{
    if (m_IME)
    {
        haltMode = HALT_MODE0;
        return true;
    }
    else if (!m_IME && ((IE & IF) == 0))
    {
        haltMode = HALT_MODE1;
        return true;
    }
    else if (!m_IME && ((IE & IF) != 0))
    {
        haltMode = HALT_MODE2;
        return false;
    }

#ifndef GUI_MODE
    std::cerr << "Yo nigga, InterruptManager::SetHaltMode has gone mad!" << std::endl;
#else
    LOG_IF_F(WARNING, true, "Yo nigga, InterruptManager::SetHaltMode has gone mad!");
#endif

    return false;
}

void InterruptManager::ResetHaltMode()
{
    haltMode = -1;
}

void InterruptManager::RequestInterrupt(int i)
{
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
            if (m_IME && TEST_BIT(IF, i) && TEST_BIT(IE, i))
            {
                m_Emulator->m_CPU->HandleInterrupt(i);
            }
        }
    }
}

void InterruptManager::Debug_PrintStatus()
{
    std::cout << "*INTERRUPT MANAGER STATUS*" << std::endl;
    std::cout << "IME=" << m_Emulator->m_IntManager->GetIME() << " "
              << "IF=" << int_to_bin8(IF) << " "
              << "IE=" << int_to_bin8(IE) << std::endl;
    std::cout << std::endl;
}

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
    std::cout << "DIV=" << int_to_hex(DIV) << " "
              << "TIMA=" << int_to_hex(+TIMA, 2) << " "
              << "TMA=" << int_to_hex(+TMA, 2) << " "
              << "TAC=" << int_to_bin8(TAC) << std::endl;
    std::cout << std::endl;
}

bool SimpleGPU::bLCDEnabled()
{
    return TEST_BIT(LCDC, 7);
}

bool SimpleGPU::bWindowEnable()
{
    return TEST_BIT(LCDC, 5);
}

bool SimpleGPU::bSpriteEnable()
{
    return TEST_BIT(LCDC, 1);
}

bool SimpleGPU::bBGAndWindowEnable()
{
    return TEST_BIT(LCDC, 0);
}

bool SimpleGPU::bCoincidenceStatInterrupt()
{
    return TEST_BIT(STAT, 6);
}

bool SimpleGPU::bOAMStatInterrupt()
{
    return TEST_BIT(STAT, 5);
}

bool SimpleGPU::bVBlankStatInterrupt()
{
    return TEST_BIT(STAT, 4);
}

bool SimpleGPU::bHBlankStatInterrupt()
{
    return TEST_BIT(STAT, 3);
}

void SimpleGPU::UpdateMode(int mode)
{
    STAT &= 0b11111100;
    STAT |= mode;
}

void SimpleGPU::UpdateLCDStatus()
{
    if (!SimpleGPU::bLCDEnabled())
    {
        m_Counter = DOTS_PER_SCANLINE;
        LY = 0;
        SimpleGPU::UpdateMode(MODE_VBLANK);
        return;
    }

    bool stat_int = false;
    int old_mode = SimpleGPU::GetMode();
    int new_mode;

    if (LY >= 144)
    {
        new_mode = MODE_VBLANK;
        stat_int = SimpleGPU::bVBlankStatInterrupt();
    }
    else
    {
        int dots = DOTS_PER_SCANLINE - m_Counter;

        if (dots <= 80) // mode 2
        {
            new_mode = MODE_OAM_SEARCH;
            stat_int = SimpleGPU::bOAMStatInterrupt();
        }
        else if (dots > 80 && dots <= 360) // mode 3
        {
            new_mode = MODE_PIXEL_TRANSFER;
        }
        else // mode 0
        {
            new_mode = MODE_HBLANK;
            stat_int = SimpleGPU::bHBlankStatInterrupt();
        }
    }

    SimpleGPU::UpdateMode(new_mode);

    if (stat_int && (old_mode != new_mode))
    {
        m_Emulator->m_IntManager->RequestInterrupt(INT_LCD_STAT);
    }

    if (LY == LYC)
    {
        SET_BIT(STAT, 2);

        if (SimpleGPU::bCoincidenceStatInterrupt())
        {
            m_Emulator->m_IntManager->RequestInterrupt(INT_LCD_STAT);
        }
    }
    else
    {
        RES_BIT(STAT, 2);
    }
}

void SimpleGPU::DrawCurrentLine()
{
    if (SimpleGPU::bBGAndWindowEnable())
    {
        SimpleGPU::RenderBackground();
    }

    if (SimpleGPU::bSpriteEnable())
    {
        SimpleGPU::RenderSprites();
    }
}

void SimpleGPU::RenderBackground()
{
    uint16_t wndTileMem = TEST_BIT(LCDC, 4) ? 0x8000 : 0x8800;

    bool _signed = wndTileMem == 0x8800;
    bool usingWnd = TEST_BIT(LCDC, 5) && WY <= LY; // true if window display is enabled (specified in LCDC) and WndY <= LY

    uint16_t bkgdTileMem = TEST_BIT(LCDC, usingWnd ? 6 : 3) ? 0x9C00 : 0x9800;

    // the y-position is used to determine which of 32 (256 / 8) vertical tiles will used (background map y)
    uint8_t yPos = !usingWnd ? SCY + LY : LY - WY; // map to window coordinates if necessary
    uint16_t tileRow = ((uint8_t) (yPos / 8)) * 32; // which of 8 vertical pixels of the current tile is the scanline on?

    // time to draw a scanline which consists of 160 pixels
    for (int pixel = 0; pixel < 160; ++pixel)
    {
        uint8_t xPos = usingWnd && pixel >= WX ? pixel - WX : SCX + pixel;
        uint16_t tileCol = xPos / 8; // which of horizontal pixels of the current tile does xPos fall in?
        uint16_t tileAddr = bkgdTileMem + tileRow + tileCol;
        int16_t tileNum = _signed ? (int8_t) m_Emulator->m_MemControl->ReadByte(tileAddr) : (uint8_t) m_Emulator->m_MemControl->ReadByte(tileAddr);
        uint16_t tileLocation = wndTileMem + (_signed ? (tileNum + 128) * 16 : tileNum * 16);

        // determine the correct row of pixels
        uint8_t line = yPos % 8;
        line *= 2; // each line needs 2 bytes of memory so...

        uint8_t data1 = m_Emulator->m_MemControl->ReadByte(tileLocation + line);
        uint8_t data2 = m_Emulator->m_MemControl->ReadByte(tileLocation + line + 1);

        int bit = 7 - (xPos % 8); // bit position in data1 and data2 (because pixel 0 corresponds to bit 7 and pixel 1 corresponds to bit 6 and so on)
        int colourNum = (GET_BIT(data2, bit) << 1) | GET_BIT(data1, bit);

        SimpleGPU::RenderPixel(LY, pixel, SimpleGPU::GetColour(colourNum, 0xFF47), -1);
    }
}

void SimpleGPU::RenderSprites()
{
    bool use8x16 = TEST_BIT(LCDC, 2); // determine the sprite size

    // the sprite layer can display up to 40 sprites
    for (int i = 0; i < 40; ++i)
    {
        int index = i * 4; // each sprite takes 4 bytes of OAM space (0xFE00-0xFE9F)

        uint8_t spriteY = m_Emulator->m_MemControl->ReadByte(0xFE00 + index) - 16;
        uint8_t spriteX = m_Emulator->m_MemControl->ReadByte(0xFE00 + index + 1) - 8;
        uint8_t patternNumber = m_Emulator->m_MemControl->ReadByte(0xFE00 + index + 2);
        uint8_t attributes = m_Emulator->m_MemControl->ReadByte(0xFE00 + index + 3);

        bool xFlip = TEST_BIT(attributes, 5);
        bool yFlip = TEST_BIT(attributes, 6);

        int height = use8x16 ? 16 : 8;

        // does the scanline intercept this sprite?
        if ((LY >= spriteY) && (LY < (spriteY + height)))
        {
            int line = LY - spriteY;

            if (yFlip)
            {
                line = height - line; // read backwards if y flipping is allowed
            }

            line *= 2; // each line takes 2 bytes of memory

            uint16_t tileLocation = 0x8000 + (patternNumber * 16);

            uint8_t data1 = m_Emulator->m_MemControl->ReadByte(tileLocation + line);
            uint8_t data2 = m_Emulator->m_MemControl->ReadByte(tileLocation + line + 1);

            for (int tilePixel = 7; tilePixel >= 0; tilePixel--)
            {
                int bit = tilePixel;

                if (xFlip)
                {
                    bit = 7 - bit; // read backwards if x flipping is allowed
                }

                int colourNum = (GET_BIT(data2, bit) << 1) | GET_BIT(data1, bit);
                rgb_tuple col = SimpleGPU::GetColour(colourNum, TEST_BIT(attributes, 4) ? 0xFF49 : 0xFF48);

                // it's transparent for sprites
                if (col.r == gb_colours[SHADE0].r && col.g == gb_colours[SHADE0].g && col.b == gb_colours[SHADE0].b)
                {
                    continue;
                }

                int pixel = spriteX + (7 - tilePixel);

                if ((LY < 0) || (LY > 143) || (pixel < 0) || (pixel > 159))
                {
                    continue;
                }

                SimpleGPU::RenderPixel(LY, pixel, col, attributes);
            }
        }
    }
}

void SimpleGPU::RenderPixel(int row, int col, rgb_tuple colour, int attr)
{
    int offset = row * SCREEN_WIDTH * 4 + col * 4;

    // check if pixel is hidden behind background
    if (attr != -1)
    {
        // if the bit 7 of attributes is set then the screen pixel colour must be SHADE0 before changing colour
        if (TEST_BIT(attr, 7) && ((m_Pixels[offset + 2] != gb_colours[SHADE0].r) || (m_Pixels[offset + 1] != gb_colours[SHADE0].g) || (m_Pixels[offset] != gb_colours[SHADE0].b)))
        {
            return;
        }
    }

    m_Pixels[offset]     = colour.b;
    m_Pixels[offset + 1] = colour.g;
    m_Pixels[offset + 2] = colour.r;
    m_Pixels[offset + 3] = 255;
}

rgb_tuple SimpleGPU::GetColour(int colourNum, uint16_t address)
{
    uint8_t palette = m_Emulator->m_MemControl->ReadByte(address);
    int hi, lo;

    switch (colourNum)
    {
    case 0:
        hi = 1;
        lo = 0;
        break;
    case 1:
        hi = 3;
        lo = 2;
        break;
    case 2:
        hi = 5;
        lo = 4;
        break;
    case 3:
        hi = 7;
        lo = 6;
        break;
    }

    return gb_colours[(GET_BIT(palette, hi) << 1) | GET_BIT(palette, lo)];
}

SimpleGPU::SimpleGPU(Emulator *emu)
  : LCDC(emu->m_MemControl->m_IO[0x40]),
    STAT(emu->m_MemControl->m_IO[0x41]),
    SCY(emu->m_MemControl->m_IO[0x42]),
    SCX(emu->m_MemControl->m_IO[0x43]),
    LY(emu->m_MemControl->m_IO[0x44]),
    LYC(emu->m_MemControl->m_IO[0x45]),
    BGP(emu->m_MemControl->m_IO[0x47]),
    OBP0(emu->m_MemControl->m_IO[0x48]),
    OBP1(emu->m_MemControl->m_IO[0x49]),
    WY(emu->m_MemControl->m_IO[0x4A]),
    WX(emu->m_MemControl->m_IO[0x4B])
{
    m_Emulator = emu;
}

SimpleGPU::~SimpleGPU()
{
    
}

void SimpleGPU::Init()
{
    LY = 0x00;
    std::fill(m_Pixels.begin(), m_Pixels.end(), 0);
}

void SimpleGPU::Reset()
{
    LCDC = 0x91;
    STAT = 0x85;
    SCY = 0x00;
    SCX = 0x00;
    LY = 0x00;
    LYC = 0x00;
    BGP = 0xFC;
    OBP0 = 0xFF;
    OBP1 = 0xFF;
    WY = 0x00;
    WX = 0x00;
}

/* TODO when running boot run, the emulator takes a while to exit this loop

LD A,($FF00+$44)    ; $0064  wait for screen frame
CP $90      ; $0066
JR NZ, Addr_0064    ; $0068
DEC C           ; $006a
JR NZ, Addr_0064    ; $006b

figure out why
*/
void SimpleGPU::Update(int cycles)
{
    SimpleGPU::UpdateLCDStatus();

    if (!SimpleGPU::bLCDEnabled()) return;

    m_Counter -= cycles;

    if (m_Counter <= 0)
    {
        m_Counter = DOTS_PER_SCANLINE;
        LY++;

        if (LY == 144)
        {
            m_Emulator->m_IntManager->RequestInterrupt(INT_VBLANK);
        }
        else if (LY == 154)
        {
            LY = 0;
        }
        else if (LY < 144)
        {
            SimpleGPU::DrawCurrentLine();
        }
    }
}

int SimpleGPU::GetMode()
{
    return STAT & 0b00000011;
}

void SimpleGPU::Debug_PrintStatus()
{
    std::cout << "*GPU STATUS*" << std::endl;
    // TODO
    std::cout << std::endl;
}

void MemoryController::InitBootROM()
{
    std::ifstream istream(BOOT_ROM_PATH, std::ios::in | std::ios::binary);

    if (!istream)
    {
        throw std::system_error(errno, std::system_category(), "failed to open " + BOOT_ROM_PATH);
    }

    istream.read(reinterpret_cast<char *>(m_BootROM.data()), 0x100);

    istream.close();
}

void MemoryController::DMATransfer(uint8_t data)
{
    uint16_t address = data << 8;

    for (int i = 0; i < 0xA0; ++i)
    {
        MemoryController::WriteByte(0xFE00 + i, MemoryController::ReadByte(address + i));
    }
}

MemoryController::MemoryController(Emulator *emu)
{
    m_Emulator = emu;

    if (ENABLE_BOOT_ROM)
    {
        MemoryController::InitBootROM();
    }
}

MemoryController::~MemoryController()
{
    
}

void MemoryController::Init()
{
    std::generate(m_VRAM.begin(), m_VRAM.end(), std::rand);
    std::generate(m_WRAM.begin(), m_WRAM.end(), std::rand);
    std::generate(m_OAM.begin(), m_OAM.end(), std::rand);
    m_IO.fill(0);
    std::generate(m_HRAM.begin(), m_HRAM.end(), std::rand);
}

void MemoryController::LoadROM(const std::string& rom_file)
{
    std::ifstream istream(rom_file, std::ios::in | std::ios::binary);

    if (!istream)
    {
        throw std::system_error(errno, std::system_category(), "failed to open " + rom_file);
    }

    istream.seekg(0, std::ios::end);
    size_t length = istream.tellg();
    istream.seekg(0, std::ios::beg);

    if (length > m_Cartridge.size())
    {
        length = m_Cartridge.size();
    }

    istream.read(reinterpret_cast<char *>(m_Cartridge.data()), length);

    istream.close();

    if (!ENABLE_BOOT_ROM)
    {
        m_Emulator->ResetComponents();
    }
    else
    {
        inBootMode = true;
    }
}

void MemoryController::ReloadROM()
{
    if (!ENABLE_BOOT_ROM)
    {
        m_Emulator->ResetComponents();
    }
    else
    {
        inBootMode = true;
    }
}

uint8_t MemoryController::ReadByte(uint16_t address) const
{
    if (inBootMode && address <= 0xFF)
    {
        return m_BootROM[address];
    }
    else if (address < 0x8000)
    {
        return m_Cartridge[address];
    }
    if (address >= 0x8000 && address < 0xA000)
    {
        return m_VRAM[address - 0x8000];
    }
    else if (address >= 0xA000 && address < 0xC000)
    {
#ifndef GUI_MODE
        std::cout << "WARNING: Attempted to read from $A000-$BFFF; Probably need to implement MBC; address=" << int_to_hex(address) << std::endl;
#else
        LOG_IF_F(WARNING, true, "Attempted to read from $A000-$BFFF; Probably need to implement MBC; address=%04X", address);
#endif
        return 0x00;
    }
    else if (address >= 0xC000 && address < 0xE000)
    {
        return m_WRAM[address - 0xC000];
    }
    else if (address >= 0xE000 && address < 0xFE00)
    {
        return ReadByte(address - 0x2000);
    }
    else if (address >= 0xFE00 && address < 0xFEA0)
    {
        return m_OAM[address - 0xFE00];
    }
    else if (address >= 0xFEA0 && address < 0xFF00)
    {
#ifndef GUI_MODE
        std::cout << "WARNING: Attempted to read from $FEA0-$FEFF; address=" << int_to_hex(address) << std::endl;
#else
        LOG_IF_F(WARNING, true, "Attempted to read from $FEA0-$FEFF; address=%04X", address);
#endif
        return 0x00;
    }
    else if (address == 0xFF00)
    {
        return m_Emulator->m_JoyPad->ReadP1();
    }
    else if (address == 0xFF04)
    {
        return m_Emulator->m_Timer->DIV >> 8;
    }
    else if (address >= 0xFF00 && address < 0xFF80)
    {
        return m_IO[address - 0xFF00];
    }
    else if (address >= 0xFF80 && address < 0xFFFF)
    {
        return m_HRAM[address - 0xFF80];
    }
    else if (address == 0xFFFF)
    {
        return IE;
    }
    else
    {
#ifndef GUI_MODE
        std::cerr << "MemoryController::ReadByte: Invalid address range: " << int_to_hex(address) << std::endl;
        m_Emulator->m_CPU->Debug_PrintStatus();
#else
        LOG_IF_F(ERROR, true, "MemoryController::ReadByte: Invalid address range: %04X", address);
#endif
        throw std::runtime_error("Bad memory access!");
        return 0x0;
    }
}

uint16_t MemoryController::ReadWord(uint16_t address) const
{
    uint8_t low = MemoryController::ReadByte(address);
    uint8_t high = MemoryController::ReadByte(address + 1);
    return (high << 8) | low;
}

void MemoryController::WriteByte(uint16_t address, uint8_t data)
{
    if (inBootMode && address <= 0xFF)
    {
#ifndef GUI_MODE
        std::cout << "WARNING: Attempted to write to $00-$FF; address=" << int_to_hex(address) << std::endl;
#else
        LOG_IF_F(WARNING, true, "Attempted to write to $00-$FF; address=%04X", address);
#endif
    }
    else if (address < 0x8000)
    {
#ifndef GUI_MODE
        std::cout << "WARNING: Attempted to write to $0000-$7FFF; address=" << int_to_hex(address) << std::endl;
#else
        LOG_IF_F(WARNING, true, "Attempted to write to $0000-$7FFF; address=%04X", address);
#endif
    }
    else if (address >= 0x8000 && address < 0xA000)
    {
        m_VRAM[address - 0x8000] = data;
    }
    else if (address >= 0xA000 && address < 0xC000)
    {
#ifndef GUI_MODE
        std::cout << "WARNING: Attempted to write to $A000-$BFFF; Probably need to implement MBC; address=" << int_to_hex(address) << std::endl;
#else
        LOG_IF_F(WARNING, true, "Attempted to write to $A000-$BFFF; Probably need to implement MBC; address=%04X", address);
#endif
    }
    else if (address >= 0xC000 && address < 0xE000)
    {
        m_WRAM[address - 0xC000] = data;
    }
    else if (address >= 0xE000 && address < 0xFE00)
    {
        WriteByte(address - 0x2000, data);
    }
    else if (address >= 0xFE00 && address < 0xFEA0)
    {
        m_OAM[address - 0xFE00] = data;
    }
    else if (address >= 0xFEA0 && address < 0xFF00)
    {
#ifndef GUI_MODE
        std::cout << "WARNING: Attempted to write to $FEA0-$FEFF; address=" << int_to_hex(address) << std::endl;
#else
        LOG_IF_F(WARNING, true, "Attempted to write to $FEA0-$FEFF; address=%04X", address);
#endif
    }
    else if (address == 0xFF04)
    {
        m_Emulator->m_Timer->DIV = 0;
    }
    else if (address == 0xFF07)
    {
        int oldfreq = m_Emulator->m_Timer->ClockSelect();
        m_Emulator->m_Timer->TAC = data;
        int newfreq = m_Emulator->m_Timer->ClockSelect();

        if (oldfreq != newfreq)
        {
            m_Emulator->m_Timer->UpdateFreq();
        }
    }
    else if (address == 0xFF44)
    {
        m_Emulator->m_GPU->LY = 0;
    }
    else if (address == 0xFF46)
    {
        MemoryController::DMATransfer(data);
    }
    else if (inBootMode && address == 0xFF50)
    {
        m_IO[0x50] = data;
        inBootMode = false;
        m_Emulator->ResetComponents();
    }
    else if (address >= 0xFF00 && address < 0xFF80)
    {
        m_IO[address - 0xFF00] = data;
    }
    else if (address >= 0xFF80 && address < 0xFFFF)
    {
        m_HRAM[address - 0xFF80] = data;
    }
    else if (address == 0xFFFF)
    {
        IE = data;
    }
    else
    {
#ifndef GUI_MODE
        std::cerr << "MemoryController::WriteByte: Invalid address range: " << int_to_hex(address) << std::endl;
        m_Emulator->m_CPU->Debug_PrintStatus();
#else
        LOG_IF_F(ERROR, true, "MemoryController::WriteByte: Invalid address range: %04X", address);
#endif
        throw std::runtime_error("Bad memory access!");
    }
}

void MemoryController::WriteWord(uint16_t address, uint16_t data)
{
    uint8_t high = data >> 8;
    uint8_t low = data & 0xFF;
    MemoryController::WriteByte(address, low);
    MemoryController::WriteByte(address + 1, high);
}

void MemoryController::Debug_PrintMemory(uint16_t address)
{
    std::cout << "memory value at address " << int_to_hex(address) << " is " << int_to_hex(+MemoryController::ReadByte(address), 2) << std::endl;
}

void MemoryController::Debug_EditMemory(uint16_t address, uint8_t data)
{
    MemoryController::WriteByte(address, data);
    std::cout << "memory value at address " << int_to_hex(address) << " is now changed to " << int_to_hex(+data, 2) << std::endl;
}

void MemoryController::Debug_PrintMemoryRange(uint16_t start, uint16_t end)
{
    std::cout << "address: value" << std::endl;
    for (uint16_t addr = start; addr < end; ++addr)
    {
        std::cout << int_to_hex(addr) << ": " << int_to_hex(+MemoryController::ReadByte(addr), 2) << std::endl;
    }
}

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

Emulator::Emulator()
{
    m_CPU = std::make_unique<CPU>(this);
    m_MemControl = std::make_unique<MemoryController>(this);
    m_IntManager = std::make_unique<InterruptManager>(this);
    m_Timer = std::make_unique<Timer>(this);
    m_GPU = std::make_unique<SimpleGPU>(this);
    m_JoyPad = std::make_unique<JoyPad>(this);
}

Emulator::~Emulator()
{
    
}

void Emulator::InitComponents()
{
    m_CPU->Init();
    m_IntManager->Init();
    m_Timer->Init();
    m_GPU->Init();
    m_JoyPad->Init();
    m_TotalCycles = m_PrevTotalCycles = 0;
}

void Emulator::ResetComponents()
{
    m_CPU->Reset();
    m_IntManager->Reset();
    m_Timer->Reset();
    m_GPU->Reset();
    m_JoyPad->Reset();
    m_TotalCycles = m_PrevTotalCycles = 0;
}

void Emulator::Update()
{
    while ((m_TotalCycles - m_PrevTotalCycles) <= MAX_CYCLES)
    {
        int initial_cycles = m_TotalCycles;
        m_CPU->Step();
        int cycles = m_TotalCycles - initial_cycles;
        m_GPU->Update(cycles);
        m_Timer->Update(cycles);
    }

    m_PrevTotalCycles = m_TotalCycles;
}

void Emulator::Tick()
{
    m_TotalCycles += 4;
}

void Emulator::Debug_Step(std::vector<char>& blargg_serial, int times)
{
    for (int i = 0; i < times; ++i)
    {
        if ((m_TotalCycles - m_PrevTotalCycles) > MAX_CYCLES)
        {
            m_PrevTotalCycles = m_TotalCycles;
        }

        int initial_cycles = m_TotalCycles;
        m_CPU->Step();
        int cycles = m_TotalCycles - initial_cycles;
        m_GPU->Update(cycles);
        m_Timer->Update(cycles);

        // blarggs test - serial output
        if (m_MemControl->ReadByte(0xFF02) == 0x81)
        {
            blargg_serial.push_back(m_MemControl->ReadByte(0xFF01));
            m_MemControl->WriteByte(0xFF02, 0x0);
        }
    }
}

void Emulator::Debug_StepTill(std::vector<char>& blargg_serial, uint16_t x)
{
    while (m_CPU->GetPC() != x)
    {
        Emulator::Debug_Step(blargg_serial, 1);
    }
}

void Emulator::Debug_PrintEmulatorStatus()
{
    m_CPU->Debug_PrintStatus();
    m_IntManager->Debug_PrintStatus();
    m_Timer->Debug_PrintStatus();
    m_GPU->Debug_PrintStatus();
    m_JoyPad->Debug_PrintStatus();
}

#ifdef GUI_MODE

GameBoyWindows::GameBoyWindows()
{
    m_Emulator = std::make_unique<Emulator>();
}

GameBoyWindows::~GameBoyWindows()
{
    
}

void GameBoyWindows::Initialize()
{
    m_Emulator->InitComponents();
}

void GameBoyWindows::LoadROM(const std::string& rom_file)
{
    m_Emulator->InitComponents();
    m_Emulator->m_MemControl->LoadROM(rom_file);
    LOG_F(INFO, "Loaded %s", rom_file.c_str());
}

void GameBoyWindows::ReloadROM()
{
    m_Emulator->InitComponents();
    m_Emulator->m_MemControl->ReloadROM();
    LOG_F(INFO, "Current ROM reloaded");
}

void GameBoyWindows::Create(HWND hWnd)
{
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
    {
        PopMessageBox(TEXT("SDL Error"), MB_OK | MB_ICONERROR, "Couldn't initialize SDL: %s", SDL_GetError());
        std::exit(1);
    }

    m_Window = SDL_CreateWindowFrom(hWnd);
    if (m_Window == nullptr)
    {
        PopMessageBox(TEXT("SDL Error"), MB_OK | MB_ICONERROR, "Couldn't create window: %s", SDL_GetError());
        std::exit(1);
    }

    m_Renderer = SDL_CreateRenderer(m_Window, -1, SDL_RENDERER_ACCELERATED);
    if (m_Renderer == nullptr)
    {
        PopMessageBox(TEXT("SDL Error"), MB_OK | MB_ICONERROR, "Couldn't create renderer: %s", SDL_GetError());
        std::exit(1);
    }

    m_Texture = SDL_CreateTexture(m_Renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);
    if (m_Texture == nullptr)
    {
        PopMessageBox(TEXT("SDL Error"), MB_OK | MB_ICONERROR, "Couldn't create texture: %s", SDL_GetError());
        std::exit(1);
    }
}

void GameBoyWindows::FixSize()
{
    SDL_SetWindowSize(m_Window, SCREEN_WIDTH * SCREEN_SCALE_FACTOR, SCREEN_HEIGHT * SCREEN_SCALE_FACTOR);
}

void GameBoyWindows::Update()
{
    m_Emulator->Update();
}

void GameBoyWindows::RenderGraphics()
{
    SDL_SetRenderDrawColor(m_Renderer, 0, 0, 0, 255);
    SDL_RenderClear(m_Renderer);
    SDL_UpdateTexture(m_Texture, nullptr, m_Emulator->m_GPU->m_Pixels.data(), SCREEN_WIDTH * 4);
    SDL_RenderCopy(m_Renderer, m_Texture, nullptr, nullptr);
    SDL_RenderPresent(m_Renderer);
}

void GameBoyWindows::HandleKeyDown(WPARAM wParam)
{
    switch (wParam)
    {
    case 0x41: // a key
        m_Emulator->m_JoyPad->PressButton(JOYPAD_A);
        break;
    case 0x42: // b key
        m_Emulator->m_JoyPad->PressButton(JOYPAD_B);
        break;
    case VK_RETURN:
        m_Emulator->m_JoyPad->PressButton(JOYPAD_START);
        break;
    case VK_SPACE:
        m_Emulator->m_JoyPad->PressButton(JOYPAD_SELECT);
        break;
    case VK_RIGHT:
        m_Emulator->m_JoyPad->PressButton(JOYPAD_RIGHT);
        break;
    case VK_LEFT:
        m_Emulator->m_JoyPad->PressButton(JOYPAD_LEFT);
        break;
    case VK_UP:
        m_Emulator->m_JoyPad->PressButton(JOYPAD_UP);
        break;
    case VK_DOWN:
        m_Emulator->m_JoyPad->PressButton(JOYPAD_DOWN);
        break;
    }
}

void GameBoyWindows::HandleKeyUp(WPARAM wParam)
{
    switch (wParam)
    {
    case 0x41: // a key
        m_Emulator->m_JoyPad->ReleaseButton(JOYPAD_A);
        break;
    case 0x42: // b key
        m_Emulator->m_JoyPad->ReleaseButton(JOYPAD_B);
        break;
    case VK_RETURN:
        m_Emulator->m_JoyPad->ReleaseButton(JOYPAD_START);
        break;
    case VK_SPACE:
        m_Emulator->m_JoyPad->ReleaseButton(JOYPAD_SELECT);
        break;
    case VK_RIGHT:
        m_Emulator->m_JoyPad->ReleaseButton(JOYPAD_RIGHT);
        break;
    case VK_LEFT:
        m_Emulator->m_JoyPad->ReleaseButton(JOYPAD_LEFT);
        break;
    case VK_UP:
        m_Emulator->m_JoyPad->ReleaseButton(JOYPAD_UP);
        break;
    case VK_DOWN:
        m_Emulator->m_JoyPad->ReleaseButton(JOYPAD_DOWN);
        break;
    }
}

void GameBoyWindows::CleanUp()
{
    SDL_DestroyTexture(m_Texture);
    m_Texture = nullptr;

    SDL_DestroyRenderer(m_Renderer);
    m_Renderer = nullptr;

    SDL_DestroyWindow(m_Window);
    m_Window = nullptr;

    SDL_Quit();
}

void GameBoyWindows::Destroy()
{
    PostQuitMessage(0);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static GameBoyWindows gb;

    switch (msg)
    {
    case WM_CREATE:
    {
        gb.Create(hWnd);

        HMENU hMenuBar = CreateMenu();
        HMENU hFile = CreatePopupMenu();
        HMENU hHelp = CreatePopupMenu();

        AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR) hFile, "File");
        AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR) hHelp, "Help");

        AppendMenu(hFile, MF_STRING, ID_LOAD_ROM, "Load ROM");
        AppendMenu(hFile, MF_STRING, ID_RELOAD_ROM, "Reload ROM");
        AppendMenu(hFile, MF_STRING, ID_EXIT, "Exit");

        AppendMenu(hHelp, MF_STRING, ID_ABOUT, "About");

        SetMenu(hWnd, hMenuBar);

        gb.FixSize(); // resize because we just added the menubar
        gb.Initialize();

        break;
    }
    case WM_TIMER:
    {
        gb.Update();
        InvalidateRect(hWnd, NULL, FALSE);
        break;
    }
    case WM_PAINT:
    {
        gb.RenderGraphics();
        break;
    }
    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case ID_LOAD_ROM:
        {
            OPENFILENAME ofn;
            char szFileName[MAX_PATH] = "";
            ZeroMemory(&ofn, sizeof(ofn));

            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hWnd;
            ofn.lpstrFilter = "ROM files (*.gb)\0*.gb\0";
            ofn.lpstrFile = szFileName;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
            ofn.lpstrDefExt = "gb";

            if(GetOpenFileName(&ofn))
            {
                gb.LoadROM(szFileName);

                if(!SetTimer(hWnd, ID_TIMER, UPDATE_INTERVAL, NULL))
                {
                    MessageBox(hWnd, "Could not set timer!", "Error", MB_OK | MB_ICONEXCLAMATION);
                    PostQuitMessage(1);
                }
            }
            break;
        }
        case ID_RELOAD_ROM:
        {
            gb.ReloadROM();

            if(!SetTimer(hWnd, ID_TIMER, UPDATE_INTERVAL, NULL))
            {
                MessageBox(hWnd, "Could not set timer!", "Error", MB_OK | MB_ICONEXCLAMATION);
                PostQuitMessage(1);
            }
            break;
        }
        case ID_EXIT:
        {
            PostQuitMessage(0);
            break;
        }
        case ID_ABOUT:
        {
            MessageBox(hWnd, TEXT("um..."), TEXT("About Noufu"), MB_ICONINFORMATION | MB_OK);
            break;
        }
        }
        break;
    }
    case WM_KEYDOWN:
    {
        gb.HandleKeyDown(wParam);
        break;
    }
    case WM_KEYUP:
    {
        gb.HandleKeyUp(wParam);
        break;
    }
    case WM_CLOSE:
    {
        gb.CleanUp();
        KillTimer(hWnd, ID_TIMER);
        DestroyWindow(hWnd);
        break;
    }
    case WM_DESTROY:
        gb.Destroy();
        break;
    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }

    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    loguru::init(__argc, __argv);
    loguru::add_file("emulation_log.txt", loguru::Append, loguru::Verbosity_MAX);

    std::srand(unsigned(std::time(nullptr)));
    ConfigureEmulatorSettings();

    const TCHAR szClassName[] = TEXT("MyClass");

    WNDCLASS wc;
    HWND hWnd;
    MSG msg;

    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = szClassName;
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClass(&wc))
    {
        MessageBox(NULL, TEXT("Window Registration Failed!"), TEXT("Error!"),
            MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    hWnd = CreateWindow(szClassName,
        TEXT(EMULATOR_NAME.c_str()),
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, SCREEN_WIDTH * SCREEN_SCALE_FACTOR, SCREEN_HEIGHT * SCREEN_SCALE_FACTOR,
        NULL, NULL, hInstance, NULL);

    if (hWnd == NULL)
    {
        MessageBox(NULL, TEXT("Window Creation Failed!"), TEXT("Error!"),
            MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int) msg.wParam;
}

#else

int main(int argc, char *argv[])
{
    std::srand(unsigned(std::time(nullptr)));
    ConfigureEmulatorSettings();

    std::unique_ptr<Emulator> emu = std::make_unique<Emulator>();

    emu->InitComponents();
    emu->m_MemControl->LoadROM(ROM_FILE_PATH /*argv[1]*/);

    std::vector<char> blargg_serial;
    std::string input_str;

    std::cout << "Noufu - cli debugger mode" << std::endl;

    fout.open("gblog.txt", std::ofstream::out | std::ofstream::trunc);

    while (true)
    {
        std::cout << ">> ";
        std::getline(std::cin, input_str);

        auto iss = std::istringstream{input_str};
        auto str = std::string{};

        iss >> str;
        if (str == "step")
        {
            int x = 1;

            if (iss >> str)
            {
                x = int_from_string(str, false);
            }

            emu->Debug_Step(blargg_serial, x);

            std::cout << "stepped " << x << " time(s)" << std::endl;
        }
        else if (str == "quit")
        {
            break;
        }
        else if (str == "print")
        {
            iss >> str;
            if (str == "status")
            {
                iss >> str;
                if (str == "all")
                {
                    emu->Debug_PrintEmulatorStatus();
                }
                else if (str == "cpu")
                {
                    emu->m_CPU->Debug_PrintStatus();
                }
                else if (str == "intman")
                {
                    emu->m_IntManager->Debug_PrintStatus();
                }
                else if (str == "timer")
                {
                    emu->m_Timer->Debug_PrintStatus();
                }
                else if (str == "gpu")
                {
                    emu->m_GPU->Debug_PrintStatus();
                }
                else if (str == "joypad")
                {
                    emu->m_JoyPad->Debug_PrintStatus();
                }
            }
            else if (str == "mem")
            {
                iss >> str;
                if (str == "range")
                {
                    iss >> str;
                    uint16_t start = (uint16_t) int_from_string(str, true);
                    iss >> str;
                    uint16_t end = (uint16_t) int_from_string(str, true);
                    emu->m_MemControl->Debug_PrintMemoryRange(start, end);
                }
                else
                {
                    emu->m_MemControl->Debug_PrintMemory((uint16_t) int_from_string(str, true));
                }
            }
            else if (str == "ins")
            {
                emu->m_CPU->Debug_PrintCurrentInstruction();
            }
        }
        else if (str == "edit")
        {
            iss >> str;
            if (str == "reg8")
            {
                iss >> str;
                int r8 = int_from_string(str, false);
                iss >> str;
                int v = int_from_string(str, true);
                emu->m_CPU->Debug_EditRegister(r8, v, true);
            }
            else if (str == "flag")
            {
                iss >> str;
                int f = int_from_string(str, false);
                iss >> str;
                int v = int_from_string(str, false);
                emu->m_CPU->Debug_EditFlag(f, v);
            }
            else if (str == "reg16")
            {
                iss >> str;
                int r16 = int_from_string(str, false);
                iss >> str;
                int v = int_from_string(str, true);
                emu->m_CPU->Debug_EditRegister(r16, v, false);
            }
            else if (str == "mem")
            {
                iss >> str;
                int addr = int_from_string(str, true);
                iss >> str;
                int v = int_from_string(str, true);
                emu->m_MemControl->Debug_EditMemory(addr, v);
            }
        }
        else if (str == "blargg")
        {
            std::string blargg_str(blargg_serial.begin(), blargg_serial.end());
            std::cout << blargg_str << std::endl;
        }
        else if (str == "run")
        {
            iss >> str;
            int x = int_from_string(str, true);
            emu->Debug_StepTill(blargg_serial, (uint16_t) x);
        }
        else if (str == "reload-rom")
        {
            emu->InitComponents();
            emu->m_MemControl->ReloadROM();
            blargg_serial.clear();
            fout.close();
            fout.open("gblog.txt", std::ofstream::out | std::ofstream::trunc);
        }
    }

    fout.close();

    return 0;
}

#endif