#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include "Emulator.h"
#include "MemoryController.h"

#define FMT_HEADER_ONLY
#include "fmt/format.h"

void MemoryController::InitBootROM()
{
    std::string path = m_Emulator->m_Config->GetValue("BootROMPath");

    std::ifstream istream(path, std::ios::in | std::ios::binary);

    if (!istream)
    {
        m_Emulator->m_EmulatorLogger->DoLog(LOG_ERROR, "MMU::InitBootROM", "Failed to open {}", path);
        throw std::system_error(errno, std::system_category(), "failed to open " + path);
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
    bLoggingEnabled = emu->m_Config->GetValue("MMULogging") == "1";
    bUseBootROM = emu->m_Config->GetValue("UseBootROM") == "1";

    if (bUseBootROM)
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

void MemoryController::LoadROM(const std::string &rom_file)
{
    std::ifstream istream(rom_file, std::ios::in | std::ios::binary);

    if (!istream)
    {
        m_Emulator->m_EmulatorLogger->DoLog(LOG_ERROR, "MMU::LoadROM", "Failed to open {}", rom_file);
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

    if (!bUseBootROM)
    {
        m_Emulator->ResetComponents();
    }
    else
    {
        inBootMode = true;
    }

    m_Emulator->m_EmulatorLogger->DoLog(LOG_INFO, "MMU::LoadROM", "Loaded {} (BOOT_MODE={:d})", rom_file, inBootMode);
}

void MemoryController::ReloadROM()
{
    if (!bUseBootROM)
    {
        m_Emulator->ResetComponents();
    }
    else
    {
        inBootMode = true;
    }

    m_Emulator->m_EmulatorLogger->DoLog(LOG_INFO, "MMU::ReloadROM", "Current ROM reloaded (BOOT_MODE={:d})", inBootMode);
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
        if (bLoggingEnabled)
        {
            m_Emulator->m_EmulatorLogger->DoLog(LOG_WARNING, "MMU::ReadByte", "Attempted to read from $A000-$BFFF; Probably need to implement MBC; address={0:04X}", address);
        }
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
        if (bLoggingEnabled)
        {
            m_Emulator->m_EmulatorLogger->DoLog(LOG_WARNING, "MMU::ReadByte", "Attempted to read from $FEA0-$FEFF; address={0:04X}", address);
        }
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
        m_Emulator->m_EmulatorLogger->DoLog(LOG_ERROR, "MMU::ReadByte", "Invalid address range: {0:04X}", address);
        throw std::runtime_error("Bad memory access!");
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
        if (bLoggingEnabled)
        {
            m_Emulator->m_EmulatorLogger->DoLog(LOG_WARNING, "MMU::WriteByte", "Attempted to write to $00-$FF; address={0:04X}", address);
        }
    }
    else if (address < 0x8000)
    {
        if (bLoggingEnabled)
        {
            m_Emulator->m_EmulatorLogger->DoLog(LOG_WARNING, "MMU::WriteByte", "Attempted to write to $0000-$7FFF; address={0:04X}", address);
        }
    }
    else if (address >= 0x8000 && address < 0xA000)
    {
        m_VRAM[address - 0x8000] = data;
    }
    else if (address >= 0xA000 && address < 0xC000)
    {
        if (bLoggingEnabled)
        {
            m_Emulator->m_EmulatorLogger->DoLog(LOG_WARNING, "MMU::WriteByte", "Attempted to write to $A000-$BFFF; Probably need to implement MBC; address={0:04X}", address);
        }
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
        if (bLoggingEnabled)
        {
            m_Emulator->m_EmulatorLogger->DoLog(LOG_WARNING, "MMU::WriteByte", "Attempted to write to $FEA0-$FEFF; address={0:04X}", address);
        }
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
        m_Emulator->m_EmulatorLogger->DoLog(LOG_ERROR, "MMU::WriteByte", "Invalid address range: {0:04X}", address);
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
    std::cout << fmt::format("memory value at ${0:04X} is ${0:02X}", address, MemoryController::ReadByte(address)) << std::endl;
}

void MemoryController::Debug_EditMemory(uint16_t address, uint8_t data)
{
    MemoryController::WriteByte(address, data);
    std::cout << fmt::format("memory value at ${0:04X} is now changed to ${0:02X}", address, data) << std::endl;
}

void MemoryController::Debug_PrintMemoryRange(uint16_t start, uint16_t end)
{
    std::cout << "address: value" << std::endl;

    for (uint16_t addr = start; addr < end; ++addr)
    {
        std::cout << fmt::format("${0:04X}: ${0:02X}", addr, MemoryController::ReadByte(addr)) << std::endl;
    }
}