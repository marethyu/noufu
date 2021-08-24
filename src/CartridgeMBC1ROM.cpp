#include <fstream>

#include "BitMagic.h"

#include "cartridge/CartridgeMBC1ROM.h"

static const uint8_t rb_bitmask[7] = {
    1,  // 0b00000001
    3,  // 0b00000011
    7,  // 0b00000111
    15, // 0b00001111
    31, // 0b00011111
    31, // 0b00011111
    31  // 0b00011111
};

uint8_t CartridgeMBC1ROM::CalcZeroBankNumber()
{
    uint8_t zero_bankn = 0; // default

    // TODO support https://gbdev.io/pandocs/MBC1.html#note-for-1-mb-multi-game-compilation-carts
    if (rom_size == 5 && GET_BIT(ram_bank, 0))
    {
        SET_BIT(zero_bankn, 5);
    }

    if (rom_size == 6 && GET_BIT(ram_bank, 1))
    {
        SET_BIT(zero_bankn, 6);
    }

    return zero_bankn;
}

uint8_t CartridgeMBC1ROM::CalcHighBankNumber()
{
    uint8_t high_bankn = rom_bank;

    if (rom_size == 5)
    {
        if (GET_BIT(ram_bank, 0))
        {
            SET_BIT(high_bankn, 5);
        }
        else
        {
            RES_BIT(high_bankn, 5);
        }
    }

    if (rom_size == 6)
    {
        if (GET_BIT(ram_bank, 1))
        {
            SET_BIT(high_bankn, 6);
        }
        else
        {
            RES_BIT(high_bankn, 6);
        }
    }

    return high_bankn;
}

CartridgeMBC1ROM::CartridgeMBC1ROM(const std::string &rom_file, uint8_t rom_size, uint8_t ram_size, uint8_t type) : rom_size(rom_size), ram_size(ram_size)
{
    std::ifstream istream(rom_file, std::ios::in | std::ios::binary);
    m_ROM = std::vector<uint8_t>(std::istreambuf_iterator<char>(istream), std::istreambuf_iterator<char>());
    istream.close();

    rom_bank = 1;
    ram_bank = 0;
    ram_enable = false;
    mode_flag = false;

    switch (ram_size)
    {
    case 0x0: break;
    case 0x1:
    {
        m_RAM.resize(2048);
        break;
    }
    case 0x2:
    {
        m_RAM.resize(8192);
        break;
    }
    case 0x3:
    {
        m_RAM.resize(32768);
        break;
    }
    }

    switch (type)
    {
    case MBC_PLAIN:
    {
        use_ram = false;
        break;
    }
    case MBC_RAM:
    {
        use_ram = true;
        battery_buf = false;
        break;
    }
    case MBC_RAM_BATTERY:
    {
        use_ram = true;
        battery_buf = true;
        break;
    }
    }
}

CartridgeMBC1ROM::~CartridgeMBC1ROM()
{
    
}

uint8_t CartridgeMBC1ROM::ReadByte(uint16_t address)
{
    if (address < 0x4000)
    {
        if (mode_flag)
        {
            return m_ROM[0x4000 * CalcZeroBankNumber() + address];
        }

        return m_ROM[address];
    }

    return m_ROM[0x4000 * CalcHighBankNumber() + (address - 0x4000)];
}

void CartridgeMBC1ROM::WriteByte(uint16_t address, uint8_t data)
{
    if (address < 0x2000)
    {
        ram_enable = (data & 0b1111) == 0xA;
    }
    else if (address >= 0x2000 && address < 0x4000)
    {
        if ((data & 0b11111) == 0)
        {
            rom_bank = 1;
        }
        else
        {
            rom_bank = data & rb_bitmask[rom_size];
        }
    }
    else if (address >= 0x4000 && address < 0x6000)
    {
        ram_bank = data & 0x3;
    }
    else if (address >= 0x6000)
    {
        mode_flag = data & 1;
    }
}

uint8_t CartridgeMBC1ROM::RamReadByte(uint16_t address)
{
    if (use_ram && ram_enable)
    {
        if (ram_size == 1 || ram_size == 2)
        {
            return m_RAM[(address - 0xA000) % m_RAM.size()];
        }
        else // ram_size == 3
        {
            if (mode_flag)
            {
                return m_RAM[0x2000 * ram_bank + (address - 0xA000)];
            }

            return m_RAM[address - 0xA000];
        }
    }

    return 0xFF;
}

void CartridgeMBC1ROM::RamWriteByte(uint16_t address, uint8_t data)
{
    if (use_ram && ram_enable)
    {
        if (ram_size == 1 || ram_size == 2)
        {
            m_RAM[(address - 0xA000) % m_RAM.size()] = data;
        }
        else // ram_size == 3
        {
            if (mode_flag)
            {
                m_RAM[0x2000 * ram_bank + (address - 0xA000)] = data;
            }

            m_RAM[address - 0xA000] = data;
        }
    }
}