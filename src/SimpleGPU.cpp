#include <algorithm>
#include <iostream>
#include <sstream>

#include "BitMagic.h"
#include "Emulator.h"
#include "SimpleGPU.h"

#define FMT_HEADER_ONLY
#include "fmt/format.h"

static const int DOTS_PER_SCANLINE = 456;

enum
{
    MODE_HBLANK = 0,
    MODE_VBLANK,
    MODE_OAM_SEARCH,
    MODE_PIXEL_TRANSFER
};

enum
{
    SHADE0 = 0,
    SHADE1,
    SHADE2,
    SHADE3
};

void SimpleGPU::InitRGBTuple(rgb_tuple &tup, const std::string &colour_info)
{
    std::istringstream ss(colour_info);
    std::string token;

    uint8_t *a[3] = {&tup.r, &tup.g, &tup.b};
    int i = 0;

    while(std::getline(ss, token, '.'))
    {
        *a[i++] = std::stoul(token, nullptr, 16);
    }
}

bool SimpleGPU::bLCDEnabled()
{
    return GET_BIT(LCDC, 7);
}

bool SimpleGPU::bWindowEnable()
{
    return GET_BIT(LCDC, 5);
}

bool SimpleGPU::bSpriteEnable()
{
    return GET_BIT(LCDC, 1);
}

bool SimpleGPU::bBGAndWindowEnable()
{
    return GET_BIT(LCDC, 0);
}

bool SimpleGPU::bCoincidenceStatInterrupt()
{
    return GET_BIT(STAT, 6);
}

bool SimpleGPU::bOAMStatInterrupt()
{
    return GET_BIT(STAT, 5);
}

bool SimpleGPU::bVBlankStatInterrupt()
{
    return GET_BIT(STAT, 4);
}

bool SimpleGPU::bHBlankStatInterrupt()
{
    return GET_BIT(STAT, 3);
}

void SimpleGPU::UpdateMode(int mode)
{
    STAT &= 0b11111100;
    STAT |= mode;
}

int SimpleGPU::GetMode()
{
    return STAT & 0b00000011;
}

void SimpleGPU::UpdateLCDStatus()
{
    if (!SimpleGPU::bLCDEnabled())
    {
        m_Counter = DOTS_PER_SCANLINE;
        LY = 0;
        SimpleGPU::UpdateMode(MODE_VBLANK);
        return;
    }

    bool stat_int = false;
    int old_mode = SimpleGPU::GetMode();
    int new_mode;

    if (LY >= 144)
    {
        new_mode = MODE_VBLANK;
        stat_int = SimpleGPU::bVBlankStatInterrupt();
    }
    else
    {
        int dots = DOTS_PER_SCANLINE - m_Counter;

        if (dots <= 80) // mode 2
        {
            new_mode = MODE_OAM_SEARCH;
            stat_int = SimpleGPU::bOAMStatInterrupt();
        }
        else if (dots > 80 && dots <= 360) // mode 3
        {
            new_mode = MODE_PIXEL_TRANSFER;
        }
        else // mode 0
        {
            new_mode = MODE_HBLANK;
            stat_int = SimpleGPU::bHBlankStatInterrupt();
        }
    }

    SimpleGPU::UpdateMode(new_mode);

    if (stat_int && (old_mode != new_mode))
    {
        m_Emulator->m_IntManager->RequestInterrupt(INT_LCD_STAT);
    }

    if (LY == LYC)
    {
        SET_BIT(STAT, 2);

        if (SimpleGPU::bCoincidenceStatInterrupt())
        {
            m_Emulator->m_IntManager->RequestInterrupt(INT_LCD_STAT);
        }
    }
    else
    {
        RES_BIT(STAT, 2);
    }
}

void SimpleGPU::DrawCurrentLine()
{
    if (SimpleGPU::bBGAndWindowEnable())
    {
        SimpleGPU::RenderBackground();
    }

    if (SimpleGPU::bSpriteEnable())
    {
        SimpleGPU::RenderSprites();
    }
}

void SimpleGPU::RenderBackground()
{
    uint16_t wndTileMem = GET_BIT(LCDC, 4) ? 0x8000 : 0x8800;

    bool _signed = wndTileMem == 0x8800;
    bool usingWnd = GET_BIT(LCDC, 5) && WY <= LY; // true if window display is enabled (specified in LCDC) and WndY <= LY

    uint16_t bkgdTileMem = GET_BIT(LCDC, usingWnd ? 6 : 3) ? 0x9C00 : 0x9800;

    // the y-position is used to determine which of 32 (256 / 8) vertical tiles will used (background map y)
    uint8_t yPos = !usingWnd ? SCY + LY : LY - WY; // map to window coordinates if necessary
    uint16_t tileRow = ((uint8_t) (yPos / 8)) * 32; // which of 8 vertical pixels of the current tile is the scanline on?

    // time to draw a scanline which consists of 160 pixels
    for (int pixel = 0; pixel < 160; ++pixel)
    {
        uint8_t xPos = usingWnd && pixel >= WX ? pixel - WX : SCX + pixel;
        uint16_t tileCol = xPos / 8; // which of horizontal pixels of the current tile does xPos fall in?
        uint16_t tileAddr = bkgdTileMem + tileRow + tileCol;
        int16_t tileNum = _signed ? (int8_t) m_Emulator->m_MemControl->ReadByte(tileAddr) : (uint8_t) m_Emulator->m_MemControl->ReadByte(tileAddr);
        uint16_t tileLocation = wndTileMem + (_signed ? (tileNum + 128) * 16 : tileNum * 16);

        // determine the correct row of pixels
        uint8_t line = yPos % 8;
        line *= 2; // each line needs 2 bytes of memory so...

        uint8_t data1 = m_Emulator->m_MemControl->ReadByte(tileLocation + line);
        uint8_t data2 = m_Emulator->m_MemControl->ReadByte(tileLocation + line + 1);

        int bit = 7 - (xPos % 8); // bit position in data1 and data2 (because pixel 0 corresponds to bit 7 and pixel 1 corresponds to bit 6 and so on)
        int colourNum = (GET_BIT(data2, bit) << 1) | GET_BIT(data1, bit);

        SimpleGPU::RenderPixel(LY, pixel, SimpleGPU::GetColour(colourNum, BGP), -1);
    }
}

void SimpleGPU::RenderSprites()
{
    bool use8x16 = GET_BIT(LCDC, 2); // determine the sprite size

    // the sprite layer can display up to 40 sprites
    for (int i = 0; i < 40; ++i)
    {
        int index = i * 4; // each sprite takes 4 bytes of OAM space (0xFE00-0xFE9F)

        uint8_t spriteY = m_Emulator->m_MemControl->ReadByte(0xFE00 + index) - 16;
        uint8_t spriteX = m_Emulator->m_MemControl->ReadByte(0xFE00 + index + 1) - 8;
        uint8_t patternNumber = m_Emulator->m_MemControl->ReadByte(0xFE00 + index + 2);
        uint8_t attributes = m_Emulator->m_MemControl->ReadByte(0xFE00 + index + 3);

        bool xFlip = GET_BIT(attributes, 5);
        bool yFlip = GET_BIT(attributes, 6);

        int height = use8x16 ? 16 : 8;

        // does the scanline intercept this sprite?
        if ((LY >= spriteY) && (LY < (spriteY + height)))
        {
            int line = LY - spriteY;

            if (yFlip)
            {
                line = height - line; // read backwards if y flipping is allowed
            }

            line *= 2; // each line takes 2 bytes of memory

            uint16_t tileLocation = 0x8000 + (patternNumber * 16);

            uint8_t data1 = m_Emulator->m_MemControl->ReadByte(tileLocation + line);
            uint8_t data2 = m_Emulator->m_MemControl->ReadByte(tileLocation + line + 1);

            for (int tilePixel = 7; tilePixel >= 0; tilePixel--)
            {
                int bit = tilePixel;

                if (xFlip)
                {
                    bit = 7 - bit; // read backwards if x flipping is allowed
                }

                int colourNum = (GET_BIT(data2, bit) << 1) | GET_BIT(data1, bit);
                rgb_tuple col = SimpleGPU::GetColour(colourNum, GET_BIT(attributes, 4) ? OBP1 : OBP0);

                // it's transparent for sprites
                if (col.r == gb_colours[SHADE0].r && col.g == gb_colours[SHADE0].g && col.b == gb_colours[SHADE0].b)
                {
                    continue;
                }

                int pixel = spriteX + (7 - tilePixel);

                if ((LY < 0) || (LY > 143) || (pixel < 0) || (pixel > 159))
                {
                    continue;
                }

                SimpleGPU::RenderPixel(LY, pixel, col, attributes);
            }
        }
    }
}

void SimpleGPU::RenderPixel(int row, int col, rgb_tuple colour, int attr)
{
    int offset = row * SCREEN_WIDTH * 4 + col * 4;

    // check if pixel is hidden behind background
    if (attr != -1)
    {
        // if the bit 7 of attributes is set then the screen pixel colour must be SHADE0 before changing colour
        if (GET_BIT(attr, 7) && ((m_Pixels[offset + 2] != gb_colours[SHADE0].r) || (m_Pixels[offset + 1] != gb_colours[SHADE0].g) || (m_Pixels[offset] != gb_colours[SHADE0].b)))
        {
            return;
        }
    }

    m_Pixels[offset]     = colour.b;
    m_Pixels[offset + 1] = colour.g;
    m_Pixels[offset + 2] = colour.r;
    m_Pixels[offset + 3] = 255;
}

rgb_tuple SimpleGPU::GetColour(int colourNum, uint8_t palette)
{
    int hi, lo;

    switch (colourNum)
    {
    case 0:
        hi = 1;
        lo = 0;
        break;
    case 1:
        hi = 3;
        lo = 2;
        break;
    case 2:
        hi = 5;
        lo = 4;
        break;
    case 3:
        hi = 7;
        lo = 6;
        break;
    }

    return gb_colours[SimpleGPU::GetValue(palette, hi, lo)];
}

int SimpleGPU::GetValue(uint8_t palette, int hi, int lo)
{
    return (GET_BIT(palette, hi) << 1) | GET_BIT(palette, lo);
}

void SimpleGPU::PrintPalette(uint8_t palette)
{
    std::cout << fmt::format("Info for palette ${0:04X}:", palette) << std::endl;
    std::cout << fmt::format("Bit 1-0 - ", SimpleGPU::GetValue(palette, 1, 0)) << std::endl;
    std::cout << fmt::format("Bit 3-2 - ", SimpleGPU::GetValue(palette, 3, 2)) << std::endl;
    std::cout << fmt::format("Bit 5-4 - ", SimpleGPU::GetValue(palette, 5, 4)) << std::endl;
    std::cout << fmt::format("Bit 7-6 - ", SimpleGPU::GetValue(palette, 7, 6)) << std::endl;
    std::cout << std::endl;
}

SimpleGPU::SimpleGPU(Emulator *emu)
  : LCDC(emu->m_MemControl->m_IO[0x40]),
    STAT(emu->m_MemControl->m_IO[0x41]),
    SCY(emu->m_MemControl->m_IO[0x42]),
    SCX(emu->m_MemControl->m_IO[0x43]),
    LY(emu->m_MemControl->m_IO[0x44]),
    LYC(emu->m_MemControl->m_IO[0x45]),
    BGP(emu->m_MemControl->m_IO[0x47]),
    OBP0(emu->m_MemControl->m_IO[0x48]),
    OBP1(emu->m_MemControl->m_IO[0x49]),
    WY(emu->m_MemControl->m_IO[0x4A]),
    WX(emu->m_MemControl->m_IO[0x4B])
{
    m_Emulator = emu;
    InitRGBTuple(gb_colours[0], m_Emulator->m_Config->GetValue("Color0"));
    InitRGBTuple(gb_colours[1], m_Emulator->m_Config->GetValue("Color1"));
    InitRGBTuple(gb_colours[2], m_Emulator->m_Config->GetValue("Color2"));
    InitRGBTuple(gb_colours[3], m_Emulator->m_Config->GetValue("Color3"));
    std::fill(m_Pixels.begin(), m_Pixels.end(), 0);
}

SimpleGPU::~SimpleGPU()
{
    
}

void SimpleGPU::Init()
{
    LY = 0x00;
    std::fill(m_Pixels.begin(), m_Pixels.end(), 0);
}

void SimpleGPU::Reset()
{
    LCDC = 0x91;
    STAT = 0x85;
    SCY = 0x00;
    SCX = 0x00;
    LY = 0x00;
    LYC = 0x00;
    BGP = 0xFC;
    OBP0 = 0xFF;
    OBP1 = 0xFF;
    WY = 0x00;
    WX = 0x00;
}

/* TODO when running boot run, the emulator takes a while to exit this loop

LD A,($FF00+$44)    ; $0064  wait for screen frame
CP $90      ; $0066
JR NZ, Addr_0064    ; $0068
DEC C           ; $006a
JR NZ, Addr_0064    ; $006b

figure out why
*/
void SimpleGPU::Update(int cycles)
{
    SimpleGPU::UpdateLCDStatus();

    if (!SimpleGPU::bLCDEnabled()) return;

    m_Counter -= cycles;

    if (m_Counter <= 0)
    {
        m_Counter = DOTS_PER_SCANLINE;
        LY++;

        if (LY == 144)
        {
            m_Emulator->m_IntManager->RequestInterrupt(INT_VBLANK);
        }
        else if (LY == 154)
        {
            LY = 0;
        }
        else if (LY < 144)
        {
            SimpleGPU::DrawCurrentLine();
        }
    }
}

uint8_t SimpleGPU::CPUReadVRAM(uint16_t address)
{
    if (SimpleGPU::GetMode() == MODE_PIXEL_TRANSFER)
    {
        return 0xFF;
    }

    return m_Emulator->m_MemControl->ReadByte(address);
}

uint8_t SimpleGPU::CPUReadOAM(uint16_t address)
{
    int mode = SimpleGPU::GetMode();

    if (mode == MODE_OAM_SEARCH || mode == MODE_PIXEL_TRANSFER)
    {
        return 0xFF;
    }

    return m_Emulator->m_MemControl->ReadByte(address);
}

void SimpleGPU::CPUWriteVRAM(uint16_t address, uint8_t data)
{
    if (SimpleGPU::GetMode() == MODE_PIXEL_TRANSFER)
    {
        return;
    }

    m_Emulator->m_MemControl->WriteByte(address, data);
}

void SimpleGPU::CPUWriteOAM(uint16_t address, uint8_t data)
{
    int mode = SimpleGPU::GetMode();

    if (mode == MODE_OAM_SEARCH || mode == MODE_PIXEL_TRANSFER)
    {
        return;
    }

    m_Emulator->m_MemControl->WriteByte(address, data);
}

void SimpleGPU::Debug_PrintStatus()
{
    std::cout << "*GPU STATUS*" << std::endl;
    std::cout << fmt::format("LCDC={0:08b}b", LCDC) << std::endl;
    std::cout << fmt::format("LCDEnable={:d} WindowEnable={:d} SpriteEnable={:d} BGAndWindowEnable={:d}",
                             SimpleGPU::bLCDEnabled(),
                             SimpleGPU::bWindowEnable(),
                             SimpleGPU::bSpriteEnable(),
                             SimpleGPU::bBGAndWindowEnable()) << std::endl;
    std::cout << fmt::format("STAT={0:08b}b", STAT) << std::endl;
    std::cout << fmt::format("WX={0:d} WY={0:d}", WX, WY) << std::endl;
    std::cout << fmt::format("SCX={0:d} SCY={0:d}", SCX, SCY) << std::endl;
    std::cout << fmt::format("LY={0:d} LYC={0:d}", LY, LYC) << std::endl;
    std::cout << std::endl;

    SimpleGPU::PrintPalette(BGP);
    SimpleGPU::PrintPalette(OBP0);
    SimpleGPU::PrintPalette(OBP1);
}