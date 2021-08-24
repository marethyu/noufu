#ifndef _CARTRIDGE_ROM_BASE_H_
#define _CARTRIDGE_ROM_BASE_H_

#include <fstream>
#include <string>
#include <vector>

#define MBC_PLAIN 0
#define MBC_RAM 1
#define MBC_RAM_BATTERY 2

class CartridgeROMBase
{
protected:
    std::vector<uint8_t> m_ROM;

    CartridgeROMBase(const std::string &rom_file)
    {
        std::ifstream istream(rom_file, std::ios::in | std::ios::binary);
        m_ROM = std::vector<uint8_t>(std::istreambuf_iterator<char>(istream), std::istreambuf_iterator<char>());
        istream.close();
    }
public:
    virtual uint8_t ReadByte(uint16_t address) = 0;
    virtual void WriteByte(uint16_t address, uint8_t data) = 0;

    virtual uint8_t RamReadByte(uint16_t address) = 0;
    virtual void RamWriteByte(uint16_t address, uint8_t data) = 0;
};

#endif