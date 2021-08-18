#include <cstdlib>
#include <ctime>

#include "Constants.h"
#include "GameBoyWindows.h"
#include "Version.h"

#define ID_LOAD_ROM 0
#define ID_RELOAD_ROM 1
#define ID_PAUSE_RESUME_EMU 2
#define ID_STOP_EMULATION 3
#define ID_CAPTURE_SCREEN 4
#define ID_EXIT 5
#define ID_ABOUT 6

#define ID_TIMER 1

// Delay between updates
static const int UPDATE_INTERVAL = 14; // 1000 ms / 59.72 fps = 16.744 (adjusted for win32 timer)

std::shared_ptr<Logger> logger = std::make_shared<Logger>("emulation_log.txt");
std::shared_ptr<EmulatorConfig> config = std::make_shared<EmulatorConfig>();

static void MyMessageBox(Severity severity, const char *message)
{
    MessageBox(NULL,
               TEXT(message),
               TEXT(severity_str[severity].c_str()),
               MB_OK | (severity == LOG_WARN_POPUP ? MB_ICONWARNING :
                                                     MB_ICONERROR));
}

static void Initialize()
{
    std::srand(unsigned(std::time(NULL)));

    logger->SetDoMessageBox(MyMessageBox);
    if (config->InitSettings())
    {
        logger->DoLog(LOG_WARN_POPUP, "Emulator::Emulator", "The configuration file was not found, so the new one created with default settings.");
    }
}

static void StartTimer(HWND hWnd)
{
    if(!SetTimer(hWnd, ID_TIMER, UPDATE_INTERVAL, NULL))
    {
        MessageBox(hWnd, "Could not set timer!", "Error", MB_OK | MB_ICONEXCLAMATION);
        PostQuitMessage(1);
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static GameBoyWindows gb(logger, config);
    static HMENU hMenuBar;
    static HMENU hFile, hHelp;
    static bool paused;

    switch (msg)
    {
    case WM_CREATE:
    {
#ifdef USE_SDL
        if (!gb.Create(hWnd))
        {
            DestroyWindow(hWnd);
        }
#endif
        hMenuBar = CreateMenu();

        hFile = CreatePopupMenu();
        hHelp = CreatePopupMenu();

        AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR) hFile, "File");
        AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR) hHelp, "Help");

        AppendMenu(hFile, MF_STRING, ID_LOAD_ROM, "Load ROM");
        AppendMenu(hFile, MF_STRING, ID_RELOAD_ROM, "Reload ROM");
        AppendMenu(hFile, MF_STRING, ID_PAUSE_RESUME_EMU, "Pause emulation");
        AppendMenu(hFile, MF_STRING, ID_STOP_EMULATION, "Stop emulation");
        AppendMenu(hFile, MF_STRING, ID_CAPTURE_SCREEN, "Screenshot");
        AppendMenu(hFile, MF_STRING, ID_EXIT, "Exit");

        AppendMenu(hHelp, MF_STRING, ID_ABOUT, "About");

        SetMenu(hWnd, hMenuBar);

        EnableMenuItem(hMenuBar, ID_RELOAD_ROM, MF_DISABLED | MF_GRAYED);
        EnableMenuItem(hMenuBar, ID_PAUSE_RESUME_EMU, MF_DISABLED | MF_GRAYED);
        EnableMenuItem(hMenuBar, ID_STOP_EMULATION, MF_DISABLED | MF_GRAYED);
        EnableMenuItem(hMenuBar, ID_CAPTURE_SCREEN, MF_DISABLED | MF_GRAYED);

        gb.Initialize();
        paused = false;

        break;
    }
    case WM_TIMER:
    {
        gb.Update();
        InvalidateRect(hWnd, NULL, FALSE);
        break;
    }
    case WM_PAINT:
    {
#ifndef USE_SDL
        gb.RenderGraphics(hWnd);
#else
        gb.RenderGraphics();
#endif
        break;
    }
    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case ID_LOAD_ROM:
        {
            OPENFILENAME ofn;
            char szFileName[MAX_PATH] = "";
            ZeroMemory(&ofn, sizeof(ofn));

            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hWnd;
            ofn.lpstrFilter = "ROM files (*.gb)\0*.gb\0";
            ofn.lpstrFile = szFileName;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
            ofn.lpstrDefExt = "gb";

            if(GetOpenFileName(&ofn))
            {
                gb.LoadROM(szFileName);

                EnableMenuItem(hMenuBar, ID_RELOAD_ROM, MF_ENABLED);
                EnableMenuItem(hMenuBar, ID_PAUSE_RESUME_EMU, MF_ENABLED);
                EnableMenuItem(hMenuBar, ID_STOP_EMULATION, MF_ENABLED);
                EnableMenuItem(hMenuBar, ID_CAPTURE_SCREEN, MF_ENABLED);

                StartTimer(hWnd);
            }
            break;
        }
        case ID_RELOAD_ROM:
        {
            gb.ReloadROM();
            StartTimer(hWnd);
            break;
        }
        case ID_PAUSE_RESUME_EMU:
        {
            paused = !paused;

            if (paused)
            {
                KillTimer(hWnd, ID_TIMER);
                ModifyMenu(hFile, ID_PAUSE_RESUME_EMU, MF_STRING | MF_BYCOMMAND, ID_PAUSE_RESUME_EMU, "Resume emulation");
            }
            else
            {
                StartTimer(hWnd);
                ModifyMenu(hFile, ID_PAUSE_RESUME_EMU, MF_STRING | MF_BYCOMMAND, ID_PAUSE_RESUME_EMU, "Pause emulation");
            }
            break;
        }
        case ID_STOP_EMULATION:
        {
            gb.StopEmulation();

            EnableMenuItem(hMenuBar, ID_RELOAD_ROM, MF_DISABLED | MF_GRAYED);
            EnableMenuItem(hMenuBar, ID_PAUSE_RESUME_EMU, MF_DISABLED | MF_GRAYED);
            EnableMenuItem(hMenuBar, ID_STOP_EMULATION, MF_DISABLED | MF_GRAYED);
            EnableMenuItem(hMenuBar, ID_CAPTURE_SCREEN, MF_DISABLED | MF_GRAYED);

            KillTimer(hWnd, ID_TIMER);
            InvalidateRect(hWnd, NULL, FALSE);
            break;
        }
        case ID_CAPTURE_SCREEN:
        {
            gb.CaptureScreen();
            break;
        }
        case ID_EXIT:
        {
            DestroyWindow(hWnd);
            break;
        }
        case ID_ABOUT:
        {
            MessageBox(hWnd,
                       TEXT("i have no idea what to write here"),
                       TEXT(fmt::format("About Noufu v{}", CURRENT_VERSION).c_str()),
                       MB_ICONINFORMATION | MB_OK);
            break;
        }
        }
        break;
    }
    case WM_KEYDOWN:
    {
        gb.HandleKeyDown(wParam);
        break;
    }
    case WM_KEYUP:
    {
        gb.HandleKeyUp(wParam);
        break;
    }
    case WM_CLOSE:
    {
        DestroyWindow(hWnd);
        break;
    }
    case WM_DESTROY:
    {
        KillTimer(hWnd, ID_TIMER);
        gb.Destroy();
        break;
    }
    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }

    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    Initialize();

    const TCHAR szClassName[] = TEXT("MyClass");

    WNDCLASS wc;
    HWND hWnd;
    MSG msg;
    RECT rcClient;
    UINT style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE;

    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = szClassName;
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClass(&wc))
    {
        MessageBox(NULL, TEXT("Window Registration Failed!"), TEXT("Error!"),
            MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    int screenScale = std::stoi(config->GetValue("ScreenScaleFactor"));

    rcClient.left = 0;
    rcClient.top = 0;
    rcClient.right = SCREEN_WIDTH * screenScale;
    rcClient.bottom = SCREEN_HEIGHT * screenScale;

    AdjustWindowRectEx(&rcClient, style, TRUE, 0);

    hWnd = CreateWindow(szClassName,
        TEXT(EMULATOR_NAME),
        style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rcClient.right - rcClient.left,
        rcClient.bottom - rcClient.top,
        NULL,
        NULL,
        hInstance,
        NULL);

    if (hWnd == NULL)
    {
        MessageBox(NULL, TEXT("Window Creation Failed!"), TEXT("Error!"),
            MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int) msg.wParam;
}