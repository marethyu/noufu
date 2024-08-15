#ifndef _CARTRIDGE_PLAIN_ROM_H_
#define _CARTRIDGE_PLAIN_ROM_H_

#include <array>
#include <string>

#include "CartridgeROMBase.h"

class CartridgePlainROM : public CartridgeROMBase
{
public:
    CartridgePlainROM(const std::string &rom_file);
    ~CartridgePlainROM();

    uint8_t ReadByte(uint16_t address);
    void WriteByte(uint16_t address, uint8_t data);

    uint8_t RamReadByte(uint16_t address);
    void RamWriteByte(uint16_t address, uint8_t data);

    void SaveRAM();
};

#endif