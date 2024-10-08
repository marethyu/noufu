#include <fstream>
#include <iostream>

#include "EmulatorConfig.h"

EmulatorConfig::EmulatorConfig()
{
    
}

EmulatorConfig::~EmulatorConfig()
{
    
}

bool EmulatorConfig::InitSettings()
{
    std::ifstream istream("config.ini", std::ios::in);
    std::string line;

    if (!istream)
    {
        EmulatorConfig::DoDefaultSettings();

        std::ofstream config("config.ini");

        for (auto it = m_Settings.begin(); it != m_Settings.end(); it++)
        {
            config << it->first << "=" << it->second << std::endl;
        }

        config.close();

        return true;
    }

    while (std::getline(istream, line))
    {
        /* basic format: key=value */
        std::string key = line.substr(0, line.find("="));
        std::string value = line.substr(line.find("=") + 1);
        m_Settings[key] = value;
    }

    istream.close();

    return false;
}

std::string EmulatorConfig::GetValue(const std::string& key)
{
    return m_Settings[key];
}

void EmulatorConfig::DoDefaultSettings()
{
    m_Settings["ScreenScaleFactor"] = "3";
    m_Settings["UseBootROM"] = "0";
    m_Settings["BootROMPath"] = "";
    m_Settings["CPULogging"] = "0";
    m_Settings["MMULogging"] = "0";
    m_Settings["InterruptLogging"] = "0";
    m_Settings["Color0"] = "E0.F8.D0";
    m_Settings["Color1"] = "88.C0.70";
    m_Settings["Color2"] = "34.68.56";
    m_Settings["Color3"] = "08.18.20";
    m_Settings["PreviewBackground"] = "0";
}