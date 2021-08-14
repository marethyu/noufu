#ifndef _SIMPLE_GPU_H_
#define _SIMPLE_GPU_H_

#include <array>

#include "Constants.h"
#include "GBComponent.h"
#include "RGBTuple.h"

class SimpleGPU : public GBComponent
{
private:
    Emulator *m_Emulator;

    int m_Counter; // keep track of cycles in a current scanline

    // https://gbdev.io/pandocs/LCDC.html
    bool bLCDEnabled();
    bool bWindowEnable();
    bool bSpriteEnable();
    bool bBGAndWindowEnable();

    // https://gbdev.io/pandocs/STAT.html
    bool bCoincidenceStatInterrupt();
    bool bOAMStatInterrupt();
    bool bVBlankStatInterrupt();
    bool bHBlankStatInterrupt();
    void UpdateMode(int mode);

    void UpdateLCDStatus();
    void DrawCurrentLine();
    void RenderBackground();
    void RenderSprites();

    void RenderPixel(int row, int col, rgb_tuple colour, int attr);
    rgb_tuple GetColour(int colourNum, uint16_t address);
public:
    SimpleGPU(Emulator *emu);
    ~SimpleGPU();

    void Init();
    void Reset();
    void Update(int cycles);
    int GetMode();

    std::array<uint8_t, SCREEN_WIDTH * SCREEN_HEIGHT * 4> m_Pixels;

    uint8_t &LCDC;
    uint8_t &STAT;
    uint8_t &SCY;
    uint8_t &SCX;
    uint8_t &LY;
    uint8_t &LYC;
    uint8_t &BGP;
    uint8_t &OBP0;
    uint8_t &OBP1;
    uint8_t &WY;
    uint8_t &WX;

    void Debug_PrintStatus();
};

#endif