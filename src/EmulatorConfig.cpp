#include <fstream>
#include <iostream>

#include "EmulatorConfig.h"

#include "loguru.hpp"

EmulatorConfig::EmulatorConfig()
{
    std::ifstream istream("config", std::ios::in);
    std::string line;

    if (!istream)
    {
#ifdef GUI_MODE
        LOG_F(INFO, "config is not found, creating one with default settings...");
#else
        /*fout << "config is not found, creating one with default settings..." << std::endl;*/
#endif
        m_Settings["UseBootROM"] = "0";
        m_Settings["BootROMPath"] = "";

        std::ofstream config("config");

        for (auto it = m_Settings.begin(); it != m_Settings.end(); it++)
        {
            config << it->first << "=" << it->second << std::endl;
        }

        config.close();

        return;
    }

    while (std::getline(istream, line))
    {
        /* basic format: key=value */
        std::string key = line.substr(0, line.find("="));
        std::string value = line.substr(line.find("=") + 1);
        m_Settings[key] = value;
    }

    istream.close();
}

EmulatorConfig::~EmulatorConfig()
{
    
}

std::string EmulatorConfig::GetValue(const std::string& key)
{
    return m_Settings[key];
}