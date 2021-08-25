#include <fstream>

#include "cartridge/CartridgePlainROM.h"

CartridgePlainROM::CartridgePlainROM(const std::string &rom_file)
  : CartridgeROMBase(rom_file)
{
}

CartridgePlainROM::~CartridgePlainROM()
{
    
}

uint8_t CartridgePlainROM::ReadByte(uint16_t address)
{
    return m_ROM[address];
}

void CartridgePlainROM::WriteByte(uint16_t address, uint8_t data)
{
    
}

uint8_t CartridgePlainROM::RamReadByte(uint16_t address)
{
    return 0xFF;
}

void CartridgePlainROM::RamWriteByte(uint16_t address, uint8_t data)
{
    
}

void CartridgePlainROM::SaveRAM()
{
    
}