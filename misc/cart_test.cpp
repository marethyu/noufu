#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>

namespace fs = std::filesystem;

const std::unordered_map<uint8_t, std::string> rom_sizes = {
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

const std::unordered_map<uint8_t, std::string> ram_sizes = {
    {0x0, "0"},
    {0x1, "- (Unused)"},
    {0x2, "8 KB"},
    {0x3, "32 KB"},
    {0x4, "128 KB"},
    {0x5, "64 KB"}
};

void PrintCartridgeInfo(const std::string &rom_file)
{
    std::ifstream fin(rom_file, std::ios::in);

    if (!fin.is_open())
    {
        std::cout << "Error: Can't open " << rom_file << std::endl;
        return;
    }

    fin.seekg(0x100);

    std::array<uint8_t, 0x4F> buffer;
    fin.read(reinterpret_cast<char *>(buffer.data()), 0x4F);

    std::string title;
    for (int i = 0; i < 16; ++i)
    {
        if (buffer[0x34 + i] == '\0')
        {
            break;
        }

        title.append(1, buffer[0x34 + i]);
    }

    uint16_t checksum = 0;
    for (int i = 0x34; i <= 0x4C; ++i)
    {
        checksum = checksum - buffer[i] - 1;
    }

    std::cout << "BASIC CARTRIDGE INFORMATION FOR " << rom_file << std::endl;
    std::cout << "TITLE: " << title << std::endl;
    std::cout << "TYPE: " << int(buffer[0x47]) << std::endl;
    std::cout << "ROM SIZE: " << rom_sizes.at(buffer[0x48]) << std::endl;
    std::cout << "RAM SIZE: " << ram_sizes.at(buffer[0x49]) << std::endl;
    std::cout << "CHECKSUM OKAY: " << ((checksum & 0xFF) == buffer[0x4D]) << std::endl;
    std::cout << std::endl;
}

int main()
{
    std::string path("./ROMS/");
    std::string ext(".gb");

    for (auto &p : fs::recursive_directory_iterator(path))
    {
        if (p.path().extension() == ext)
        {
            PrintCartridgeInfo(p.path().string());
        }
    }

    return 0;
}