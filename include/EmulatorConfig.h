#ifndef _EMULATOR_CONFIG_H_
#define _EMULATOR_CONFIG_H_

#include <map>
#include <string>

class EmulatorConfig
{
public:
    // bConfigFileExists - used for logging purposes
    EmulatorConfig(bool &bConfigFileExists);
    ~EmulatorConfig();

    std::string GetValue(const std::string& key);
private:
    // Available settings:
    // - UseBootROM
    // - BootROMPath
    // - CPULogging
    // - MMULogging
    // - InterruptLogging
    // - Color0
    // - Color1
    // - Color2
    // - Color3
    std::map<std::string, std::string> m_Settings;

    void DefaultSettings();
};

#endif