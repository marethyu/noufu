#include "loguru.hpp"

const std::string ROM_FILE_PATH = "../ROMS/blargg_tests/instr_timing/instr_timing.gb";

std::ofstream fout;

class GPU; /* GPU with pixel FIFO */ // TODO
class Cartridge; // TODO
class MBCBase; /* abstract */ // TODO
class MBC1; // TODO

#ifdef GUI_MODE



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