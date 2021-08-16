#include <Shlwapi.h>

#include "Constants.h"
#include "GameBoyWindows.h"

#define FMT_HEADER_ONLY
#include "fmt/format.h"

static void MyMessageBox(Severity severity, const char *message)
{
    MessageBox(NULL,
               TEXT(message),
               TEXT(severity_str[severity].c_str()),
               MB_OK | (severity == LOG_WARN_POPUP ? MB_ICONWARNING :
                                                     MB_ICONERROR));
}

// Save the pixel data to a bmp file
// Adapted from https://www.technical-recipes.com/2011/creating-bitmap-files-from-raw-pixel-data-in-c/
static int SavePixelsToBitmap(const std::array<uint8_t, SCREEN_WIDTH * SCREEN_HEIGHT * 4> &pixels, const std::string &fname)
{
    // Some basic bitmap parameters
    unsigned long headers_size = sizeof(BITMAPFILEHEADER) +
                                 sizeof(BITMAPINFOHEADER);

    BITMAPINFOHEADER bmpInfoHeader = {0};

    // Set the size
    bmpInfoHeader.biSize = sizeof(BITMAPINFOHEADER);

    // Bit count
    bmpInfoHeader.biBitCount = 32;

    // Use all colors
    bmpInfoHeader.biClrImportant = 0;

    // Use as many colors according to bits per pixel
    bmpInfoHeader.biClrUsed = 0;

    // Store as un Compressed
    bmpInfoHeader.biCompression = BI_RGB;

    // Set the height in pixels
    bmpInfoHeader.biHeight = SCREEN_HEIGHT;

    // Width of the Image in pixels
    bmpInfoHeader.biWidth = SCREEN_WIDTH;

    // Default number of planes
    bmpInfoHeader.biPlanes = 1;

    // Calculate the image size in bytes
    bmpInfoHeader.biSizeImage = SCREEN_HEIGHT * SCREEN_WIDTH * 4;

    BITMAPFILEHEADER bfh = {0};

    // This value should be values of BM letters i.e 0x4D42
    // 0x4D = M 0×42 = B storing in reverse order to match with endian
    bfh.bfType = 0x4D42;
    //bfh.bfType = 'B'+('M' << 8);

    // <<8 used to shift ‘M’ to end  */

    // Offset to the RGBQUAD
    bfh.bfOffBits = headers_size;

    // Total size of image including size of headers
    bfh.bfSize = headers_size;

    // Create the file in disk to write
    TCHAR bmp_file[MAX_PATH];
    TCHAR exe_path[MAX_PATH] = {0};

    GetModuleFileName(NULL, exe_path, MAX_PATH);
    PathCombine(bmp_file, std::string(exe_path).substr(0, std::string(exe_path).find_last_of("\\/")).c_str(), fname.c_str());

    HANDLE hFile = CreateFile(TEXT(bmp_file),
                              GENERIC_WRITE,
                              0,
                              NULL,
                              CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL,
                              NULL);

    // Return if error opening file
    if(!hFile) return 0;

    DWORD dwWritten = 0;

    // Write the File header
    WriteFile(hFile,
              &bfh,
              sizeof(bfh),
              &dwWritten,
              NULL);

    // Write the bitmap info header
    WriteFile(hFile,
              &bmpInfoHeader,
              sizeof(bmpInfoHeader),
              &dwWritten,
              NULL);

    // Write the RGB Data

    std::array<uint8_t, SCREEN_WIDTH * SCREEN_HEIGHT * 4> cpy;

    // Perform 180 degree rotation and y-axis mirror before that...
    for (int i = 0; i < SCREEN_HEIGHT; ++i)
    {
        for (int j = 0; j < SCREEN_WIDTH; ++j)
        {
            int offset = i * SCREEN_WIDTH * 4 + j * 4;
            int offset_ = (SCREEN_HEIGHT - i - 1) * SCREEN_WIDTH * 4 + j * 4;

            cpy[offset_] = pixels[offset];
            cpy[offset_ + 1] = pixels[offset + 1];
            cpy[offset_ + 2] = pixels[offset + 2];
            cpy[offset_ + 3] = pixels[offset + 3];
        }
    }

    WriteFile(hFile,
              cpy.data(),
              bmpInfoHeader.biSizeImage,
              &dwWritten,
              NULL);

    // Close the file handle
    CloseHandle(hFile);

    return 1;
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
    m_Emulator->SetCapture(SavePixelsToBitmap);

#ifndef USE_SDL
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = SCREEN_WIDTH;
    info.bmiHeader.biHeight = SCREEN_HEIGHT;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    info.bmiHeader.biSizeImage = SCREEN_HEIGHT * SCREEN_WIDTH * 4;
    info.bmiHeader.biXPelsPerMeter = 0;
    info.bmiHeader.biYPelsPerMeter = 0;
    info.bmiHeader.biClrUsed = 0;
    info.bmiHeader.biClrImportant = 0;
#endif

    cnt = 0;
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

void GameBoyWindows::StopEmulation()
{
    m_Logger->DoLog(LOG_INFO, "GameBoyWindows::StopEmulation", "Emulation stopped");
    m_Emulator->InitComponents();
}

void GameBoyWindows::CaptureScreen()
{
    std::string fname = fmt::format("screenshot{0:d}.bmp", cnt);
    cnt += m_Emulator->CaptureScreen(fname);
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

void GameBoyWindows::Destroy()
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

    PostQuitMessage(0);
}