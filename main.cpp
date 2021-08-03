// Gameboy skeleton with debugger
/*
Stage 1: Pass Blargg's CPU tests (complete)
Stage 1.5: Implement `get-ppm` debug command
Stage 2: Refactor code (make seperate files for each class) and implement PPU to run Boot rom (don't forgot to make a Makefile)
Stage 3: Pass other Blargg's tests and make sure it can run Tetris without problems
Stage 4: Implement a seperate class for cartridge with basic MBC support and make sure it can run Mario
*/
/*
debug cmds:
step ([x]) - step cpu for x times (x=1 is default)
quit - quit debugger
print status [all cpu intman timer]
print mem [addr] - print value in mem bus at addr
print mem range [start] [end] - print memory values from start to end
print ins - print current instruction
edit reg8 [r8] [v] - change the value of register r8 with v
edit flag [f] [v] - change the value of flag f with v
edit reg16 [r16] [v] - change the value of register r16 with v
edit mem [addr] [v] - change the value of mem addr with v
blargg - display serial output results
run x - run till PC=x (x is hex)
reload-rom

TODO:
get-ppm - create a ppm file of current screen
print rom-info - print basic rom information
save-state fname - save current emulator state to fname
load-state fname - load current emulator state from fname

notes: both addr and v are hex
*/
/*
blargg tests passed:
- 01-special.gb
- 02-interrupts.gb**
- 03-op sp,hl.gb
- 04-op r,imm.gb
- 05-op rp.gb
- 06-ld r,r.gb
- 07-jr,jp,call,ret,rst.gb
- 08-misc instrs.gb
- 09-op r,r.gb
- 10-bit ops.gb
- 11-op a,(hl).gb

**halt is taking too long... perhaps it will be solved after PPU implementation is complete...
*/
// TODO optimize code? proper cli, label memory regions for debugging

#include <array>
#include <bitset>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <sstream>
#include <string>
#include <vector>

#include <cstdio>

using namespace std::chrono;

// #define NDEBUG

// Maximum number of cycles per update
const int MAX_CYCLES = 70224;         // 154 scanlines * 456 cycles per frame = 70224

// Delay between updates
const float UPDATE_INTERVAL = 16.744; // 1000 ms / 59.72 fps

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

std::ofstream fout;

inline uint64_t unix_epoch_millis()
{
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

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

class GBComponent;
class Cpu;
class InterruptManager;
class Timer;
class MemoryController;
class CpuMemoryController;
class Timer;
class Emulator;

class GBComponent
{
public:
    virtual void Reset() = 0;
    virtual void Debug_PrintStatus() = 0;
protected:
    Emulator *m_Emulator;
};

class Cpu : GBComponent
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

    void WriteByte(uint16_t address, uint8_t data);
    void WriteWord(uint16_t address, uint16_t data);
    uint8_t ReadByte(uint16_t address) const;
    uint16_t ReadWord(uint16_t address) const;
    uint8_t NextByte();
    uint16_t NextWord();

    bool IsFlagSet(int flag);
    void ModifyFlag(int flag, int value);
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
    Cpu(Emulator *emu);
    ~Cpu();

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

    bool GetIME();
    void EnableIME();
    void DisableIME();
    void Reset();
    bool SetHaltMode(); // sets halt mode and determines if halt should be executed or not
    void ResetHaltMode();
    void RequestInterrupt(int i);
    void InterruptRoutine();

    void Debug_PrintStatus();
};

class Timer : GBComponent
{
private:
    Emulator *m_Emulator;

    int m_Counter;
public:
    Timer(Emulator *emu);
    ~Timer();

    uint16_t DIV;

    void Reset();
    void Update(int cycles);
    void UpdateCounter();
    int TimerEnable();
    int ClockSelect();

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
public:
    MemoryController(Emulator *emu);
    ~MemoryController();

    void Reset();
    void LoadROM(const std::string& rom_file);
    void WriteByte(uint16_t address, uint8_t data);
    void WriteWord(uint16_t address, uint16_t data);
    uint8_t ReadByte(uint16_t address) const;
    uint16_t ReadWord(uint16_t address) const;

    void Debug_PrintStatus() {} // nothing
    void Debug_PrintMemory(uint16_t address);
    void Debug_EditMemory(uint16_t address, uint8_t data);
    void Debug_PrintMemoryRange(uint16_t start, uint16_t end);

    std::array<uint8_t, 0x80> m_IO;
    uint8_t &IF = m_IO[0x0F];
    uint8_t IE;

    uint8_t &TIMA = m_IO[0x05];
    uint8_t &TMA = m_IO[0x06];
    uint8_t &TAC = m_IO[0x07];
};

class Emulator
{
public:
    Emulator();
    ~Emulator();

    void ResetComponents();
    void Update();
    void Tick(); // + 1 M-cycle

    void Debug_Step(std::vector<char>& blargg_serial, int times);
    void Debug_StepTill(std::vector<char>& blargg_serial, uint16_t x);
    void Debug_PrintEmulatorStatus();

    // All components
    std::unique_ptr<Cpu> m_Cpu;
    std::unique_ptr<MemoryController> m_MemControl;
    std::unique_ptr<InterruptManager> m_IntManager;
    std::unique_ptr<Timer> m_Timer;

    int m_TotalCycles;
    int m_PrevTotalCycles;
};

void Cpu::WriteByte(uint16_t address, uint8_t data)
{
    m_Emulator->Tick();
    m_Emulator->m_MemControl->WriteByte(address, data);
}

void Cpu::WriteWord(uint16_t address, uint16_t data)
{
    m_Emulator->Tick();
    m_Emulator->Tick();
    m_Emulator->m_MemControl->WriteWord(address, data);
}

uint8_t Cpu::ReadByte(uint16_t address) const
{
    m_Emulator->Tick();
    return m_Emulator->m_MemControl->ReadByte(address);
}

uint16_t Cpu::ReadWord(uint16_t address) const
{
    m_Emulator->Tick();
    m_Emulator->Tick();
    return m_Emulator->m_MemControl->ReadWord(address);
}

uint8_t Cpu::NextByte()
{
    return Cpu::ReadByte(PC++);
}

uint16_t Cpu::NextWord()
{
    uint16_t word = Cpu::ReadWord(PC);
    PC += 2;
    return word;
}

bool Cpu::IsFlagSet(int flag)
{
    return TEST_BIT(F, flag);
}

void Cpu::ModifyFlag(int flag, int value)
{
    F = (F & ~(1 << flag)) | (value << flag);
}

void Cpu::Push(uint16_t data)
{
    Cpu::WriteByte(--SP, data >> 8); // hi
    Cpu::WriteByte(--SP, data & 0xFF); // lo
}

uint16_t Cpu::Pop()
{
    uint16_t data = Cpu::ReadWord(SP);
    SP += 2;
    return data;
}

bool Cpu::Condition(int i)
{
    bool result = false;

    switch (i)
    {
    case 0: result = !Cpu::IsFlagSet(FLAG_Z); break;
    case 1: result =  Cpu::IsFlagSet(FLAG_Z); break;
    case 2: result = !Cpu::IsFlagSet(FLAG_C); break;
    case 3: result =  Cpu::IsFlagSet(FLAG_C); break;
    }

    if (result) m_Emulator->Tick();

    return result;
}

void Cpu::Call(uint16_t addr)
{
    Cpu::Push(PC);
    PC = addr;
}

void Cpu::RotateBitsLeft(uint8_t &n, bool circular, bool resetZ)
{
    bool bIsBit7Set = TEST_BIT(n, 7);
    n <<= 1;
    if (circular ? bIsBit7Set : Cpu::IsFlagSet(FLAG_C)) SET_BIT(n, 0);
    Cpu::ModifyFlag(FLAG_Z, resetZ ? 0 : n == 0);
    Cpu::ModifyFlag(FLAG_N, 0);
    Cpu::ModifyFlag(FLAG_H, 0);
    Cpu::ModifyFlag(FLAG_C, bIsBit7Set);
}

void Cpu::RotateBitsRight(uint8_t &n, bool circular, bool resetZ)
{
    bool bIsBit0Set = TEST_BIT(n, 0);
    n >>= 1;
    if (circular ? bIsBit0Set : Cpu::IsFlagSet(FLAG_C)) SET_BIT(n, 7);
    Cpu::ModifyFlag(FLAG_Z, resetZ ? 0 : n == 0);
    Cpu::ModifyFlag(FLAG_N, 0);
    Cpu::ModifyFlag(FLAG_H, 0);
    Cpu::ModifyFlag(FLAG_C, bIsBit0Set);
}

void Cpu::ShiftBitsLeft(uint8_t &n)
{
    bool bIsBit7Set = TEST_BIT(n, 7);
    n <<= 1;
    Cpu::ModifyFlag(FLAG_Z, n == 0);
    Cpu::ModifyFlag(FLAG_N, 0);
    Cpu::ModifyFlag(FLAG_H, 0);
    Cpu::ModifyFlag(FLAG_C, bIsBit7Set);
}

void Cpu::ShiftBitsRight(uint8_t &n, bool logical)
{
    bool bIsBit0Set = TEST_BIT(n, 0);
    bool bIsBit7Set = TEST_BIT(n, 7);
    n >>= 1;
    if (!logical && bIsBit7Set) SET_BIT(n, 7); // The content of bit 7 is unchanged. (see the gameboy programming manual)
    Cpu::ModifyFlag(FLAG_Z, n == 0);
    Cpu::ModifyFlag(FLAG_N, 0);
    Cpu::ModifyFlag(FLAG_H, 0);
    Cpu::ModifyFlag(FLAG_C, bIsBit0Set);
}

void Cpu::HandlePrefixCB()
{
    uint8_t opcode;

    switch (opcode = Cpu::NextByte())
    {
    /* rl(c) r8 */
    case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x07: // rlc r8
    case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x17: // rl r8
    {
        Cpu::RotateBitsLeft(*reg8_group[opcode & 7], !((opcode >> 3) & 7));
        break;
    }
    /* rl(c) (HL) */
    case 0x06: case 0x16:
    {
        uint8_t hl = Cpu::ReadByte(HL);
        Cpu::RotateBitsLeft(hl, opcode == 0x06);
        Cpu::WriteByte(HL, hl);
        break;
    }
    /* rr(c) r8 */
    case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D: case 0x0F: // rrc r8
    case 0x18: case 0x19: case 0x1A: case 0x1B: case 0x1C: case 0x1D: case 0x1F: // rr r8
    {
        Cpu::RotateBitsRight(*reg8_group[(opcode & 0x0F) - 0x08], !((opcode >> 4) & 0x0F));
        break;
    }
    /* rr(c) (HL) */
    case 0x0E: case 0x1E:
    {
        uint8_t hl = Cpu::ReadByte(HL);
        Cpu::RotateBitsRight(hl, opcode == 0x0E);
        Cpu::WriteByte(HL, hl);
        break;
    }
    /* sla r8 */
    case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x27: // sla r8
    {
        Cpu::ShiftBitsLeft(*reg8_group[opcode - 0x20]);
        break;
    }
    /* sla (HL) */
    case 0x26:
    {
        uint8_t hl = Cpu::ReadByte(HL);
        Cpu::ShiftBitsLeft(hl);
        Cpu::WriteByte(HL, hl);
        break;
    }
    /* swap r8 */
    case 0x30: case 0x31: case 0x32: case 0x33: case 0x34: case 0x35: case 0x37: // swap r8
    {
        int idx = opcode - 0x30;
        uint8_t n = *reg8_group[idx];
        SWAP_NIBBLES8(n);
        Cpu::ModifyFlag(FLAG_Z, n == 0);
        Cpu::ModifyFlag(FLAG_N, 0);
        Cpu::ModifyFlag(FLAG_H, 0);
        Cpu::ModifyFlag(FLAG_C, 0);
        *reg8_group[idx] = n;
        break;
    }
    /* swap (HL) */
    case 0x36:
    {
        uint8_t hl = Cpu::ReadByte(HL);
        SWAP_NIBBLES8(hl);
        Cpu::ModifyFlag(FLAG_Z, hl == 0);
        Cpu::ModifyFlag(FLAG_N, 0);
        Cpu::ModifyFlag(FLAG_H, 0);
        Cpu::ModifyFlag(FLAG_C, 0);
        Cpu::WriteByte(HL, hl);
        break;
    }
    /* sra r8 */
    case 0x28: case 0x29: case 0x2A: case 0x2B: case 0x2C: case 0x2D: case 0x2F: // sra r8
    {
        Cpu::ShiftBitsRight(*reg8_group[opcode - 0x28]);
        break;
    }
    /* sra (HL) */
    case 0x2E:
    {
        uint8_t hl = Cpu::ReadByte(HL);
        Cpu::ShiftBitsRight(hl);
        Cpu::WriteByte(HL, hl);
        break;
    }
    /* srl r8 */
    case 0x38: case 0x39: case 0x3A: case 0x3B: case 0x3C: case 0x3D: case 0x3F: // srl r8
    {
        Cpu::ShiftBitsRight(*reg8_group[opcode - 0x38], true);
        break;
    }
    /* srl (HL) */
    case 0x3E:
    {
        uint8_t hl = Cpu::ReadByte(HL);
        Cpu::ShiftBitsRight(hl, true);
        Cpu::WriteByte(HL, hl);
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
        Cpu::ModifyFlag(FLAG_Z, !TEST_BIT(*reg8_group[opcode & 7], opcode >> 3 & 7));
        Cpu::ModifyFlag(FLAG_N, 0);
        Cpu::ModifyFlag(FLAG_H, 1);
        break;
    }
    /* bit n,(HL) */
    case 0x46: case 0x4E: case 0x56: case 0x5E: case 0x66: case 0x6E: case 0x76: case 0x7E:
    {
        uint8_t hl = Cpu::ReadByte(HL);
        Cpu::ModifyFlag(FLAG_Z, !TEST_BIT(hl, (opcode - 0x46) >> 3));
        Cpu::ModifyFlag(FLAG_N, 0);
        Cpu::ModifyFlag(FLAG_H, 1);
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
        uint8_t hl = Cpu::ReadByte(HL);
        SET_BIT(hl, (opcode - 0xC6) >> 3);
        Cpu::WriteByte(HL, hl);
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
        uint8_t hl = Cpu::ReadByte(HL);
        RES_BIT(hl, (opcode - 0x86) >> 3);
        Cpu::WriteByte(HL, hl);
        break;
    }
    default:
    {
        std::cerr << "Unimplemented opcode: " << int_to_hex(opcode) << std::endl;
        Cpu::Debug_PrintStatus();
        throw std::runtime_error("^^^");
        break;
    }
    }
}

Cpu::Cpu(Emulator *emu)
{
    m_Emulator = emu;
}

Cpu::~Cpu()
{
    delete m_Emulator;
}

void Cpu::Reset()
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

void Cpu::Step()
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

#ifndef NDEBUG
    Cpu::Debug_LogState();
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

    switch (opcode = Cpu::NextByte())
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
        *reg8_group[(opcode - 6) >> 3] = Cpu::NextByte();
        break;
    }
    /* ld r8,(HL) ; 8c */
    case 0x46: case 0x4E: case 0x56: case 0x5E: case 0x66: case 0x6E: case 0x7E: // ld r8,(HL)
    {
        *reg8_group[(opcode - 70) >> 3] = Cpu::ReadByte(HL);
        break;
    }
    /* ld (HL),r8 ; 8c */
    case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x77: // ld (HL),r8
    {
        Cpu::WriteByte(HL, *reg8_group[opcode - 0x70]);
        break;
    }
    /* ld (HL),n ; 12c */
    case 0x36:
    {
        Cpu::WriteByte(HL, Cpu::NextByte());
        break;
    }
    /* ld A,(r16-g2) ; 8c */
    case 0x0A: case 0x1A: case 0x2A: case 0x3A: // ld A,(r16-g2)
    {
        int idx = (opcode - 0x0A) >> 4;
        A = Cpu::ReadByte(*reg16_group2[idx]);
        HL += HL_add[idx];
        break;
    }
    /* ld A,(nn) ; 16c */
    case 0xFA:
    {
        A = Cpu::ReadByte(Cpu::NextWord());
        break;
    }
    /* ld (r16-g2),A ; 8c */
    case 0x02: case 0x12: case 0x22: case 0x32: // ld (r16-g2),A
    {
        int idx = (opcode - 0x02) >> 4;
        Cpu::WriteByte(*reg16_group2[idx], A);
        HL += HL_add[idx];
        break;
    }
    /* ld (nn),A ; 16c */
    case 0xEA:
    {
        Cpu::WriteByte(Cpu::NextWord(), A);
        break;
    }
    /* ld A,(FF00+n) ; 12c */
    case 0xF0:
    {
        A = Cpu::ReadByte(0xFF00 + Cpu::NextByte());
        break;
    }
    /* ld (FF00+n),A ; 12c */
    case 0xE0:
    {
        Cpu::WriteByte(0xFF00 + Cpu::NextByte(), A);
        break;
    }
    /* ld A,(FF00+C) ; 8c */
    case 0xF2:
    {
        A = Cpu::ReadByte(0xFF00 + reg8[0]);
        break;
    }
    /* ld (FF00+C),A ; 8c */
    case 0xE2:
    {
        Cpu::WriteByte(0xFF00 + reg8[0], A);
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
        *reg16_group1[(opcode - 0x01) >> 4] = Cpu::NextWord();
        break;
    }
    /* ld (nn),SP ; 20c */
    case 0x08:
    {
        Cpu::WriteWord(Cpu::NextWord(), SP);
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
        Cpu::Push(*reg16_group3[(opcode - 0xC5) >> 4]);
        m_Emulator->Tick(); // internal
        break;
    }
    /* pop r16-g3 ; 12c */
    case 0xC1: case 0xD1: case 0xE1: case 0xF1: // pop r16-g3
    {
        *reg16_group3[(opcode - 0xC1) >> 4] = Cpu::Pop();
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
        int cy = opcode > 0x87 ? Cpu::IsFlagSet(FLAG_C) : 0;
        Cpu::ModifyFlag(FLAG_H, HC8(A, r8, cy));
        Cpu::ModifyFlag(FLAG_C, CARRY8(A, r8, cy));
        A += r8 + cy;
        Cpu::ModifyFlag(FLAG_Z, A == 0);
        Cpu::ModifyFlag(FLAG_N, 0);
        break;
    }
    /* (add / adc) A,n ; 8c */
    case 0xC6: case 0xCE:
    {
        uint8_t n = Cpu::NextByte();
        int cy = opcode == 0xCE ? Cpu::IsFlagSet(FLAG_C) : 0;
        Cpu::ModifyFlag(FLAG_H, HC8(A, n, cy));
        Cpu::ModifyFlag(FLAG_C, CARRY8(A, n, cy));
        A += n + cy;
        Cpu::ModifyFlag(FLAG_Z, A == 0);
        Cpu::ModifyFlag(FLAG_N, 0);
        break;
    }
    /* (add / adc) A,(HL) ; 8c */
    case 0x86: case 0x8E:
    {
        uint8_t hl = Cpu::ReadByte(HL);
        int cy = opcode == 0x8E ? Cpu::IsFlagSet(FLAG_C) : 0;
        Cpu::ModifyFlag(FLAG_H, HC8(A, hl, cy));
        Cpu::ModifyFlag(FLAG_C, CARRY8(A, hl, cy));
        A += hl + cy;
        Cpu::ModifyFlag(FLAG_Z, A == 0);
        Cpu::ModifyFlag(FLAG_N, 0);
        break;
    }
    /* (sub / sbc) A,r8 ; 4c */
    case 0x90: case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x97: // sub r8
    case 0x98: case 0x99: case 0x9A: case 0x9B: case 0x9C: case 0x9D: case 0x9F: // sbc A,r8
    {
        uint8_t r8 = *reg8_group[opcode - (opcode <= 0x97 ? 0x90 : 0x98)];
        int cy = opcode > 0x97 ? Cpu::IsFlagSet(FLAG_C) : 0;
        Cpu::ModifyFlag(FLAG_H, HC8_SUB(A, r8, cy));
        Cpu::ModifyFlag(FLAG_C, CARRY8_SUB(A, r8, cy));
        A -= r8 + cy;
        Cpu::ModifyFlag(FLAG_Z, A == 0);
        Cpu::ModifyFlag(FLAG_N, 1);
        break;
    }
    /* (sub / sbc) A,n ; 8c */
    case 0xD6: case 0xDE:
    {
        uint8_t n = Cpu::NextByte();
        int cy = opcode == 0xDE ? Cpu::IsFlagSet(FLAG_C) : 0;
        Cpu::ModifyFlag(FLAG_H, HC8_SUB(A, n, cy));
        Cpu::ModifyFlag(FLAG_C, CARRY8_SUB(A, n, cy));
        A -= n + cy;
        Cpu::ModifyFlag(FLAG_Z, A == 0);
        Cpu::ModifyFlag(FLAG_N, 1);
        break;
    }
    /* (sub / sbc) A,(HL) ; 8c */
    case 0x96: case 0x9E:
    {
        uint8_t hl = Cpu::ReadByte(HL);
        int cy = opcode == 0x9E ? Cpu::IsFlagSet(FLAG_C) : 0;
        Cpu::ModifyFlag(FLAG_H, HC8_SUB(A, hl, cy));
        Cpu::ModifyFlag(FLAG_C, CARRY8_SUB(A, hl, cy));
        A -= hl + cy;
        Cpu::ModifyFlag(FLAG_Z, A == 0);
        Cpu::ModifyFlag(FLAG_N, 1);
        break;
    }
    /* and r8 ; 4c */
    case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA4: case 0xA5: case 0xA7: // and r8
    {
        A &= *reg8_group[opcode - 0xA0];
        Cpu::ModifyFlag(FLAG_Z, A == 0);
        Cpu::ModifyFlag(FLAG_N, 0);
        Cpu::ModifyFlag(FLAG_H, 1);
        Cpu::ModifyFlag(FLAG_C, 0);
        break;
    }
    /* and n ; 8c */
    case 0xE6:
    {
        A &= Cpu::NextByte();
        Cpu::ModifyFlag(FLAG_Z, A == 0);
        Cpu::ModifyFlag(FLAG_N, 0);
        Cpu::ModifyFlag(FLAG_H, 1);
        Cpu::ModifyFlag(FLAG_C, 0);
        break;
    }
    /* and (HL) ; 8c */
    case 0xA6:
    {
        A &= Cpu::ReadByte(HL);
        Cpu::ModifyFlag(FLAG_Z, A == 0);
        Cpu::ModifyFlag(FLAG_N, 0);
        Cpu::ModifyFlag(FLAG_H, 1);
        Cpu::ModifyFlag(FLAG_C, 0);
        break;
    }
    /* xor r8 ; 4c */
    case 0xA8: case 0xA9: case 0xAA: case 0xAB: case 0xAC: case 0xAD: case 0xAF: // xor r8
    {
        A ^= *reg8_group[opcode - 0xA8];
        Cpu::ModifyFlag(FLAG_Z, A == 0);
        Cpu::ModifyFlag(FLAG_N, 0);
        Cpu::ModifyFlag(FLAG_H, 0);
        Cpu::ModifyFlag(FLAG_C, 0);
        break;
    }
    /* xor n ; 8c */
    case 0xEE:
    {
        A ^= Cpu::NextByte();
        Cpu::ModifyFlag(FLAG_Z, A == 0);
        Cpu::ModifyFlag(FLAG_N, 0);
        Cpu::ModifyFlag(FLAG_H, 0);
        Cpu::ModifyFlag(FLAG_C, 0);
        break;
    }
    /* xor (HL) ; 8c */
    case 0xAE:
    {
        A ^= Cpu::ReadByte(HL);
        Cpu::ModifyFlag(FLAG_Z, A == 0);
        Cpu::ModifyFlag(FLAG_N, 0);
        Cpu::ModifyFlag(FLAG_H, 0);
        Cpu::ModifyFlag(FLAG_C, 0);
        break;
    }
    /* or r8 ; 4c */
    case 0xB0: case 0xB1: case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB7: // or r8
    {
        A |= *reg8_group[opcode - 0xB0];
        Cpu::ModifyFlag(FLAG_Z, A == 0);
        Cpu::ModifyFlag(FLAG_N, 0);
        Cpu::ModifyFlag(FLAG_H, 0);
        Cpu::ModifyFlag(FLAG_C, 0);
        break;
    }
    /* or n ; 8c */
    case 0xF6:
    {
        A |= Cpu::NextByte();
        Cpu::ModifyFlag(FLAG_Z, A == 0);
        Cpu::ModifyFlag(FLAG_N, 0);
        Cpu::ModifyFlag(FLAG_H, 0);
        Cpu::ModifyFlag(FLAG_C, 0);
        break;
    }
    /* or (HL) ; 8c */
    case 0xB6:
    {
        A |= Cpu::ReadByte(HL);
        Cpu::ModifyFlag(FLAG_Z, A == 0);
        Cpu::ModifyFlag(FLAG_N, 0);
        Cpu::ModifyFlag(FLAG_H, 0);
        Cpu::ModifyFlag(FLAG_C, 0);
        break;
    }
    /* cp r8 ; 4c */
    case 0xB8: case 0xB9: case 0xBA: case 0xBB: case 0xBC: case 0xBD: case 0xBF: // cp r8
    {
        uint8_t r8 = *reg8_group[opcode - 0xB8];
        Cpu::ModifyFlag(FLAG_H, HC8_SUB(A, r8, 0));
        Cpu::ModifyFlag(FLAG_C, CARRY8_SUB(A, r8, 0));
        Cpu::ModifyFlag(FLAG_Z, A == r8);
        Cpu::ModifyFlag(FLAG_N, 1);
        break;
    }
    /* cp n ; 8c */
    case 0xFE:
    {
        uint8_t n = Cpu::NextByte();
        Cpu::ModifyFlag(FLAG_H, HC8_SUB(A, n, 0));
        Cpu::ModifyFlag(FLAG_C, CARRY8_SUB(A, n, 0));
        Cpu::ModifyFlag(FLAG_Z, A == n);
        Cpu::ModifyFlag(FLAG_N, 1);
        break;
    }
    /* cp (HL) ; 8c */
    case 0xBE:
    {
        uint8_t hl = Cpu::ReadByte(HL);
        Cpu::ModifyFlag(FLAG_H, HC8_SUB(A, hl, 0));
        Cpu::ModifyFlag(FLAG_C, CARRY8_SUB(A, hl, 0));
        Cpu::ModifyFlag(FLAG_Z, A == hl);
        Cpu::ModifyFlag(FLAG_N, 1);
        break;
    }
    /* inc r8 ; 4c */
    case 0x04: case 0x0C: case 0x14: case 0x1C: case 0x24: case 0x2C: case 0x3C: // inc r8
    {
        int idx = (opcode - 0x04) >> 3;
        uint8_t val = *reg8_group[idx];
        Cpu::ModifyFlag(FLAG_H, HC8(val, 1, 0));
        val += 1;
        Cpu::ModifyFlag(FLAG_Z, val == 0);
        *reg8_group[idx] = val;
        Cpu::ModifyFlag(FLAG_N, 0);
        break;
    }
    /* inc (HL) ; 12c */
    case 0x34:
    {
        uint8_t hl = Cpu::ReadByte(HL);
        Cpu::ModifyFlag(FLAG_H, HC8(hl, 1, 0));
        hl += 1;
        Cpu::ModifyFlag(FLAG_Z, hl == 0);
        Cpu::WriteByte(HL, hl);
        Cpu::ModifyFlag(FLAG_N, 0);
        break;
    }
    /* dec r8 ; 4c */
    case 0x05: case 0x0D: case 0x15: case 0x1D: case 0x25: case 0x2D: case 0x3D: // dec r8
    {
        int idx = (opcode - 0x05) >> 3;
        uint8_t val = *reg8_group[idx];
        Cpu::ModifyFlag(FLAG_H, HC8_SUB(val, 1, 0));
        val -= 1;
        Cpu::ModifyFlag(FLAG_Z, val == 0);
        *reg8_group[idx] = val;
        Cpu::ModifyFlag(FLAG_N, 1);
        break;
    }
    /* dec (HL) ; 12c */
    case 0x35:
    {
        uint8_t hl = Cpu::ReadByte(HL);
        Cpu::ModifyFlag(FLAG_H, HC8_SUB(hl, 1, 0));
        hl -= 1;
        Cpu::ModifyFlag(FLAG_Z, hl == 0);
        Cpu::WriteByte(HL, hl);
        Cpu::ModifyFlag(FLAG_N, 1);
        break;
    }
    /* daa ; 4c */
    case 0x27: // You should try implementing yourself without copying other people's code...
    {
        if (!Cpu::IsFlagSet(FLAG_N))
        {
            // after an addition, adjust if (half-)carry occurred or if result is out of bounds
            if (Cpu::IsFlagSet(FLAG_C) || A > 0x99)
            {
                A += 0x60;
                Cpu::ModifyFlag(FLAG_C, 1);
            }

            if (Cpu::IsFlagSet(FLAG_H) || (A & 0xF) > 0x9)
            {
                A += 0x6;
            }
        }
        else
        {
            // after a subtraction, only adjust if (half-)carry occurred
            if (Cpu::IsFlagSet(FLAG_C))
            {
                A -= 0x60;
                Cpu::ModifyFlag(FLAG_C, 1);
            }

            if (Cpu::IsFlagSet(FLAG_H))
            {
                A -= 0x6;
            }
        }

        Cpu::ModifyFlag(FLAG_Z, A == 0);
        Cpu::ModifyFlag(FLAG_H, 0);

        break;
    }
    /* cpl ; 4c */
    case 0x2F:
    {
        A ^= 0xFF;
        Cpu::ModifyFlag(FLAG_N, 1);
        Cpu::ModifyFlag(FLAG_H, 1);
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
        Cpu::ModifyFlag(FLAG_H, HC16(HL, val));
        Cpu::ModifyFlag(FLAG_C, CARRY16(HL, val));
        HL += val;
        Cpu::ModifyFlag(FLAG_N, 0);
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
        int8_t dd = static_cast<int8_t>(Cpu::NextByte());
        Cpu::ModifyFlag(FLAG_H, HC8(SP, dd, 0));
        Cpu::ModifyFlag(FLAG_C, CARRY8(SP, dd, 0));
        SP += dd;
        Cpu::ModifyFlag(FLAG_Z, 0);
        Cpu::ModifyFlag(FLAG_N, 0);
        m_Emulator->Tick(); // internal
        m_Emulator->Tick(); // write
        break;
    }
    /* ld HL,SP+dd ; 12c */
    case 0xF8:
    {
        int8_t dd = static_cast<int8_t>(Cpu::NextByte());
        Cpu::ModifyFlag(FLAG_H, HC8(SP, dd, 0));
        Cpu::ModifyFlag(FLAG_C, CARRY8(SP, dd, 0));
        HL = SP + dd;
        Cpu::ModifyFlag(FLAG_Z, 0);
        Cpu::ModifyFlag(FLAG_N, 0);
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
        Cpu::RotateBitsLeft(A, opcode == 0x07, true);
        break;
    }
    /* rr(c)a ; 4c */
    case 0x0F: case 0x1F:
    {
        Cpu::RotateBitsRight(A, opcode == 0x0F, true);
        break;
    }
    case 0xCB:
    {
        Cpu::HandlePrefixCB();
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
        Cpu::ModifyFlag(FLAG_C, !Cpu::IsFlagSet(FLAG_C));
        Cpu::ModifyFlag(FLAG_N, 0);
        Cpu::ModifyFlag(FLAG_H, 0);
        break;
    }
    /* scf ; 4c */
    case 0x37:
    {
        Cpu::ModifyFlag(FLAG_C, 1);
        Cpu::ModifyFlag(FLAG_N, 0);
        Cpu::ModifyFlag(FLAG_H, 0);
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
        PC = Cpu::NextWord();
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
        uint16_t nn = Cpu::NextWord();
        if (Cpu::Condition((opcode - 0xC2) >> 3))
        {
            PC = nn;
        }
        break;
    }
    /* jr PC+dd ; 12c */
    case 0x18:
    {
        int8_t dd = static_cast<int8_t>(Cpu::NextByte());
        PC += dd;
        m_Emulator->Tick(); // internal
        break;
    }
    /* jr f,PC+dd ; 12c/8c */
    case 0x20: case 0x28: case 0x30: case 0x38: // jr f,PC+dd
    {
        int8_t dd = static_cast<int8_t>(Cpu::NextByte());
        if (Cpu::Condition((opcode - 0x20) >> 3))
        {
            PC += dd;
        }
        break;
    }
    /* call nn ; 24c */
    case 0xCD:
    {
        Cpu::Call(Cpu::NextWord());
        m_Emulator->Tick(); // internal
        break;
    }
    /* call f,nn ; 24c/12c */
    case 0xC4: case 0xCC: case 0xD4: case 0xDC: // call f,nn
    {
        uint16_t nn = Cpu::NextWord();
        if (Cpu::Condition((opcode - 0xC4) >> 3))
        {
            Cpu::Call(nn);
        }
        break;
    }
    /* ret ; 16c */
    case 0xC9:
    {
        PC = Cpu::Pop();
        m_Emulator->Tick(); // internal
        break;
    }
    /* ret f ; 20c/8c */
    case 0xC0: case 0xC8: case 0xD0: case 0xD8: // ret f
    {
        if (Cpu::Condition((opcode - 0xC0) >> 3))
        {
            PC = Cpu::Pop();
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
        PC = Cpu::Pop(); /* RET */
        m_Emulator->Tick(); // internal
        m_Emulator->m_IntManager->EnableIME(); /* EI */
        break;
    }
    /* rst n ; 16c */
    case 0xC7: case 0xCF: case 0xD7: case 0xDF: case 0xE7: case 0xEF: case 0xF7: case 0xFF:
    {
        Cpu::Call(RST_ADDR[(opcode - 0xC7) >> 3]);
        m_Emulator->Tick(); // internal
        break;
    }
    default:
    {
        std::cerr << "Unimplemented opcode: " << int_to_hex(opcode) << std::endl;
        Cpu::Debug_PrintStatus();
        throw std::runtime_error("^^^");
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

void Cpu::HandleInterrupt(int i)
{
    // https://gbdev.io/pandocs/Interrupts.html#interrupt-handling
    // 5 cycles (+1 if halt)
    RES_BIT(m_Emulator->m_MemControl->IF, i);
    m_Emulator->m_IntManager->DisableIME();
    m_Emulator->Tick();
    m_Emulator->Tick();
    Cpu::Call(int_vectors[i]);
    m_Emulator->Tick();
    if (bHalted) m_Emulator->Tick(); // refer to 4.9 in https://github.com/AntonioND/giibiiadvance/blob/master/docs/TCAGBD.pdf
}

bool Cpu::isHalted()
{
    return bHalted;
}

void Cpu::Resume()
{
    bHalted = false;
}

void Cpu::Debug_PrintStatus()
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
    std::cout << "Z(7)=" << Cpu::IsFlagSet(FLAG_Z) << " "
              << "N(6)=" << Cpu::IsFlagSet(FLAG_N) << " "
              << "H(5)=" << Cpu::IsFlagSet(FLAG_H) << " "
              << "C(4)=" << Cpu::IsFlagSet(FLAG_C) << std::endl;
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

void Cpu::Debug_PrintCurrentInstruction()
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

void Cpu::Debug_EditRegister(int reg, int val, bool is8Bit)
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

void Cpu::Debug_EditFlag(int flag, int val)
{
    Cpu::ModifyFlag(flag, val);
    std::cout << "flag " << flag_str[flag - 4] << " is now set to " << val << std::endl;
}

void Cpu::Debug_LogState()
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
{
    m_Emulator = emu;
}

InterruptManager::~InterruptManager()
{
    delete m_Emulator;
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

void InterruptManager::Reset()
{
    m_IME = false;
    haltMode = -1;
    m_Emulator->m_MemControl->IF = 0xE0;
    m_Emulator->m_MemControl->IE = 0;
}

bool InterruptManager::SetHaltMode()
{
    if (m_IME)
    {
        haltMode = HALT_MODE0;
        return true;
    }
    else if (!m_IME && ((m_Emulator->m_MemControl->IE & m_Emulator->m_MemControl->IF) == 0))
    {
        haltMode = HALT_MODE1;
        return true;
    }
    else if (!m_IME && ((m_Emulator->m_MemControl->IE & m_Emulator->m_MemControl->IF) != 0))
    {
        haltMode = HALT_MODE2;
        return false;
    }

    std::cerr << "Yo nigga, InterruptManager::SetHaltMode has gone mad!" << std::endl;
    return false;
}

void InterruptManager::ResetHaltMode()
{
    haltMode = -1;
}

void InterruptManager::RequestInterrupt(int i)
{
    SET_BIT(m_Emulator->m_MemControl->IF, i);
}

void InterruptManager::InterruptRoutine()
{
    // once you enter halt mode, the only thing you can do to get out of it is to receive an interrupt
    if (haltMode != HALT_MODE2 && ((m_Emulator->m_MemControl->IE & m_Emulator->m_MemControl->IF) != 0))
    {
        if (m_Emulator->m_Cpu->isHalted())
        {
            m_Emulator->m_Cpu->Resume();
            if (!m_IME) m_Emulator->Tick(); // 1 cycle increase (tick) for exiting halt mode when IME=0 and is halted ??
            InterruptManager::ResetHaltMode();
        }

        for (int i = 0; i < 5; ++i)
        {
            if (m_IME && TEST_BIT(m_Emulator->m_MemControl->IF, i) && TEST_BIT(m_Emulator->m_MemControl->IE, i))
            {
                m_Emulator->m_Cpu->HandleInterrupt(i);
            }
        }
    }
}

void InterruptManager::Debug_PrintStatus()
{
    std::cout << "*INTERRUPT MANAGER STATUS*" << std::endl;
    std::cout << "IME=" << m_Emulator->m_IntManager->GetIME() << " "
              << "IF=" << int_to_bin8(m_Emulator->m_MemControl->IF) << " "
              << "IE=" << int_to_bin8(m_Emulator->m_MemControl->IE) << std::endl;
    std::cout << std::endl;
}

Timer::Timer(Emulator *emu)
{
    m_Emulator = emu;
}

Timer::~Timer()
{
    delete m_Emulator;
}

void Timer::Reset()
{
    DIV = 0xABCC;
    m_Emulator->m_MemControl->TIMA = 0x00;
    m_Emulator->m_MemControl->TMA = 0x00;
    m_Emulator->m_MemControl->TAC = 0xF8;
    Timer::UpdateCounter();
}

void Timer::Update(int cycles)
{
    DIV += cycles;

    if (Timer::TimerEnable())
    {
        m_Counter -= cycles;

        if (m_Counter <= 0)
        {
            Timer::UpdateCounter();

            if (m_Emulator->m_MemControl->TIMA < 0xFF)
            {
                m_Emulator->m_MemControl->TIMA++;
            }
            else
            {
                m_Emulator->m_MemControl->TIMA = m_Emulator->m_MemControl->TMA;
                m_Emulator->m_IntManager->RequestInterrupt(INT_TIMER);
            }
        }
    }
}

void Timer::UpdateCounter()
{
    switch (Timer::ClockSelect())
    {
    case 0: m_Counter = 1024; break;
    case 1: m_Counter = 16;   break;
    case 2: m_Counter = 64;   break;
    case 3: m_Counter = 256;  break;
    }
}

int Timer::TimerEnable()
{
    return TEST_BIT(m_Emulator->m_MemControl->TAC, 2);
}

int Timer::ClockSelect()
{
    return m_Emulator->m_MemControl->TAC & 0x3;
}

void Timer::Debug_PrintStatus()
{
    std::cout << "*TIMER STATUS*" << std::endl;
    std::cout << "DIV=" << int_to_hex(DIV) << " "
              << "TIMA=" << int_to_hex(+m_Emulator->m_MemControl->TIMA, 2) << " "
              << "TMA=" << int_to_hex(+m_Emulator->m_MemControl->TMA, 2) << " "
              << "TAC=" << int_to_bin8(m_Emulator->m_MemControl->TAC) << std::endl;
    std::cout << std::endl;
}

MemoryController::MemoryController(Emulator *emu)
{
    m_Emulator = emu;
    m_Cartridge.fill(0);
}

MemoryController::~MemoryController()
{
    delete m_Emulator;
}

void MemoryController::Reset()
{
    m_VRAM.fill(0);
    m_WRAM.fill(0);
    m_OAM.fill(0);
    m_IO.fill(0);
    m_HRAM.fill(0);

    // https://gbdev.io/pandocs/Power_Up_Sequence.html
    // TODO move to PPU class
    m_IO[0x40] = 0x91; // LCDC
    m_IO[0x41] = 0x85; // STAT
    m_IO[0x42] = 0x00; // SCY
    m_IO[0x43] = 0x00; // SCX
    m_IO[0x44] = 0x90; // LY - note: usually 0x00 but use 0x90 if logging is enabled
    m_IO[0x45] = 0x00; // LYC
    m_IO[0x46] = 0xFF; // DMA
    m_IO[0x47] = 0xFC; // BGP
    m_IO[0x48] = 0xFF; // OBP0
    m_IO[0x49] = 0xFF; // OBP1
    m_IO[0x4A] = 0x00; // WY
    m_IO[0x4B] = 0x00; // WX
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
}

void MemoryController::WriteByte(uint16_t address, uint8_t data)
{
    if (address >= 0x8000 && address < 0xA000)
    {
        m_VRAM[address - 0x8000] = data;
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
    else if (address == 0xFF04)
    {
        m_Emulator->m_Timer->DIV = 0;
    }
    else if (address == 0xFF07)
    {
        int oldfreq = m_Emulator->m_Timer->ClockSelect();
        TAC = data;
        int newfreq = m_Emulator->m_Timer->ClockSelect();

        if (oldfreq != newfreq)
        {
            m_Emulator->m_Timer->UpdateCounter();
        }
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
        std::cerr << "MemoryController::WriteByte: Invalid address range: " << int_to_hex(address) << std::endl;
        m_Emulator->m_Cpu->Debug_PrintStatus();
        throw std::runtime_error("^^^");
    }
}

void MemoryController::WriteWord(uint16_t address, uint16_t data)
{
    uint8_t high = data >> 8;
    uint8_t low = data & 0xFF;
    MemoryController::WriteByte(address, low);
    MemoryController::WriteByte(address + 1, high);
}

uint8_t MemoryController::ReadByte(uint16_t address) const
{
    if (address < 0x8000)
    {
        return m_Cartridge[address];
    }
    if (address >= 0x8000 && address < 0xA000)
    {
        return m_VRAM[address - 0x8000];
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
        std::cerr << "MemoryController::ReadByte: Invalid address range: " << int_to_hex(address) << std::endl;
        m_Emulator->m_Cpu->Debug_PrintStatus();
        throw std::runtime_error("^^^");
        return 0x0;
    }
}

uint16_t MemoryController::ReadWord(uint16_t address) const
{
    uint8_t low = MemoryController::ReadByte(address);
    uint8_t high = MemoryController::ReadByte(address + 1);
    return (high << 8) | low;
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

Emulator::Emulator()
{
    m_Cpu = std::unique_ptr<Cpu>(new Cpu(this));
    m_MemControl = std::unique_ptr<MemoryController>(new MemoryController(this));
    m_IntManager = std::unique_ptr<InterruptManager>(new InterruptManager(this));
    m_Timer = std::unique_ptr<Timer>(new Timer(this));
}

Emulator::~Emulator()
{}

void Emulator::ResetComponents()
{
    m_MemControl->Reset();
    m_Cpu->Reset();
    m_IntManager->Reset();
    m_Timer->Reset();
    m_TotalCycles = m_PrevTotalCycles = 0;
}

void Emulator::Update()
{
    while ((m_TotalCycles - m_PrevTotalCycles) <= MAX_CYCLES)
    {
        int initial_cycles = m_TotalCycles;
        m_Cpu->Step();
        m_Timer->Update(m_TotalCycles - initial_cycles);

        // blarggs test - serial output
        if (m_MemControl->ReadByte(0xFF02) == 0x81) {
            std::cout << m_MemControl->ReadByte(0xFF01);
            m_MemControl->WriteByte(0xFF02, 0x0);
        }
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
        m_Cpu->Step();
        m_Timer->Update(m_TotalCycles - initial_cycles);

        if (m_MemControl->ReadByte(0xFF02) == 0x81) {
            blargg_serial.push_back(m_MemControl->ReadByte(0xFF01));
            m_MemControl->WriteByte(0xFF02, 0x0);
        }
    }
}

void Emulator::Debug_StepTill(std::vector<char>& blargg_serial, uint16_t x)
{
    while (m_Cpu->GetPC() != x)
    {
        Emulator::Debug_Step(blargg_serial, 1);
    }
}

void Emulator::Debug_PrintEmulatorStatus()
{
    m_Cpu->Debug_PrintStatus();
    m_IntManager->Debug_PrintStatus();
    m_Timer->Debug_PrintStatus();
}

int main(int argc, char *argv[])
{
    Emulator *emu = new Emulator();

    emu->m_MemControl->LoadROM("../IronBoy/ROMS/blargg_tests/cpu_instrs/individual/02-interrupts.gb" /*argv[1]*/);
    emu->ResetComponents();

#ifdef NDEBUG
    uint64_t initial_ms = unix_epoch_millis();
    uint64_t millis = 0;

    while (true)
    {
        uint64_t current_ms = unix_epoch_millis() - initial_ms;

        if (current_ms > (millis + UPDATE_INTERVAL))
        {
            emu->Update();
            millis = current_ms;
        }
    }
#else
    std::vector<char> blargg_serial;
    std::string input_str;

    std::cout << "Noufu - debug mode enabled" << std::endl;

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
                    emu->m_Cpu->Debug_PrintStatus();
                }
                else if (str == "intman")
                {
                    emu->m_IntManager->Debug_PrintStatus();
                }
                else if (str == "timer")
                {
                    emu->m_Timer->Debug_PrintStatus();
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
                emu->m_Cpu->Debug_PrintCurrentInstruction();
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
                emu->m_Cpu->Debug_EditRegister(r8, v, true);
            }
            else if (str == "flag")
            {
                iss >> str;
                int f = int_from_string(str, false);
                iss >> str;
                int v = int_from_string(str, false);
                emu->m_Cpu->Debug_EditFlag(f, v);
            }
            else if (str == "reg16")
            {
                iss >> str;
                int r16 = int_from_string(str, false);
                iss >> str;
                int v = int_from_string(str, true);
                emu->m_Cpu->Debug_EditRegister(r16, v, false);
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
            emu->ResetComponents();
            blargg_serial.clear();
            fout.close();
            fout.open("gblog.txt", std::ofstream::out | std::ofstream::trunc);
        }
    }

    fout.close();
#endif

    return 0;
}