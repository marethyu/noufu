CXX = g++
CXXFLAGS = -std=c++2a -Wall -fmax-errors=5

.PHONY: all
.PHONY: clean

SRC_PATH = ./src/
INC_PATH = ./include/

OBJ1 := Cartridge.o \
        CartridgePlainROM.o \
        CartridgeMBC1ROM.o \
        CPUOpcodes.o \
        CPU.o \
        InterruptManager.o \
        MemoryController.o \
        Timer.o \
        JoyPad.o \
        PixelFIFO.o \
        PixelFetcher.o \
        PPU.o \
        Emulator.o \
        EmulatorConfig.o

ifeq ($(DEBUG), 1)
 LINKFLAGS = -lSDL2
 OBJ1 += DebuggerMain.o
 OBJ_PATH = ./obj/cli/
 TARGET = ./bin/cli/noufu.exe
else
 OBJ1 += GameBoyWindows.o WinMain.o
 LINKFLAGS = -mwindows -Wl,-subsystem,windows --machine-windows -lcomdlg32 -lshlwapi
 ifeq ($(SDL), 1)
  CXXFLAGS += -D USE_SDL
  LINKFLAGS += -lSDL2
  OBJ_PATH = ./obj/gui/sdl/
  TARGET = ./bin/gui/sdl/noufu.exe
 else
  LINKFLAGS += -lgdi32
  OBJ_PATH = ./obj/gui/gdi/
  TARGET = ./bin/gui/gdi/noufu.exe
  SDL=0
 endif
endif

OBJ := $(patsubst %,$(OBJ_PATH)%,$(OBJ1))

all: $(TARGET)
	@echo [INFO] Finished building [$(TARGET)]

$(TARGET): $(OBJ)
	@echo [INFO] Creating Binary Executable [$(TARGET)]
	@$(CXX) -o $@ $^ $(LINKFLAGS)

$(OBJ_PATH)%.o: $(SRC_PATH)%.cpp
	@echo [CXX] $<
	@$(CXX) $(CXXFLAGS) -o $@ -c $< -I $(INC_PATH)

clean:
	if exist obj\cli\*.o del /q obj\cli\*.o
	if exist obj\gui\gdi\*.o del /q obj\gui\gdi\*.o
	if exist obj\gui\sdl\*.o del /q obj\gui\sdl\*.o
	if exist bin\cli\noufu.exe del /q bin\cli\noufu.exe
	if exist bin\gui\gdi\noufu.exe del /q bin\gui\gdi\noufu.exe
	if exist bin\gui\sdl\noufu.exe del /q bin\gui\sdl\noufu.exe
