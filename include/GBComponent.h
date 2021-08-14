#ifndef _GBCOMPONENT_H_
#define _GBCOMPONENT_H_

class Emulator;

class GBComponent
{
public:
    virtual void Init() = 0;
    virtual void Reset() = 0;
    virtual void Debug_PrintStatus() = 0;
protected:
    Emulator *m_Emulator;
};

#endif