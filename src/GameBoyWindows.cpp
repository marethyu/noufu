#include "Constants.h"
#include "GameBoyWindows.h"

static void MyMessageBox(Severity severity, const char *message)
{
    MessageBox(NULL,
               TEXT(message),
               TEXT(severity_str[severity].c_str()),
               MB_OK | (severity == LOG_WARN_POPUP ? MB_ICONWARNING :
                                                     MB_ICONERROR));
}

void GameBoyWindows::LogSystemInfo()
{
    m_Logger->DoLog(LOG_INFO, "GameBoyWindows::LogSystemInfo", "Operating system: Windows");
#ifdef USE_SDL
    m_Logger->DoLog(LOG_INFO, "GameBoyWindows::LogSystemInfo", "Renderer: SDL");
#else
    m_Logger->DoLog(LOG_INFO, "GameBoyWindows::LogSystemInfo", "Renderer: GDI");
#endif
}

GameBoyWindows::GameBoyWindows()
{
    m_Logger = std::make_shared<Logger>("emulation_log.txt");
    m_Logger->SetDoMessageBox(MyMessageBox);

    m_Emulator = std::make_unique<Emulator>(m_Logger);

#ifndef USE_SDL
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = SCREEN_WIDTH;
    info.bmiHeader.biHeight = SCREEN_HEIGHT;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    info.bmiHeader.biSizeImage = 0;
    info.bmiHeader.biXPelsPerMeter = 0;
    info.bmiHeader.biYPelsPerMeter = 0;
    info.bmiHeader.biClrUsed = 0;
    info.bmiHeader.biClrImportant = 0;
#endif
}

GameBoyWindows::~GameBoyWindows()
{
    
}

void GameBoyWindows::Initialize()
{
    GameBoyWindows::LogSystemInfo();
}

void GameBoyWindows::LoadROM(const std::string &rom_file)
{
    m_Emulator->InitComponents();
    m_Emulator->m_MemControl->LoadROM(rom_file);
}

void GameBoyWindows::ReloadROM()
{
    m_Emulator->InitComponents();
    m_Emulator->m_MemControl->ReloadROM();
}

#ifdef USE_SDL
bool GameBoyWindows::Create(HWND hWnd)
{
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
    {
        m_Logger->DoLog(LOG_ERROR, "GameBoyWindows::Create", "[SDL Error] Couldn't initialize SDL: {}", SDL_GetError());
        return false;
    }

    m_Window = SDL_CreateWindowFrom(hWnd);
    if (m_Window == nullptr)
    {
        m_Logger->DoLog(LOG_ERROR, "GameBoyWindows::Create", "[SDL Error] Couldn't create window: {}", SDL_GetError());
        return false;
    }

    m_Renderer = SDL_CreateRenderer(m_Window, -1, SDL_RENDERER_ACCELERATED);
    if (m_Renderer == nullptr)
    {
        m_Logger->DoLog(LOG_ERROR, "GameBoyWindows::Create", "[SDL Error] Couldn't create renderer: {}", SDL_GetError());
        return false;
    }

    m_Texture = SDL_CreateTexture(m_Renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);
    if (m_Texture == nullptr)
    {
        m_Logger->DoLog(LOG_ERROR, "GameBoyWindows::Create", "[SDL Error] Couldn't create texture: {}", SDL_GetError());
        return false;
    }

    return true;
}

void GameBoyWindows::FixSize()
{
    SDL_SetWindowSize(m_Window, SCREEN_WIDTH * SCREEN_SCALE_FACTOR, SCREEN_HEIGHT * SCREEN_SCALE_FACTOR);
}
#endif

void GameBoyWindows::Update()
{
    m_Emulator->Update();
}

#ifndef USE_SDL
void GameBoyWindows::RenderGraphics(HWND hWnd)
{
    RECT rcClient;
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);

    GetClientRect(hWnd, &rcClient);

    int width = rcClient.right - rcClient.left;
    int height = rcClient.bottom - rcClient.top;

    StretchDIBits(hdc,
                  0,
                  height,
                  width,
                  -height,
                  0,
                  0,
                  SCREEN_WIDTH,
                  SCREEN_HEIGHT,
                  m_Emulator->m_GPU->m_Pixels.data(),
                  &info,
                  DIB_RGB_COLORS,
                  SRCCOPY);

    EndPaint(hWnd, &ps);
}
#else
void GameBoyWindows::RenderGraphics()
{
    SDL_SetRenderDrawColor(m_Renderer, 0, 0, 0, 255);
    SDL_RenderClear(m_Renderer);
    SDL_UpdateTexture(m_Texture, nullptr, m_Emulator->m_GPU->m_Pixels.data(), SCREEN_WIDTH * 4);
    SDL_RenderCopy(m_Renderer, m_Texture, nullptr, nullptr);
    SDL_RenderPresent(m_Renderer);
}
#endif

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
    m_Logger->DoLog(LOG_INFO, "GameBoyWindows::CleanUp", "Cleaning up resources...");

#ifdef USE_SDL
    SDL_DestroyTexture(m_Texture);
    m_Texture = nullptr;

    SDL_DestroyRenderer(m_Renderer);
    m_Renderer = nullptr;

    SDL_DestroyWindow(m_Window);
    m_Window = nullptr;

    SDL_Quit();
#endif
    return;
}

void GameBoyWindows::Destroy()
{
    PostQuitMessage(0);
}