#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <span>

enum { FETCH, EXEC };

class CPU final {
 public:
  CPU() {
    state = FETCH;
    std::fill(mem.begin(), mem.end(), 0);
    clear_ops();

    cycles = 0;
    PC = 0;
    HL = 0x5;
    mem[0] = 0x1;
    mem[HL] = 5;
  }

  void step() {
    switch (state) {
      case FETCH: {
        uint8_t opcode = ReadMemory(PC);
        PC++;
        switch (opcode) {
          case 0x1:  // inc [HL] - takes total of 12 cycles
          {
            // doesn't have to be here as long it's a static/global variable and
            // doesn't get destroyed
            static constexpr std::array<void(*)(CPU*), 2> inc_HL_addr_ops{
                [](CPU *cpu) { cpu->tmp1 = cpu->ReadMemory(cpu->HL); },
                [](CPU *cpu) {
                  cpu->tmp1 += 1;
                  cpu->WriteMemory(cpu->HL, cpu->tmp1);
                },
            };
            ops = inc_HL_addr_ops;
            break;
          }
        }
        state = EXEC;
        break;
      }
      case EXEC: {
        auto f = ops[current_op++];
        f(this);

        if (current_op == ops.size()) {
          state = FETCH;
        }
        break;
      }
    }
  }

  uint8_t ReadMemory(uint16_t addr) {
    cycles += 4;
    return mem[addr];
  }

  void WriteMemory(uint16_t addr, uint8_t data) {
    cycles += 4;
    mem[addr] = data;
  }

  void clear_ops() {
    ops = {};
    current_op = 0;
  }

  int state;

  int cycles = 0;

  std::array<uint8_t, 0x100> mem;

  std::span<void (*const)(CPU *)> ops;
  unsigned current_op;

  uint16_t HL;
  uint16_t PC;

  uint8_t tmp1;
};

auto extern_dummy = &CPU::step;

#ifdef EXECUTE
#include <iostream>

int main() {
  CPU cpu;
  cpu.step();
  std::cout << "PC=" << cpu.PC << " cycles=" << cpu.cycles
            << std::endl;  // expected: PC=1 cycles=4
  cpu.step();
  std::cout << "PC=" << cpu.PC << " cycles=" << cpu.cycles
            << std::endl;  // expected: PC=1 cycles=8
  cpu.step();
  std::cout << "PC=" << cpu.PC << " cycles=" << cpu.cycles
            << std::endl;  // expected: PC=1 cycles=12
  std::cout << "[HL]=" << (int)cpu.ReadMemory(cpu.HL)
            << std::endl;  // expected: [HL]=6
  return 0;
}
#endif