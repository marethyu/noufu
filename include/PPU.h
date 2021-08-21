#ifndef _PPU_H_
#define _PPU_H_

#include <array>
#include <deque>
#include <memory>
#include <string>

#include "Constants.h"
#include "GBComponent.h"

struct rgb_tuple
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

struct Sprite
{
    uint8_t x; // raw
    uint8_t y; // raw
    uint8_t tileIndex;
    uint8_t attr;
    uint16_t oamAddr;

    bool operator<(const Sprite &other)
    {
        if (x == other.x)
        {
            return oamAddr < other.oamAddr;
        }

        return x < other.x;
    }
};

class PixelFetcher;

class PPU : public GBComponent
{
    friend class PixelFetcher;
private:
    Emulator *m_Emulator;

    rgb_tuple gb_colours[4];                // colour palette
    bool bResetted;                         // true if LCDC.7 is 0, false otherwise
    int nDots;                              // count number of dots passed in the current scanline (max: 456)
    std::deque<Sprite> oamBuffer;           // sprite OAM buffer (filled during OAM search phase)

    int LX;                                 // it's like LY but for X-coord (max: 160)
    int WLY;                                // window internal line counter
    bool wyTrigger;                         // true when LY=WY

    std::unique_ptr<PixelFetcher> pixFetcher;

    /** LCDC functions **/
    // https://gbdev.io/pandocs/LCDC.html
    bool bLCDEnabled();                     // LCDC.7: 0=Off, 1=On
    bool bWindowTileMap();                  // LCDC.6: 0=9800-9BFF, 1=9C00-9FFF
    bool bWindowEnabled();                  // LCDC.5: 0=Off, 1=On
    bool bBackgroundAndWindowTileData();    // LCDC.4: 0=8800-97FF, 1=8000-8FFF
    bool bBackgroundTileMap();              // LCDC.3: 0=9800-9BFF, 1=9C00-9FFF
    bool bSprite8x16();                     // LCDC.2: 0=8x8, 1=8x16
    bool bSpriteEnabled();                  // LCDC.1: 0=Off, 1=On
    bool bBGAndWindowEnabled();             // LCDC.0: 0=Off, 1=On

    /** STAT functions **/
    // https://gbdev.io/pandocs/STAT.html
    bool bCoincidenceStatInterrupt();
    bool bOAMStatInterrupt();
    bool bVBlankStatInterrupt();
    bool bHBlankStatInterrupt();

    void LYCompare();
    void SwitchMode(int mode);
    void SetMode(int mode);
    int GetMode();

    /** PPU functions for each mode (each call eats an cycle (dot)) **/
    void DoOAMSearch();     // mode 2
    void DoPixelTransfer(); // mode 3
    void DoHBlank();        // mode 0
    void DoVBlank();        // mode 1

    /** Rendering functions **/
    void DrawPixel(int x, int y, const rgb_tuple &colour);

    /** Misc **/
    void InitRGBTuple(rgb_tuple &tup, const std::string &colour_info);
    void PrintPalette(uint8_t palette);
public:
    PPU(Emulator *emu);
    ~PPU();

    void Init();
    void Reset();
    void Update(int cycles);

    // the below functions are used by CPU only
    uint8_t CPUReadVRAM(uint16_t address);
    uint8_t CPUReadOAM(uint16_t address);
    void CPUWriteVRAM(uint16_t address, uint8_t data);
    void CPUWriteOAM(uint16_t address, uint8_t data);

    void Debug_PrintStatus();

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
};

#endif