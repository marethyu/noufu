#include "Constants.h"
#include "Emulator.h"

Emulator::Emulator(std::shared_ptr<Logger> logger, std::shared_ptr<EmulatorConfig> config)
{
    m_EmulatorLogger = logger;
    m_Config = config;

    bgPreview = std::stoi(config->GetValue("PreviewBackground"));
    dispWidth = SCREEN_WIDTH + (bgPreview ? 48 : 0);
    dispHeight = SCREEN_HEIGHT + (bgPreview ? 48 : 0);

    m_CPU = std::make_unique<CPU>(this);
    m_MemControl = std::make_unique<MemoryController>(this);
    m_IntManager = std::make_unique<InterruptManager>(this);
    m_Timer = std::make_unique<Timer>(this);
    m_PPU = std::make_unique<PPU>(this);
    m_JoyPad = std::make_unique<JoyPad>(this);
}

Emulator::~Emulator()
{
    
}

void Emulator::InitComponents()
{
    m_EmulatorLogger->DoLog(LOG_INFO, "Emulator::InitComponents", "--Emulator::InitComponents--");
    m_CPU->Init();
    m_MemControl->Init();
    m_IntManager->Init();
    m_Timer->Init();
    m_PPU->Init();
    m_JoyPad->Init();
    m_TotalCycles = m_PrevTotalCycles = 0;
}

void Emulator::ResetComponents()
{
    m_EmulatorLogger->DoLog(LOG_INFO, "Emulator::ResetComponents", "--Emulator::ResetComponents--");
    m_CPU->Reset();
    m_MemControl->Reset();
    m_IntManager->Reset();
    m_Timer->Reset();
    m_PPU->Reset();
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
        m_PPU->Update(cycles);
        m_Timer->Update(cycles);
    }

    m_PrevTotalCycles = m_TotalCycles;
}

void Emulator::Tick()
{
    m_TotalCycles += 4;
}

void Emulator::SetCapture(CaptureFunc capture)
{
    Capture = capture;
}

int Emulator::CaptureScreen(const std::string &fname)
{
    int ret = Capture(m_PPU->m_Pixels, std::stoi(m_Config->GetValue("ScreenScaleFactor")), fname, bgPreview);

    if (ret)
    {
        m_EmulatorLogger->DoLog(LOG_INFO, "Emulator::CaptureScreen", "Screenshot saved (file={})", fname);
    }
    else
    {
        m_EmulatorLogger->DoLog(LOG_WARN_POPUP, "Emulator::CaptureScreen", "Unable to save a screenshot");
    }

    return ret;
}

void Emulator::Debug_Step(std::vector<char> &blargg_serial, int times)
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
        m_PPU->Update(cycles);
        m_Timer->Update(cycles);

        // blarggs test - serial output
        if (m_MemControl->ReadByte(0xFF02) == 0x81)
        {
            blargg_serial.push_back(m_MemControl->ReadByte(0xFF01));
            m_MemControl->WriteByte(0xFF02, 0x0);
        }
    }
}

void Emulator::Debug_StepTill(std::vector<char> &blargg_serial, uint16_t x)
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
    m_PPU->Debug_PrintStatus();
    m_JoyPad->Debug_PrintStatus();
}