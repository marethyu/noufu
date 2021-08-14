#include "Constants.h"
#include "GameBoyWindows.h"
#include "Util.h"

#include "loguru.hpp"

GameBoyWindows::GameBoyWindows()
{
    m_Emulator = std::make_unique<Emulator>();
}

GameBoyWindows::~GameBoyWindows()
{
    
}

void GameBoyWindows::Initialize()
{
    m_Emulator->InitComponents();
}

void GameBoyWindows::LoadROM(const std::string& rom_file)
{
    m_Emulator->InitComponents();
    m_Emulator->m_MemControl->LoadROM(rom_file);
    LOG_F(INFO, "Loaded %s", rom_file.c_str());
}

void GameBoyWindows::ReloadROM()
{
    m_Emulator->InitComponents();
    m_Emulator->m_MemControl->ReloadROM();
    LOG_F(INFO, "Current ROM reloaded");
}

void GameBoyWindows::Create(HWND hWnd)
{
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
    {
        PopMessageBox(TEXT("SDL Error"), MB_OK | MB_ICONERROR, "Couldn't initialize SDL: %s", SDL_GetError());
        std::exit(1);
    }

    m_Window = SDL_CreateWindowFrom(hWnd);
    if (m_Window == nullptr)
    {
        PopMessageBox(TEXT("SDL Error"), MB_OK | MB_ICONERROR, "Couldn't create window: %s", SDL_GetError());
        std::exit(1);
    }

    m_Renderer = SDL_CreateRenderer(m_Window, -1, SDL_RENDERER_ACCELERATED);
    if (m_Renderer == nullptr)
    {
        PopMessageBox(TEXT("SDL Error"), MB_OK | MB_ICONERROR, "Couldn't create renderer: %s", SDL_GetError());
        std::exit(1);
    }

    m_Texture = SDL_CreateTexture(m_Renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);
    if (m_Texture == nullptr)
    {
        PopMessageBox(TEXT("SDL Error"), MB_OK | MB_ICONERROR, "Couldn't create texture: %s", SDL_GetError());
        std::exit(1);
    }
}

void GameBoyWindows::FixSize()
{
    SDL_SetWindowSize(m_Window, SCREEN_WIDTH * SCREEN_SCALE_FACTOR, SCREEN_HEIGHT * SCREEN_SCALE_FACTOR);
}

void GameBoyWindows::Update()
{
    m_Emulator->Update();
}

void GameBoyWindows::RenderGraphics()
{
    SDL_SetRenderDrawColor(m_Renderer, 0, 0, 0, 255);
    SDL_RenderClear(m_Renderer);
    SDL_UpdateTexture(m_Texture, nullptr, m_Emulator->m_GPU->m_Pixels.data(), SCREEN_WIDTH * 4);
    SDL_RenderCopy(m_Renderer, m_Texture, nullptr, nullptr);
    SDL_RenderPresent(m_Renderer);
}

void GameBoyWindows::HandleKeyDown(WPARAM wParam)
{
    switch (wParam)
    {
    case 0x41: // a key
        m_Emulator->m_JoyPad->PressButton(JOYPAD_A);
        break;
    case 0x42: // b key
        m_Emulator->m_JoyPad->PressButton(JOYPAD_B);
        break;
    case VK_RETURN:
        m_Emulator->m_JoyPad->PressButton(JOYPAD_START);
        break;
    case VK_SPACE:
        m_Emulator->m_JoyPad->PressButton(JOYPAD_SELECT);
        break;
    case VK_RIGHT:
        m_Emulator->m_JoyPad->PressButton(JOYPAD_RIGHT);
        break;
    case VK_LEFT:
        m_Emulator->m_JoyPad->PressButton(JOYPAD_LEFT);
        break;
    case VK_UP:
        m_Emulator->m_JoyPad->PressButton(JOYPAD_UP);
        break;
    case VK_DOWN:
        m_Emulator->m_JoyPad->PressButton(JOYPAD_DOWN);
        break;
    }
}

void GameBoyWindows::HandleKeyUp(WPARAM wParam)
{
    switch (wParam)
    {
    case 0x41: // a key
        m_Emulator->m_JoyPad->ReleaseButton(JOYPAD_A);
        break;
    case 0x42: // b key
        m_Emulator->m_JoyPad->ReleaseButton(JOYPAD_B);
        break;
    case VK_RETURN:
        m_Emulator->m_JoyPad->ReleaseButton(JOYPAD_START);
        break;
    case VK_SPACE:
        m_Emulator->m_JoyPad->ReleaseButton(JOYPAD_SELECT);
        break;
    case VK_RIGHT:
        m_Emulator->m_JoyPad->ReleaseButton(JOYPAD_RIGHT);
        break;
    case VK_LEFT:
        m_Emulator->m_JoyPad->ReleaseButton(JOYPAD_LEFT);
        break;
    case VK_UP:
        m_Emulator->m_JoyPad->ReleaseButton(JOYPAD_UP);
        break;
    case VK_DOWN:
        m_Emulator->m_JoyPad->ReleaseButton(JOYPAD_DOWN);
        break;
    }
}

void GameBoyWindows::CleanUp()
{
    SDL_DestroyTexture(m_Texture);
    m_Texture = nullptr;

    SDL_DestroyRenderer(m_Renderer);
    m_Renderer = nullptr;

    SDL_DestroyWindow(m_Window);
    m_Window = nullptr;

    SDL_Quit();
}

void GameBoyWindows::Destroy()
{
    PostQuitMessage(0);
}