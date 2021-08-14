#ifndef _EMULATOR_CONFIG_H_
#define _EMULATOR_CONFIG_H_

#include <map>
#include <string>

class EmulatorConfig
{
public:
    EmulatorConfig();
    ~EmulatorConfig();

    std::string GetValue(const std::string& key);
private:
    // Available settings:
    // - UseBootROM
    // - BootROMPath
    std::map<std::string, std::string> m_Settings;
};

#endif