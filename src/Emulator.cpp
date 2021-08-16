#include "Constants.h"
#include "Emulator.h"

Emulator::Emulator(std::shared_ptr<Logger> logger)
{
    m_EmulatorLogger = logger;

    bool bConfigFileExists = true;

    m_Config = std::make_unique<EmulatorConfig>(bConfigFileExists);

    if (!bConfigFileExists)
    {
        m_EmulatorLogger->DoLog(LOG_WARN_POPUP, "Emulator::Emulator", "The configuration file was not found, so the new one created with default settings.");
    }

    m_CPU = std::make_unique<CPU>(this, m_Config->GetValue("CPULogging") == "1");
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
    m_EmulatorLogger->DoLog(LOG_INFO, "Emulator::InitComponents", "--Emulator::InitComponents--");
    m_CPU->Init();
    m_IntManager->Init();
    m_Timer->Init();
    m_GPU->Init();
    m_JoyPad->Init();
    m_TotalCycles = m_PrevTotalCycles = 0;
}

void Emulator::ResetComponents()
{
    m_EmulatorLogger->DoLog(LOG_INFO, "Emulator::ResetComponents", "--Emulator::ResetComponents--");
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

void Emulator::SetCapture(CaptureFunc capture)
{
    Capture = capture;
}

int Emulator::CaptureScreen(const std::string &fname)
{
    int ret = Capture(m_GPU->m_Pixels, fname);

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
    m_GPU->Debug_PrintStatus();
    m_JoyPad->Debug_PrintStatus();
}