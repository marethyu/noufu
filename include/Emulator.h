#ifndef _EMULATOR_H_
#define _EMULATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "EmulatorConfig.h"
#include "CPU.h"
#include "MemoryController.h"
#include "InterruptManager.h"
#include "Timer.h"
#include "SimpleGPU.h"
#include "JoyPad.h"
#include "Logger.h"

typedef void (*CaptureFunc)(const std::array<uint8_t, SCREEN_WIDTH * SCREEN_HEIGHT * 4>&, const std::string&);

class Emulator
{
public:
    Emulator(std::shared_ptr<Logger> logger);
    ~Emulator();

    void InitComponents();
    void ResetComponents();
    void Update();
    void Tick(); // + 1 M-cycle

    void SetCapture(CaptureFunc capture);
    void CaptureScreen(const std::string &fname);

    void Debug_Step(std::vector<char> &blargg_serial, int times);
    void Debug_StepTill(std::vector<char> &blargg_serial, uint16_t x);
    void Debug_PrintEmulatorStatus();

    std::unique_ptr<EmulatorConfig> m_Config;
    std::shared_ptr<Logger> m_EmulatorLogger;

    // All components
    std::unique_ptr<CPU> m_CPU;
    std::unique_ptr<MemoryController> m_MemControl;
    std::unique_ptr<InterruptManager> m_IntManager;
    std::unique_ptr<Timer> m_Timer;
    std::unique_ptr<SimpleGPU> m_GPU;
    std::unique_ptr<JoyPad> m_JoyPad;
private:
    int m_TotalCycles; // T-cycles
    int m_PrevTotalCycles;

    CaptureFunc Capture;
};

#endif