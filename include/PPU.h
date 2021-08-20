#ifndef _PPU_H_
#define _PPU_H_

#include <array>
#include <queue>
#include <string>

#include "Constants.h"
#include "GBComponent.h"

struct rgb_tuple
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

struct Pixel
{
    int colourNum;
};

class PPU : public GBComponent
{
private:
    Emulator *m_Emulator;

    bool bResetted; // true if LCDC.7 is 0, false otherwise
    int nDots;      // count number of dots passed in the current scanline (max: 456)

    /** Pixel fetcher variables **/
    int fetcherX;               // can be understood as an X-position in an 32x32 tile map
    int LX;                     // it's like LY but for X-coord (max: 160)
    int pf_state;               // state of pixel fetcher
    uint16_t addr;              // temp variable for use in PPU::TickFetcher
    uint8_t tileNo;             // tile number (set in PF_GET_TILE_NO mode)
    uint8_t tileLo;             // the lower byte of a tile (set in PF_GET_TDATA_LO)
    uint8_t tileHi;             // the upper byte of a tile (set in PF_GET_TDATA_HI)
    int ticks;                  // used to make sure that the fetcher runs every 2 dots
    std::queue<Pixel> bgFIFO;   // background FIFO (TODO implement and use a circular queue)

    void InitFetcher();
    void TickFetcher();

    rgb_tuple gb_colours[4]; // colour palette

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
    rgb_tuple GetColour(int colourNum, uint8_t palette);
    int GetValue(uint8_t palette, int hi, int lo);
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