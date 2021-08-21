#ifndef _PIXEL_FIFO_H_
#define _PIXEL_FIFO_H_

#include <cstdint>

#define MAX_SIZE 8

struct Pixel
{
    uint8_t colour;   // "final" colour number
    bool transparent; // if the colour number (different from `colour`) is zero
    bool bgEnable;    // for window or background pixels only
    bool bgPriority;  // for sprites only
};

class PixelFIFO
{
private:
    Pixel buffer[MAX_SIZE];

    int head; // front
    int tail; // back
    int size; // current size
public:
    PixelFIFO();
    ~PixelFIFO();

    void Push(const Pixel &pix);
    void Clear();
    bool IsEmpty();
    Pixel Pop();
    int Size();

    Pixel Get(int index);
    void Modify(int index, const Pixel &pix); // call PixelFIFO::Exists first before calling this method
};

#endif