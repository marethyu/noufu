#include <cstdlib>
#include <ctime>

#include "Constants.h"
#include "GameBoyWindows.h"

#define ID_LOAD_ROM 0
#define ID_RELOAD_ROM 1
#define ID_EXIT 2
#define ID_ABOUT 3

#define ID_TIMER 1

// Delay between updates
static const int UPDATE_INTERVAL = 14; // 1000 ms / 59.72 fps = 16.744 (adjusted for win32 timer)

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static GameBoyWindows gb;
    static HMENU hMenuBar;

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

        HMENU hFile = CreatePopupMenu();
        HMENU hHelp = CreatePopupMenu();

        AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR) hFile, "File");
        AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR) hHelp, "Help");

        AppendMenu(hFile, MF_STRING, ID_LOAD_ROM, "Load ROM");
        AppendMenu(hFile, MF_STRING, ID_RELOAD_ROM, "Reload ROM");
        AppendMenu(hFile, MF_STRING, ID_EXIT, "Exit");

        AppendMenu(hHelp, MF_STRING, ID_ABOUT, "About");

        SetMenu(hWnd, hMenuBar);

        EnableMenuItem(hMenuBar, ID_RELOAD_ROM, MF_DISABLED | MF_GRAYED);

        // resize because we just added the menubar
#ifdef USE_SDL
        gb.FixSize();
#else
        SetWindowPos(hWnd,
                     0,
                     0,
                     0,
                     SCREEN_WIDTH * SCREEN_SCALE_FACTOR,
                     SCREEN_HEIGHT * SCREEN_SCALE_FACTOR + GetSystemMetrics(SM_CYMENUSIZE),
                     SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER);
#endif
        gb.Initialize();

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

                if(!SetTimer(hWnd, ID_TIMER, UPDATE_INTERVAL, NULL))
                {
                    MessageBox(hWnd, "Could not set timer!", "Error", MB_OK | MB_ICONEXCLAMATION);
                    PostQuitMessage(1);
                }
            }
            break;
        }
        case ID_RELOAD_ROM:
        {
            gb.ReloadROM();

            if(!SetTimer(hWnd, ID_TIMER, UPDATE_INTERVAL, NULL))
            {
                MessageBox(hWnd, "Could not set timer!", "Error", MB_OK | MB_ICONEXCLAMATION);
                PostQuitMessage(1);
            }
            break;
        }
        case ID_EXIT:
        {
            DestroyWindow(hWnd);
            break;
        }
        case ID_ABOUT:
        {
            MessageBox(hWnd, TEXT("um..."), TEXT("About Noufu"), MB_ICONINFORMATION | MB_OK);
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
        gb.CleanUp();
        KillTimer(hWnd, ID_TIMER);
        gb.Destroy();
        break;
    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }

    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    std::srand(unsigned(std::time(NULL)));

    const TCHAR szClassName[] = TEXT("MyClass");

    WNDCLASS wc;
    HWND hWnd;
    MSG msg;

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

    hWnd = CreateWindow(szClassName,
        TEXT(EMULATOR_NAME),
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, SCREEN_WIDTH * SCREEN_SCALE_FACTOR, SCREEN_HEIGHT * SCREEN_SCALE_FACTOR,
        NULL, NULL, hInstance, NULL);

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