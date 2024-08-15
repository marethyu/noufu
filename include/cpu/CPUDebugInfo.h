#ifndef _CPU_DEBUG_INFO_
#define _CPU_DEBUG_INFO_

#include <string>

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
    {"LD BC,{0:04X}", ARG_U16},
    {"LD (BC),A", ARG_NONE},
    {"INC BC", ARG_NONE},
    {"INC B", ARG_NONE},
    {"DEC B", ARG_NONE},
    {"LD B,{0:02X}", ARG_U8},
    {"RLCA", ARG_NONE},
    {"LD ({0:04X}),SP", ARG_U16},
    {"ADD HL,BC", ARG_NONE},
    {"LD A,(BC)", ARG_NONE},
    {"DEC BC", ARG_NONE},
    {"INC C", ARG_NONE},
    {"DEC C", ARG_NONE},
    {"LD C,{0:02X}", ARG_U8},
    {"RRCA", ARG_NONE},
    {"STOP", ARG_NONE},
    {"LD DE,{0:04X}", ARG_U16},
    {"LD (DE),A", ARG_NONE},
    {"INC DE", ARG_NONE},
    {"INC D", ARG_NONE},
    {"DEC D", ARG_NONE},
    {"LD D,{0:02X}", ARG_U8},
    {"RLA", ARG_NONE},
    {"JR {0:02X}", ARG_I8},
    {"ADD HL,DE", ARG_NONE},
    {"LD A,(DE)", ARG_NONE},
    {"DEC DE", ARG_NONE},
    {"INC E", ARG_NONE},
    {"DEC E", ARG_NONE},
    {"LD E,{0:02X}", ARG_U8},
    {"RRA", ARG_NONE},
    {"JR NZ,{0:02X}", ARG_I8},
    {"LD HL,{0:04X}", ARG_U16},
    {"LD (HL+),A", ARG_NONE},
    {"INC HL", ARG_NONE},
    {"INC H", ARG_NONE},
    {"DEC H", ARG_NONE},
    {"LD H,{0:02X}", ARG_U8},
    {"DAA", ARG_NONE},
    {"JR Z,{0:02X}", ARG_I8},
    {"ADD HL,HL", ARG_NONE},
    {"LD A,(HL+)", ARG_NONE},
    {"DEC HL", ARG_NONE},
    {"INC L", ARG_NONE},
    {"DEC L", ARG_NONE},
    {"LD L,{0:02X}", ARG_U8},
    {"CPL", ARG_NONE},
    {"JR NC,{0:02X}", ARG_I8},
    {"LD SP,{0:04X}", ARG_U16},
    {"LD (HL-),A", ARG_NONE},
    {"INC SP", ARG_NONE},
    {"INC (HL)", ARG_NONE},
    {"DEC (HL)", ARG_NONE},
    {"LD (HL),{0:02X}", ARG_U8},
    {"SCF", ARG_NONE},
    {"JR C,{0:02X}", ARG_I8},
    {"ADD HL,SP", ARG_NONE},
    {"LD A,(HL-)", ARG_NONE},
    {"DEC SP", ARG_NONE},
    {"INC A", ARG_NONE},
    {"DEC A", ARG_NONE},
    {"LD A,{0:02X}", ARG_U8},
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
    {"JP NZ,{0:04X}", ARG_U16},
    {"JP {0:04X}", ARG_U16},
    {"CALL NZ,{0:04X}", ARG_U16},
    {"PUSH BC", ARG_NONE},
    {"ADD A,{0:02X}", ARG_U8},
    {"RST 00h", ARG_NONE},
    {"RET Z", ARG_NONE},
    {"RET", ARG_NONE},
    {"JP Z,{0:04X}", ARG_U16},
    {"PREFIX CB", ARG_NONE},
    {"CALL Z,{0:04X}", ARG_U16},
    {"CALL {0:04X}", ARG_U16},
    {"ADC A,{0:02X}", ARG_U8},
    {"RST 08h", ARG_NONE},
    {"RET NC", ARG_NONE},
    {"POP DE", ARG_NONE},
    {"JP NC,{0:04X}", ARG_U16},
    {"UNUSED", ARG_NONE},
    {"CALL NC,{0:04X}", ARG_U16},
    {"PUSH DE", ARG_NONE},
    {"SUB A,{0:02X}", ARG_U8},
    {"RST 10h", ARG_NONE},
    {"RET C", ARG_NONE},
    {"RETI", ARG_NONE},
    {"JP C,{0:04X}", ARG_U16},
    {"UNUSED", ARG_NONE},
    {"CALL C,{0:04X}", ARG_U16},
    {"UNUSED", ARG_NONE},
    {"SBC A,{0:02X}", ARG_U8},
    {"RST 18h", ARG_NONE},
    {"LD (FF00+{0:02X}),A", ARG_U8},
    {"POP HL", ARG_NONE},
    {"LD (FF00+C),A", ARG_NONE},
    {"UNUSED", ARG_NONE},
    {"UNUSED", ARG_NONE},
    {"PUSH HL", ARG_NONE},
    {"AND A,{0:02X}", ARG_U8},
    {"RST 20h", ARG_NONE},
    {"ADD SP,{0:02X}", ARG_I8},
    {"JP HL", ARG_NONE},
    {"LD ({0:04X}),A", ARG_U16},
    {"UNUSED", ARG_NONE},
    {"UNUSED", ARG_NONE},
    {"UNUSED", ARG_NONE},
    {"XOR A,{0:02X}", ARG_U8},
    {"RST 28h", ARG_NONE},
    {"LD A,(FF00+{0:02X})", ARG_U8},
    {"POP AF", ARG_NONE},
    {"LD A,(FF00+C)", ARG_NONE},
    {"DI", ARG_NONE},
    {"UNUSED", ARG_NONE},
    {"PUSH AF", ARG_NONE},
    {"OR A,{0:02X}", ARG_U8},
    {"RST 30h", ARG_NONE},
    {"LD HL,SP+{0:02X}", ARG_I8},
    {"LD SP,HL", ARG_NONE},
    {"LD A,({0:04X})", ARG_U16},
    {"EI", ARG_NONE},
    {"UNUSED", ARG_NONE},
    {"UNUSED", ARG_NONE},
    {"CP A,{0:02X}", ARG_U8},
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

#endif