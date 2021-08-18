#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#include "Constants.h"
#include "Emulator.h"
#include "Version.h"

#define FMT_HEADER_ONLY
#include "fmt/format.h"

#define CMD_NIL -2
#define CMD_QUIT -1
#define CMD_STEP 0
#define CMD_BLARGG 1

const std::string ROM_FILE_PATH = "C:/Users/Jimmy/OneDrive/Documents/git/noufu/ROMS/tests/blargg_tests/instr_timing/instr_timing.gb";

SDL_SpinLock lock = 0;

SDL_atomic_t cmd;
SDL_atomic_t ntimes;

std::unordered_map<std::string, int> mp = {
    {"quit", CMD_QUIT},
    {"step", CMD_STEP},
    {"blargg", CMD_BLARGG}
};

std::shared_ptr<Logger> logger = std::make_shared<Logger>("emulation_log.txt");
std::shared_ptr<EmulatorConfig> config = std::make_shared<EmulatorConfig>();

static void MyMessageBox(Severity severity, const char *message)
{
    std::cout << severity_str[severity] << message << std::endl;
}

static void Initialize()
{
    std::srand(unsigned(std::time(NULL)));

    logger->SetDoMessageBox(MyMessageBox);
    if (config->InitSettings())
    {
        logger->DoLog(LOG_WARN_POPUP, "main.cpp.Initialize", "The configuration file was not found, so the new one created with default settings.");
    }
}

static void PrintPrologue()
{
    std::cout << fmt::format("Noufu v{} (Debug mode)", CURRENT_VERSION) << std::endl;
    std::cout << fmt::format("Type \"help\" for more information.") << std::endl;
}

static int int_from_string(const std::string& s, bool isHex)
{
    int x;
    std::stringstream ss;
    if (isHex) ss << std::hex << s; else ss << s;
    ss >> x;
    return x;
}

static int Repl(void *a)
{
    std::string input_str;

    while (SDL_AtomicGet(&cmd) != CMD_QUIT)
    {
        if (lock != 0)
        {
            continue;
        }

        std::cout << ">> ";
        std::getline(std::cin, input_str);

        std::istringstream iss = std::istringstream{input_str};
        std::string str;

        iss >> str;

        if (mp.find(str) != mp.end())
        {
            int c = mp[str];

            if (c == CMD_STEP)
            {
                int x = 1;

                if (iss >> str)
                {
                    x = int_from_string(str, false);
                }

                SDL_AtomicSet(&ntimes, x);
            }

            SDL_AtomicSet(&cmd, c);
            SDL_AtomicLock(&lock);
        }
    }

    return 0;
}

int main(int argc, char *argv[])
{
    Initialize();
    PrintPrologue();

    std::unique_ptr<Emulator> gb_emu = std::make_unique<Emulator>(logger, config);

    gb_emu->InitComponents();
    gb_emu->m_MemControl->LoadROM(ROM_FILE_PATH);

    if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
    {
        gb_emu->m_EmulatorLogger->DoLog(LOG_ERROR, "main", "[SDL Error] Couldn't initialize SDL: {}", SDL_GetError());
        return 1;
    }

    int screenScale = std::stoi(config->GetValue("ScreenScaleFactor"));

    SDL_Window *m_Window = SDL_CreateWindow(EMULATOR_NAME,
                                            SDL_WINDOWPOS_UNDEFINED,
                                            SDL_WINDOWPOS_UNDEFINED,
                                            SCREEN_WIDTH * screenScale,
                                            SCREEN_HEIGHT * screenScale,
                                            SDL_WINDOW_SHOWN);
    if (m_Window == nullptr)
    {
        gb_emu->m_EmulatorLogger->DoLog(LOG_ERROR, "main", "[SDL Error] Couldn't create window: {}", SDL_GetError());
        return 1;
    }

    SDL_Renderer *m_Renderer = SDL_CreateRenderer(m_Window, -1, SDL_RENDERER_ACCELERATED);
    if (m_Renderer == nullptr)
    {
        gb_emu->m_EmulatorLogger->DoLog(LOG_ERROR, "main", "[SDL Error] Couldn't create renderer: {}", SDL_GetError());
        return 1;
    }

    SDL_Texture *m_Texture = SDL_CreateTexture(m_Renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);
    if (m_Texture == nullptr)
    {
        gb_emu->m_EmulatorLogger->DoLog(LOG_ERROR, "main", "[SDL Error] Couldn't create texture: {}", SDL_GetError());
        return 1;
    }

    SDL_SetRenderDrawColor(m_Renderer, 0, 0, 0, 255);
    SDL_RenderClear(m_Renderer);
    SDL_UpdateTexture(m_Texture, nullptr, gb_emu->m_GPU->m_Pixels.data(), SCREEN_WIDTH * 4);
    SDL_RenderCopy(m_Renderer, m_Texture, nullptr, nullptr);
    SDL_RenderPresent(m_Renderer);

    std::vector<char> blargg_serial;
    SDL_Event event;
    bool quit = false;

    SDL_AtomicSet(&cmd, CMD_NIL);
    SDL_AtomicSet(&ntimes, 0);

    SDL_CreateThread(Repl, "Repl", (void*) nullptr);

    while (!quit)
    {
        while(SDL_PollEvent(&event))
        {
            if(event.type == SDL_QUIT)
            {
                SDL_AtomicSet(&cmd, CMD_QUIT);
                break;
            }
        }

        switch (SDL_AtomicGet(&cmd))
        {
        case CMD_NIL:
            break;
        case CMD_QUIT:
        {
            quit = true;
            SDL_AtomicUnlock(&lock);
            break;
        }
        case CMD_STEP:
        {
            gb_emu->Debug_Step(blargg_serial, 1);

            SDL_SetRenderDrawColor(m_Renderer, 0, 0, 0, 255);
            SDL_RenderClear(m_Renderer);
            SDL_UpdateTexture(m_Texture, nullptr, gb_emu->m_GPU->m_Pixels.data(), SCREEN_WIDTH * 4);
            SDL_RenderCopy(m_Renderer, m_Texture, nullptr, nullptr);
            SDL_RenderPresent(m_Renderer);

            if (SDL_AtomicDecRef(&ntimes))
            {
                SDL_AtomicSet(&cmd, CMD_NIL);
                SDL_AtomicUnlock(&lock);
            }
            break;
        }
        case CMD_BLARGG:
        {
            std::string blargg_str(blargg_serial.begin(), blargg_serial.end());
            std::cout << blargg_str << std::endl;

            SDL_AtomicSet(&cmd, CMD_NIL);
            SDL_AtomicUnlock(&lock);
            break;
        }
        }
    }

    SDL_DestroyTexture(m_Texture);
    m_Texture = nullptr;

    SDL_DestroyRenderer(m_Renderer);
    m_Renderer = nullptr;

    SDL_DestroyWindow(m_Window);
    m_Window = nullptr;

    SDL_Quit();

    return 0;
}