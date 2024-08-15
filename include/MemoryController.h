#ifndef _MEMORY_CONTROLLER_H_
#define _MEMORY_CONTROLLER_H_

#include <array>
#include <memory>
#include <string>

#include "GBComponent.h"

#include "cartridge/Cartridge.h"

class MemoryController : public GBComponent
{
private:
    // Please refer to:
    // - https://gbdev.io/pandocs/Memory_Map.html
    // - http://gameboy.mongenel.com/dmg/asmmemmap.html
    std::array<uint8_t, 0x2000> m_VRAM;
    std::array<uint8_t, 0x4000> m_WRAM;
    std::array<uint8_t, 0xA0> m_OAM;
    std::array<uint8_t, 0x7F> m_HRAM;

    Emulator *m_Emulator;

    bool bLoggingEnabled;

    bool bUseBootROM;
    bool inBootMode;
    std::array<uint8_t, 0x100> m_BootROM;

    void InitBootROM();
    void DMATransfer(uint8_t data);
public:
    MemoryController(Emulator *emu);
    ~MemoryController();

    void Init();
    void Reset() {} // nothing
    bool LoadROM(const std::string &rom_file);
    void ReloadROM();
    uint8_t ReadByte(uint16_t address) const;
    uint16_t ReadWord(uint16_t address) const;
    void WriteByte(uint16_t address, uint8_t data);
    void WriteWord(uint16_t address, uint16_t data);

    void Debug_PrintStatus() {} // nothing
    void Debug_PrintMemory(uint16_t address);
    void Debug_EditMemory(uint16_t address, uint8_t data);
    void Debug_PrintMemoryRange(uint16_t start, uint16_t end);

    std::array<uint8_t, 0x80> m_IO;

    uint8_t IE;

    std::unique_ptr<Cartridge> m_Cartridge;
};

#endif