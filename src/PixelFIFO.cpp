#include "ppu/PixelFIFO.h"

PixelFIFO::PixelFIFO()
{
    
}

PixelFIFO::~PixelFIFO()
{
    
}

void PixelFIFO::Push(const Pixel &pix)
{
    buffer[head] = pix;
    head = (head + 1) % MAX_SIZE;
    size++;
}

void PixelFIFO::Clear()
{
    head = 0;
    tail = 0;
    size = 0;
}

bool PixelFIFO::IsEmpty()
{
    return size == 0;
}

Pixel PixelFIFO::Pop()
{
    Pixel pix = buffer[tail];
    tail = (tail + 1) % MAX_SIZE;
    size--;
    return pix;
}

int PixelFIFO::Size()
{
    return size;
}

Pixel PixelFIFO::Get(int index)
{
    return buffer[(tail + index) % MAX_SIZE];
}

void PixelFIFO::Modify(int index, const Pixel &pix)
{
    buffer[(tail + index) % MAX_SIZE] = pix;
}