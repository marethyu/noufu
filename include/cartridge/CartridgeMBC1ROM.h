#ifndef _CARTRIDGE_MBC1_ROM_H_
#define _CARTRIDGE_MBC1_ROM_H_

#include <string>
#include <vector>

#include "CartridgeROMBase.h"

// https://hacktixme.ga/GBEDG/mbcs/mbc1/
class CartridgeMBC1ROM : public CartridgeROMBase
{
private:
    uint8_t rom_size;
    uint8_t ram_size;

    uint8_t rom_bank;
    uint8_t ram_bank;

    bool use_ram;
    bool battery_buf;
    bool ram_enable;
    bool mode_flag;

    std::vector<uint8_t> m_RAM;

    uint8_t CalcZeroBankNumber();
    uint8_t CalcHighBankNumber();

    void LoadSAVIfExists();
public:
    CartridgeMBC1ROM(const std::string &rom_file, uint8_t rom_size, uint8_t ram_size, uint8_t type);
    ~CartridgeMBC1ROM();

    uint8_t ReadByte(uint16_t address);
    void WriteByte(uint16_t address, uint8_t data);

    uint8_t RamReadByte(uint16_t address);
    void RamWriteByte(uint16_t address, uint8_t data);

    void SaveRAM();
};

#endif