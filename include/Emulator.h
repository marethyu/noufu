#ifndef _EMULATOR_H_
#define _EMULATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "EmulatorConfig.h"
#include "MemoryController.h"
#include "InterruptManager.h"
#include "Timer.h"
#include "JoyPad.h"
#include "Logger.h"

#include "cpu/CPU.h"
#include "ppu/PPU.h"

typedef int (*CaptureFunc)(const std::vector<uint8_t>&, int, const std::string&, bool);

class Emulator
{
public:
    Emulator(std::shared_ptr<Logger> logger, std::shared_ptr<EmulatorConfig> config);
    ~Emulator();

    void InitComponents();
    void ResetComponents();
    void Update();
    void Tick(); // + 1 M-cycle

    void SetCapture(CaptureFunc capture);
    int CaptureScreen(const std::string &fname);

    void Debug_Step(std::vector<char> &blargg_serial, int times);
    void Debug_StepTill(std::vector<char> &blargg_serial, uint16_t x);
    void Debug_PrintEmulatorStatus();

    std::shared_ptr<Logger> m_EmulatorLogger;
    std::shared_ptr<EmulatorConfig> m_Config;

    // All components
    std::unique_ptr<CPU> m_CPU;
    std::unique_ptr<MemoryController> m_MemControl;
    std::unique_ptr<InterruptManager> m_IntManager;
    std::unique_ptr<Timer> m_Timer;
    std::unique_ptr<PPU> m_PPU;
    std::unique_ptr<JoyPad> m_JoyPad;

    bool bgPreview;

    int dispWidth;
    int dispHeight;
private:
    int m_TotalCycles; // T-cycles
    int m_PrevTotalCycles;

    CaptureFunc Capture;
};

#endif