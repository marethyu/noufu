#ifndef _EMULATOR_CONFIG_H_
#define _EMULATOR_CONFIG_H_

#include <map>
#include <string>

class EmulatorConfig
{
public:
    EmulatorConfig();
    ~EmulatorConfig();

    // Returns true if config.ini is created
    bool InitSettings();

    std::string GetValue(const std::string& key);
private:
    // Available settings:
    // - ScreenScaleFactor
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

    void DoDefaultSettings();
};

#endif