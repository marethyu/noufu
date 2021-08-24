#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unordered_map>

#include "cartridge/Cartridge.h"
#include "cartridge/CartridgePlainROM.h"
#include "cartridge/CartridgeMBC1ROM.h"

static const std::unordered_map<uint8_t, std::string> cart_types = {
    {0x0, "ROM ONLY"},
    {0x1, "MBC1"},
    {0x2, "MBC1+RAM"},
    {0x3, "MBC1+RAM+BATTERY"},
    {0x5, "MBC2"},
    {0x6, "MBC2+RAM+BATTERY"}
};

static const std::unordered_map<uint8_t, std::string> rom_sizes = {
    {0x0, "32 KByte"},
    {0x1, "64 KByte"},
    {0x2, "128 KByte"},
    {0x3, "256 KByte"},
    {0x4, "512 KByte"},
    {0x5, "1 MByte"},
    {0x6, "2 MByte"},
    {0x7, "4 MByte"},
    {0x8, "8 MByte"}
};

static const std::unordered_map<uint8_t, std::string> ram_sizes = {
    {0x0, "0"},
    {0x1, "- (Unused)"},
    {0x2, "8 KB"},
    {0x3, "32 KB"},
    {0x4, "128 KB"},
    {0x5, "64 KB"}
};

static std::string GetValue(const std::unordered_map<uint8_t, std::string> &mp, uint8_t n)
{
    if (mp.find(n) == mp.end())
    {
        return "unk";
    }

    return mp.at(n);
}

void Cartridge::ParseHeader(const std::string &rom_file, std::shared_ptr<Logger> logger)
{
    std::ifstream fin(rom_file, std::ios::in);

    if (!fin.is_open())
    {
        logger->DoLog(LOG_ERROR, "Cartridge::ParseHeader", "Unable to open {}", rom_file);
        throw std::system_error(errno, std::system_category(), "failed to open " + rom_file);
    }

    logger->DoLog(LOG_INFO, "Cartridge::ParseHeader", "Opened {}", rom_file);

    fin.seekg(0x100);

    std::array<uint8_t, 0x4F> buffer;
    fin.read(reinterpret_cast<char *>(buffer.data()), 0x4F);

    // get title
    for (int i = 0; i < 16; ++i)
    {
        if (buffer[0x34 + i] == '\0')
        {
            break;
        }

        title.append(1, buffer[0x34 + i]);
    }

    type = buffer[0x47];
    rom_size = buffer[0x48];
    ram_size = buffer[0x49];

    // checksum verification
    uint16_t x = 0;
    int i = 0x34;

    while (i <= 0x4C)
    {
        x = x - buffer[i] - 1;
        i++;
    }

    checksum_okay = (x & 0xFF) == buffer[0x4D];

    fin.close();

    std::string type_str = GetValue(cart_types, type);
    std::string rom_size_str = GetValue(rom_sizes, rom_size);
    std::string ram_size_str = GetValue(ram_sizes, ram_size);

    logger->DoLog(LOG_INFO, "Cartridge::ParseHeader", "title: {}", title);

    if (type_str == "unk")
    {
        logger->DoLog(LOG_WARN_POPUP, "Cartridge::ParseHeader", "Unknown cartridge type ({0:02X})", type);
    }
    else
    {
        logger->DoLog(LOG_INFO, "Cartridge::ParseHeader", "cartridge type: {}", type_str);
    }

    if (rom_size_str == "unk")
    {
        logger->DoLog(LOG_WARN_POPUP, "Cartridge::ParseHeader", "Unknown cartridge ROM size ({0:02X})", rom_size);
    }
    else
    {
        logger->DoLog(LOG_INFO, "Cartridge::ParseHeader", "ROM size: {}", rom_size_str);
    }

    if (ram_size_str == "unk")
    {
        logger->DoLog(LOG_WARN_POPUP, "Cartridge::ParseHeader", "Unknown cartridge RAM size ({0:02X})", ram_size);
    }
    else
    {
        logger->DoLog(LOG_INFO, "Cartridge::ParseHeader", "RAM size: {}", ram_size_str);
    }

    if (!checksum_okay)
    {
        logger->DoLog(LOG_WARN_POPUP, "Cartridge::ParseHeader", "Checksum verification failed");
    }
    else
    {
        logger->DoLog(LOG_INFO, "Cartridge::ParseHeader", "Checksum okay");
    }
}

Cartridge::Cartridge(const std::string &rom_file, std::shared_ptr<Logger> logger, bool &success)
{
    ParseHeader(rom_file, logger);

    switch (type)
    {
    case 0x0:
    {
        m_ROM = std::make_unique<CartridgePlainROM>(rom_file);
        success = true;
        break;
    }
    case 0x1:
    {
        m_ROM = std::make_unique<CartridgeMBC1ROM>(rom_file, rom_size, ram_size, MBC_PLAIN);
        success = true;
        break;
    }
    case 0x2:
    {
        m_ROM = std::make_unique<CartridgeMBC1ROM>(rom_file, rom_size, ram_size, MBC_RAM);
        success = true;
        break;
    }
    case 0x3:
    {
        m_ROM = std::make_unique<CartridgeMBC1ROM>(rom_file, rom_size, ram_size, MBC_RAM_BATTERY);
        success = true;
        break;
    }
    default:
    {
        logger->DoLog(LOG_WARN_POPUP, "Cartridge::Cartridge", "This cartridge type is possibly unknown or not yet supported. Stopping.");
        success = false;
        break;
    }
    }
}

Cartridge::~Cartridge()
{
    
}

std::string Cartridge::GetTitle()
{
    return title;
}

void Cartridge::PrintBasicInformation()
{
    std::cout << "**BASIC CARTRIDGE INFORMATION**" << std::endl;
    std::cout << "TITLE: " << title << std::endl;
    std::cout << "TYPE: " << GetValue(cart_types, type) << std::endl;
    std::cout << "ROM SIZE: " << GetValue(rom_sizes, rom_size) << std::endl;
    std::cout << "RAM SIZE: " << GetValue(ram_sizes, ram_size) << std::endl;
    std::cout << "CHECKSUM OKAY: " << checksum_okay << std::endl;
    std::cout << std::endl;
}