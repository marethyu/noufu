#ifndef _CARTRIDGE_ROM_BASE_H_
#define _CARTRIDGE_ROM_BASE_H_

class CartridgeROMBase
{
public:
    virtual uint8_t ReadByte(uint16_t address) = 0;
    virtual void WriteByte(uint16_t address, uint8_t data) = 0;

    virtual uint8_t RamReadByte(uint16_t address) = 0;
    virtual void RamWriteByte(uint16_t address, uint8_t data) = 0;
};

#endif