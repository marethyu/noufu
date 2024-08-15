#ifndef _CARTRIDGE_H_
#define _CARTRIDGE_H_

#include <memory>
#include <string>

#include "CartridgeROMBase.h"
#include "Logger.h"

class Cartridge
{
private:
    std::string title;

    uint8_t type;
    uint8_t rom_size;
    uint8_t ram_size;

    bool checksum_okay;

    void ParseHeader(const std::string &rom_file, std::shared_ptr<Logger> logger);
public:
    Cartridge(const std::string &rom_file, std::shared_ptr<Logger> logger);
    ~Cartridge();

    std::string GetTitle();

    void SaveRAM();
    void PrintBasicInformation();

    std::unique_ptr<CartridgeROMBase> m_ROM;
    bool ROMLoaded;
};

#endif