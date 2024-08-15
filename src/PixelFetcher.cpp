#include "BitMagic.h"
#include "Emulator.h"

#include "ppu/PixelFetcher.h"

enum
{
    PF_GET_TILE_NO = 0,
    PF_GET_TDATA_LO,
    PF_GET_TDATA_HI,
    PF_PUSH
};

// https://hacktixme.ga/GBEDG/ppu/#sprite-fetching
void PixelFetcher::TickSpritePixFetcher()
{
    if (++spf_ticks % 2 != 0) return;

    switch (pf_state2)
    {
    case PF_GET_TILE_NO:
    {
        sTileNo = cur_sprite.tileIndex;
        pf_state2 = PF_GET_TDATA_LO;
        break;
    }
    case PF_GET_TDATA_LO:
    {
        uint16_t tileMap = 0x8000;
        uint16_t offset1 = sTileNo * 16;
        uint16_t offset2 = 0;

        if (GET_BIT(cur_sprite.attr, 6))
        {
            offset2 = (m_PPU->bSprite8x16() ? 30 : 14) - 2 * (m_PPU->LY - (cur_sprite.y - 16));
        }
        else
        {
            offset2 = 2 * (m_PPU->LY - (cur_sprite.y - 16));
        }

        addr = tileMap + offset1 + offset2;
        sTileLo = m_PPU->m_Emulator->m_MemControl->ReadByte(addr);

        pf_state2 = PF_GET_TDATA_HI;
        break;
    }
    case PF_GET_TDATA_HI:
    {
        sTileHi = m_PPU->m_Emulator->m_MemControl->ReadByte(addr + 1);
        pf_state2 = PF_PUSH;
        break;
    }
    case PF_PUSH:
    {
        for(int bit = 7, i = 0; bit >= 0; bit--, i++)
        {
            if ((cur_sprite.x + i) < 8)
            {
                continue;
            }

            int effectiveBit = GET_BIT(cur_sprite.attr, 5) ? 7 - bit : bit;
            uint8_t colourNum = GET_2BITS(sTileHi, sTileLo, effectiveBit, effectiveBit);
            Pixel pix{m_PPU->GetColour(colourNum, GET_BIT(cur_sprite.attr, 4) ? m_PPU->OBP1 : m_PPU->OBP0), colourNum == 0, false, GET_BIT(cur_sprite.attr, 7)};

            if(i >= spFIFO.Size())
            {
                spFIFO.Push(pix);
            }
            else
            {
                if(spFIFO[i].transparent)
                {
                    spFIFO[i] = pix;
                }
            }
        }

        fetchingSprites = false;
        pf_state1 = PF_GET_TILE_NO;
        break;
    }
    }
}

void PixelFetcher::TickBackgroundPixFetcher()
{
    if (++bgf_ticks % 2 != 0) return;

    switch (pf_state1)
    {
    case PF_GET_TILE_NO:
    {
        uint16_t tileMap;
        uint16_t xOffset;
        uint16_t yOffset;

        if (fetchingWindow)
        {
            tileMap = m_PPU->bWindowTileMap() ? 0x9C00 : 0x9800;
            xOffset = fetcherX;
            yOffset = (m_PPU->WLY / 8) * 32;
        }
        else
        {
            tileMap = m_PPU->bBackgroundTileMap() ? 0x9C00 : 0x9800;
            xOffset = (fetcherX + (m_PPU->SCX / 8)) % 32;
            yOffset = (((m_PPU->LY + m_PPU->SCY) % 256) / 8) * 32;
        }

        addr = tileMap + ((xOffset + yOffset) & 0x3FF);
        tileNo = m_PPU->m_Emulator->m_MemControl->ReadByte(addr);

        pf_state1 = PF_GET_TDATA_LO;
        break;
    }
    case PF_GET_TDATA_LO:
    {
        bool useSigned = !m_PPU->bBackgroundAndWindowTileData();
        uint16_t tileData = useSigned ? 0x9000 : 0x8000; // the "$8800 method" uses $9000 as its base pointer and uses a signed addressing
        uint16_t offset1 = (useSigned ? int8_t(tileNo) : tileNo) * 16;
        uint16_t offset2 = fetchingWindow ? (m_PPU->WLY % 8) * 2 : ((m_PPU->LY + m_PPU->SCY) % 8) * 2;

        addr = tileData + offset1 + offset2;
        tileLo = m_PPU->m_Emulator->m_MemControl->ReadByte(addr);

        pf_state1 = PF_GET_TDATA_HI;
        break;
    }
    case PF_GET_TDATA_HI:
    {
        tileHi = m_PPU->m_Emulator->m_MemControl->ReadByte(addr + 1);
        pf_state1 = PF_PUSH;
        break;
    }
    case PF_PUSH:
    {
        if (bgFIFO.IsEmpty())
        {
            for (int bit = 7; bit >= 0; --bit)
            {
                uint8_t colourNum = GET_2BITS(tileHi, tileLo, bit, bit);
                bgFIFO.Push(Pixel{m_PPU->GetColour(colourNum, m_PPU->BGP), colourNum == 0, m_PPU->bBGAndWindowEnabled(), false});
            }

            fetcherX++;
            pf_state1 = PF_GET_TILE_NO;
        }
        else
        {
            pf_state1 = PF_PUSH;
        }
        break;
    }
    }
}

Pixel PixelFetcher::MixPixels(const Pixel &bg, const Pixel &sp)
{
    if (!sp.transparent)
    {
        if (sp.bgPriority && !bg.transparent)
        {
            return bg;
        }

        return sp;
    }

    return bg;
}

PixelFetcher::PixelFetcher(PPU *ppu)
{
    m_PPU = ppu;
}

PixelFetcher::~PixelFetcher()
{
    
}

void PixelFetcher::InitFetcher()
{
    fetcherX = 0;
    m_PPU->LX = -(m_PPU->SCX % 8); // (SCX % 8) redundant background pixels will be discarded later...
    pf_state1 = PF_GET_TILE_NO;
    bgf_ticks = 0;
    fetchingSprites = false;
    fetchingWindow = false;
    bgFIFO.Clear();
    spFIFO.Clear();
}

void PixelFetcher::TickFetcher()
{
    if (!fetchingSprites && m_PPU->bSpriteEnabled() && m_PPU->oamBuffer.size() > 0)
    {
        Sprite s = m_PPU->oamBuffer.front();

        if ((s.x - 8) <= m_PPU->LX)
        {
            cur_sprite = s;
            pf_state2 = PF_GET_TILE_NO;
            spf_ticks = 0;
            fetchingSprites = true;
            m_PPU->oamBuffer.pop_front();
        }
    }

    if (fetchingSprites)
    {
        PixelFetcher::TickSpritePixFetcher();
        return;
    }

    if (!fetchingWindow && m_PPU->bWindowEnabled() && m_PPU->wyTrigger && m_PPU->LX >= (m_PPU->WX - 7))
    {
        fetcherX = 0;
        pf_state1 = PF_GET_TILE_NO;
        fetchingWindow = true;
        bgFIFO.Clear();
    }

    PixelFetcher::TickBackgroundPixFetcher();
}

bool PixelFetcher::FetchingWindowPixels()
{
    return fetchingWindow;
}

bool PixelFetcher::Ready()
{
    bool ret = !bgFIFO.IsEmpty() && !fetchingSprites;

    if (ret && m_PPU->LX < 0)
    {
        bgFIFO.Pop();
        m_PPU->LX++;
        ret = false;
    }

    return ret;
}

Pixel PixelFetcher::FetchPixel()
{
    Pixel pix = bgFIFO.Pop();

    if (!pix.bgEnable)
    {
        if (!spFIFO.IsEmpty())
        {
            pix = spFIFO.Pop();
        }
        else
        {
            pix.colour = 0;
        }
    }
    else
    {
        if (!spFIFO.IsEmpty())
        {
            pix = PixelFetcher::MixPixels(pix, spFIFO.Pop());
        }
    }

    return pix;
}