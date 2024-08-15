#ifndef _PIXEL_FETCHER_H_
#define _PIXEL_FETCHER_H_

#include "PixelFIFO.h"
#include "PPU.h"

class PixelFetcher
{
private:
    PPU *m_PPU;

    int fetcherX;               // can be understood as an X-position in an 32x32 tile map
    int pf_state1;              // state of the pixel fetcher (for fetching background or window pixels)
    int pf_state2;              // state of the pixel fetcher (for fetching sprite pixels)
    int bgf_ticks;              // for making sure that the background pixel fetcher runs every 2 cycles
    int spf_ticks;              // for making sure that the sprite pixel fetcher runs every 2 cycles
    bool fetchingSprites;       // true if currently fetching sprite pixels, false otherwise
    bool fetchingWindow;        // true if currently fetching window pixels, false otherwise
    Sprite cur_sprite;          // temporary variable for use in PPU::TickSpritePixFetcher
    uint16_t addr;              // temporary variable for use in PPU::TickFetcher
    uint8_t tileNo;             // tile number (set in PF_GET_TILE_NO mode)
    uint8_t tileLo;             // the lower byte of a tile (set in PF_GET_TDATA_LO)
    uint8_t tileHi;             // the upper byte of a tile (set in PF_GET_TDATA_HI)
    uint8_t sTileNo;            // tile number (set in PF_GET_TILE_NO mode when fetching sprite pixels)
    uint8_t sTileLo;            // the lower byte of a tile (set in PF_GET_TDATA_LO when fetching sprite pixels)
    uint8_t sTileHi;            // the upper byte of a tile (set in PF_GET_TDATA_HI when fetching sprite pixels)

    PixelFIFO bgFIFO;           // background FIFO
    PixelFIFO spFIFO;           // sprite FIFO

    void TickSpritePixFetcher();
    void TickBackgroundPixFetcher();

    Pixel MixPixels(const Pixel &bg, const Pixel &sp);
public:
    PixelFetcher(PPU *ppu);
    ~PixelFetcher();

    void InitFetcher();
    void TickFetcher();

    bool FetchingWindowPixels();

    bool Ready();               // true when there's a pixel available ready for drawing
    Pixel FetchPixel();
};

#endif