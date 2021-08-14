#ifndef _GAMEBOY_WINDOWS_H_
#define _GAMEBOY_WINDOWS_H_

#include <string>

#include <Windows.h>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#include "Emulator.h"

class GameBoyWindows
{
private:
    std::unique_ptr<Emulator> m_Emulator;

    SDL_Window *m_Window;
    SDL_Renderer *m_Renderer;
    SDL_Texture *m_Texture;
public:
    GameBoyWindows();
    ~GameBoyWindows();

    void Initialize();
    void LoadROM(const std::string& rom_file);
    void ReloadROM();

    void Create(HWND);
    void FixSize();
    void Update();
    void RenderGraphics();
    void HandleKeyDown(WPARAM wParam);
    void HandleKeyUp(WPARAM wParam);
    void CleanUp();
    void Destroy();
};

#endif