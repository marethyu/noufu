#include <algorithm>
#include <iostream>
#include <sstream>

#include "BitMagic.h"
#include "Emulator.h"

#include "ppu/PPU.h"
#include "ppu/PixelFetcher.h"

#define FMT_HEADER_ONLY
#include "3rdparty/fmt/format.h"

enum
{
    MODE_HBLANK = 0,
    MODE_VBLANK,
    MODE_OAM_SEARCH,
    MODE_PIXEL_TRANSFER
};

bool PPU::bLCDEnabled()
{
    return GET_BIT(LCDC, 7);
}

bool PPU::bWindowTileMap()
{
    return GET_BIT(LCDC, 6);
}

bool PPU::bWindowEnabled()
{
    return GET_BIT(LCDC, 5);
}

bool PPU::bBackgroundAndWindowTileData()
{
    return GET_BIT(LCDC, 4);
}

bool PPU::bBackgroundTileMap()
{
    return GET_BIT(LCDC, 3);
}

bool PPU::bSprite8x16()
{
    return GET_BIT(LCDC, 2);
}

bool PPU::bSpriteEnabled()
{
    return GET_BIT(LCDC, 1);
}

bool PPU::bBGAndWindowEnabled()
{
    return GET_BIT(LCDC, 0);
}

bool PPU::bCoincidenceStatInterrupt()
{
    return GET_BIT(STAT, 6);
}

bool PPU::bOAMStatInterrupt()
{
    return GET_BIT(STAT, 5);
}

bool PPU::bVBlankStatInterrupt()
{
    return GET_BIT(STAT, 4);
}

bool PPU::bHBlankStatInterrupt()
{
    return GET_BIT(STAT, 3);
}

void PPU::LYCompare()
{
    if (LY == LYC)
    {
        SET_BIT(STAT, 2);

        if (PPU::bCoincidenceStatInterrupt())
        {
            m_Emulator->m_IntManager->RequestInterrupt(INT_LCD_STAT);
        }
    }
    else
    {
        RES_BIT(STAT, 2);
    }
}

void PPU::SwitchMode(int mode)
{
    bool stat_irq = false;

    switch (mode)
    {
    case MODE_HBLANK:
    {
        PPU::SetMode(MODE_HBLANK);
        stat_irq = bHBlankStatInterrupt();
        break;
    }
    case MODE_OAM_SEARCH:
    {
        PPU::SetMode(MODE_OAM_SEARCH);
        stat_irq = bOAMStatInterrupt();
        oamBuffer.clear();
        break;
    }
    case MODE_PIXEL_TRANSFER:
    {
        PPU::SetMode(MODE_PIXEL_TRANSFER);
        pixFetcher->InitFetcher();
        break;
    }
    case MODE_VBLANK:
    {
        PPU::SetMode(MODE_VBLANK);
        stat_irq = bVBlankStatInterrupt();
        m_Emulator->m_IntManager->RequestInterrupt(INT_VBLANK);

        if (m_Emulator->bgPreview)
        {
            PPU::DrawWideBackground();
        }
        break;
    }
    }

    if (stat_irq)
    {
        m_Emulator->m_IntManager->RequestInterrupt(INT_LCD_STAT);
    }
}

void PPU::SetMode(int mode)
{
    STAT &= 0b11111100;
    STAT |= mode;
}

int PPU::GetMode()
{
    return STAT & 0b00000011;
}

// https://hacktixme.ga/GBEDG/ppu/#oam-scan-mode-2
void PPU::DoOAMSearch()
{
    if (!wyTrigger && nDots == 0)
    {
        wyTrigger = LY == WY;
    }

    if (nDots % 2 == 0)
    {
        uint16_t address = 0xFE00 + (nDots / 2) * 4;

        uint8_t YPos = m_Emulator->m_MemControl->ReadByte(address);
        uint8_t XPos = m_Emulator->m_MemControl->ReadByte(address + 1);
        uint8_t tileIndex = m_Emulator->m_MemControl->ReadByte(address + 2);
        uint8_t attributes = m_Emulator->m_MemControl->ReadByte(address + 3);

        uint8_t spriteHeight = PPU::bSprite8x16() ? 16 : 8;

        if (XPos > 0 && (LY + 16) >= YPos && (LY + 16) < (YPos + spriteHeight) && oamBuffer.size() < 10)
        {
            // https://gbdev.io/pandocs/OAM.html#byte-2---tile-index
            if (PPU::bSprite8x16())
            {
                if ((LY + 16) < (YPos - 8))
                {
                    tileIndex |= 0x01;
                }
                else
                {
                    tileIndex &= 0xFE;
                }
            }

            oamBuffer.push_back(Sprite{XPos, YPos, tileIndex, attributes, address});
        }
    }

    if (++nDots == 80)
    {
        std::sort(oamBuffer.begin(), oamBuffer.end());
        PPU::SwitchMode(MODE_PIXEL_TRANSFER);
    }
}

void PPU::DoPixelTransfer()
{
    nDots++;
    pixFetcher->TickFetcher();

    if (pixFetcher->Ready())
    {
        Pixel pix = pixFetcher->FetchPixel();
        PPU::DrawPixel(LX, LY, gb_colours[pix.colour]);

        if (++LX == 160)
        {
            PPU::SwitchMode(MODE_HBLANK);
        }
    }
}

void PPU::DoHBlank()
{
    if (++nDots == 456)
    {
        nDots = 0;
        LY++;

        if (pixFetcher->FetchingWindowPixels())
        {
            WLY++;
        }

        if (LY == 144)
        {
            PPU::SwitchMode(MODE_VBLANK);
        }
        else
        {
            PPU::SwitchMode(MODE_OAM_SEARCH);
        }

        PPU::LYCompare();
    }
}

void PPU::DoVBlank()
{
    wyTrigger = false;

    if (++nDots == 456)
    {
        nDots = 0;
        LY++;

        if (LY > 153)
        {
            LY = 0;
            WLY = 0;
            PPU::SwitchMode(MODE_OAM_SEARCH);
        }

        PPU::LYCompare();
    }
}

void PPU::DrawPixel(int x, int y, const rgb_tuple &colour)
{
    int offset;

    if (m_Emulator->bgPreview)
    {
        offset = (y + 24) * m_Emulator->dispWidth * 4 + (x + 24) * 4;
    }
    else
    {
        offset = y * m_Emulator->dispWidth * 4 + x * 4;
    }

    m_Pixels[offset    ] = colour.b;
    m_Pixels[offset + 1] = colour.g;
    m_Pixels[offset + 2] = colour.r;
    m_Pixels[offset + 3] = 255;
}

void PPU::DrawWideBackground()
{
    uint16_t address = PPU::bBackgroundTileMap() ? 0x9C00 : 0x9800;
    bool useSigned = !PPU::bBackgroundAndWindowTileData();
    uint16_t tileData = useSigned ? 0x9000 : 0x8000;

    for (int yPos = 0; yPos < 192; yPos += 8)
    {
        for (int xPos = 0; xPos < 208; xPos += 8)
        {
            if ((xPos >= 24 && xPos < 184) && (yPos >= 24 && yPos < 168))
            {
                continue;
            }

            for (int i = 0; i < 8; ++i)
            {
                for (int j = 0; j < 8; ++j)
                {
                    uint16_t newX = xPos + j;
                    uint16_t newY = yPos + i;

                    uint16_t bgX = (SCX - 24) + newX;
                    uint16_t bgY = (SCY - 24) + newY;

                    uint16_t xOffset = ((256 + (bgX % 256)) % 256) / 8;
                    uint16_t yOffset = ((256 + (bgY % 256)) % 256) / 8;

                    uint8_t tileIndex = m_Emulator->m_MemControl->ReadByte(address + xOffset + yOffset * 32);
                    uint16_t tileAddr = tileData + (useSigned ? int8_t(tileIndex) : tileIndex) * 16 + (bgY % 8) * 2;

                    uint8_t tileLo = m_Emulator->m_MemControl->ReadByte(tileAddr);
                    uint8_t tileHi = m_Emulator->m_MemControl->ReadByte(tileAddr + 1);

                    int effBit = 7 - (bgX % 8);

                    rgb_tuple rgb = gb_colours[PPU::GetColour(GET_2BITS(tileHi, tileLo, effBit, effBit), BGP)];

                    int offset = newY * (SCREEN_WIDTH + 48) * 4 + newX * 4;

                    if ((newX == 23 && newY >= 23 && newY <= 168) ||
                        (newX == 184 && newY >= 23 && newY <= 168) ||
                        (newY == 23 && newX >= 23 && newX <= 184) ||
                        (newY == 168 && newX >= 23 && newX <= 184)) // for creating a black rectangular border
                    {
                        m_Pixels[offset    ] = 0;
                        m_Pixels[offset + 1] = 0;
                        m_Pixels[offset + 2] = 0;
                        m_Pixels[offset + 3] = 255;
                    }
                    else
                    {
                        m_Pixels[offset    ] = rgb.b * 3 / 4;
                        m_Pixels[offset + 1] = rgb.g * 3 / 4;
                        m_Pixels[offset + 2] = rgb.r * 3 / 4;
                        m_Pixels[offset + 3] = 255;
                    }
                }
            }
        }
    }
}

void PPU::ClearScreen()
{
    for (int i = 0; i < m_Emulator->dispHeight; ++i)
    {
        for (int j = 0; j < m_Emulator->dispWidth; ++j)
        {
            rgb_tuple rgb = gb_colours[0];
            int offset = i * m_Emulator->dispWidth * 4 + j * 4;

            m_Pixels[offset    ] = rgb.b;
            m_Pixels[offset + 1] = rgb.g;
            m_Pixels[offset + 2] = rgb.r;
            m_Pixels[offset + 3] = 255;
        }
    }
}

uint8_t PPU::GetColour(uint8_t colourNum, uint8_t palette)
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

    return GET_2BITS(palette, palette, hi, lo);
}

void PPU::InitRGBTuple(rgb_tuple &tup, const std::string &colour_info)
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

void PPU::PrintPalette(uint8_t palette)
{
    std::cout << fmt::format("Info for palette ${0:04X}:", palette) << std::endl;
    std::cout << fmt::format("Bit 1-0 - ", GET_2BITS(palette, palette, 1, 0)) << std::endl;
    std::cout << fmt::format("Bit 3-2 - ", GET_2BITS(palette, palette, 3, 2)) << std::endl;
    std::cout << fmt::format("Bit 5-4 - ", GET_2BITS(palette, palette, 5, 4)) << std::endl;
    std::cout << fmt::format("Bit 7-6 - ", GET_2BITS(palette, palette, 7, 6)) << std::endl;
    std::cout << std::endl;
}

PPU::PPU(Emulator *emu)
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
    pixFetcher = std::make_unique<PixelFetcher>(this);
    m_Pixels.resize(m_Emulator->dispWidth * m_Emulator->dispHeight * 4);

    std::fill(m_Pixels.begin(), m_Pixels.end(), 0);

    PPU::InitRGBTuple(gb_colours[0], m_Emulator->m_Config->GetValue("Color0"));
    PPU::InitRGBTuple(gb_colours[1], m_Emulator->m_Config->GetValue("Color1"));
    PPU::InitRGBTuple(gb_colours[2], m_Emulator->m_Config->GetValue("Color2"));
    PPU::InitRGBTuple(gb_colours[3], m_Emulator->m_Config->GetValue("Color3"));
}

PPU::~PPU()
{
    
}

void PPU::Init()
{
    ClearScreen();
    SCX = 0;
    bResetted = false;
    wyTrigger = false;
    WLY = 0;
}

void PPU::Reset()
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

    bResetted = false;
    wyTrigger = false;
    WLY = 0;
}

void PPU::Update(int cycles)
{
    if (!PPU::bLCDEnabled())
    {
        if (!bResetted)
        {
            PPU::SetMode(MODE_HBLANK);
            LY = 0;
            nDots = 0;
            ClearScreen();
            bResetted = true;
        }
        return;
    }
    else
    {
        if (bResetted)
        {
            PPU::SetMode(MODE_OAM_SEARCH);
            bResetted = false;
        }
    }

    for (int i = 0; i < cycles; ++i)
    {
        switch (PPU::GetMode())
        {
        case MODE_OAM_SEARCH:
        {
            DoOAMSearch();
            break;
        }
        case MODE_PIXEL_TRANSFER:
        {
            DoPixelTransfer();
            break;
        }
        case MODE_HBLANK:
        {
            DoHBlank();
            break;
        }
        case MODE_VBLANK:
        {
            DoVBlank();
            break;
        }
        }
    }
}

uint8_t PPU::CPUReadVRAM(uint16_t address)
{
    if (PPU::GetMode() == MODE_PIXEL_TRANSFER)
    {
        return 0xFF;
    }

    return m_Emulator->m_MemControl->ReadByte(address);
}

uint8_t PPU::CPUReadOAM(uint16_t address)
{
    int mode = PPU::GetMode();

    if (mode == MODE_OAM_SEARCH || mode == MODE_PIXEL_TRANSFER)
    {
        return 0xFF;
    }

    return m_Emulator->m_MemControl->ReadByte(address);
}

void PPU::CPUWriteVRAM(uint16_t address, uint8_t data)
{
    if (PPU::GetMode() == MODE_PIXEL_TRANSFER)
    {
        return;
    }

    m_Emulator->m_MemControl->WriteByte(address, data);
}

void PPU::CPUWriteOAM(uint16_t address, uint8_t data)
{
    int mode = PPU::GetMode();

    if (mode == MODE_OAM_SEARCH || mode == MODE_PIXEL_TRANSFER)
    {
        return;
    }

    m_Emulator->m_MemControl->WriteByte(address, data);
}

void PPU::Debug_PrintStatus()
{
    std::cout << "*PPU STATUS*" << std::endl;
    std::cout << fmt::format("LCDC={0:08b}b", LCDC) << std::endl;
    std::cout << fmt::format("LCDEnable={:d} WindowEnable={:d} SpriteEnable={:d} BGAndWindowEnable={:d}",
                             PPU::bLCDEnabled(),
                             PPU::bWindowEnabled(),
                             PPU::bSpriteEnabled(),
                             PPU::bBGAndWindowEnabled()) << std::endl;
    std::cout << fmt::format("STAT={0:08b}b", STAT) << std::endl;
    std::cout << fmt::format("WX={0:d} WY={0:d}", WX, WY) << std::endl;
    std::cout << fmt::format("SCX={0:d} SCY={0:d}", SCX, SCY) << std::endl;
    std::cout << fmt::format("LY={0:d} LYC={0:d}", LY, LYC) << std::endl;
    std::cout << std::endl;

    PPU::PrintPalette(BGP);
    PPU::PrintPalette(OBP0);
    PPU::PrintPalette(OBP1);
}