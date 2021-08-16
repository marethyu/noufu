#ifndef _GAMEBOY_WINDOWS_H_
#define _GAMEBOY_WINDOWS_H_

#include <string>

#include <Windows.h>

#ifdef USE_SDL
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#endif

#include "Emulator.h"

class GameBoyWindows
{
private:
    std::shared_ptr<Logger> m_Logger;
    std::unique_ptr<Emulator> m_Emulator;

#ifdef USE_SDL
    SDL_Window *m_Window;
    SDL_Renderer *m_Renderer;
    SDL_Texture *m_Texture;
#else
    BITMAPINFO info;
#endif
    int cnt; // incremented by one for each screenshot

    void LogSystemInfo();
public:
    GameBoyWindows();
    ~GameBoyWindows();

    void Initialize();
    void LoadROM(const std::string &rom_file);
    void ReloadROM();
    void StopEmulation();
    void CaptureScreen();

#ifdef USE_SDL
    bool Create(HWND hWnd);
#endif
    void Update();
#ifndef USE_SDL
    void RenderGraphics(HWND hWnd);
#else
    void RenderGraphics();
#endif
    void HandleKeyDown(WPARAM wParam);
    void HandleKeyUp(WPARAM wParam);
    void CleanUp();
    void Destroy();
};

#endif