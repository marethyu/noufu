#include <fstream>

#include "cartridge/CartridgePlainROM.h"

CartridgePlainROM::CartridgePlainROM(const std::string &rom_file)
{
    std::ifstream istream(rom_file, std::ios::in | std::ios::binary);

    istream.read(reinterpret_cast<char *>(m_Memory.data()), 0x8000);
    istream.close();
}

CartridgePlainROM::~CartridgePlainROM()
{
    
}

uint8_t CartridgePlainROM::ReadByte(uint16_t address)
{
    return m_Memory[address];
}

void CartridgePlainROM::WriteByte(uint16_t address, uint8_t data)
{
    
}

uint8_t CartridgePlainROM::RamReadByte(uint16_t address)
{
    return 0x0;
}

void CartridgePlainROM::RamWriteByte(uint16_t address, uint8_t data)
{
    
}