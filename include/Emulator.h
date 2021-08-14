#ifndef _EMULATOR_H_
#define _EMULATOR_H_

#include <memory>
#include <vector>

#include "EmulatorConfig.h"
#include "CPU.h"
#include "MemoryController.h"
#include "InterruptManager.h"
#include "Timer.h"
#include "SimpleGPU.h"
#include "JoyPad.h"

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

    std::unique_ptr<EmulatorConfig> m_Config;

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

#endif