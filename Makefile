CXX = g++

CXXFLAGS = -std=c++2a -Wall -fmax-errors=5
WIN_FLAGS = -mwindows -Wl,-subsystem,windows --machine-windows
LINKFLAGS = -lcomdlg32

.PHONY: all
.PHONY: clean

SRC_PATH = ./src/
INC_PATH = ./include/

# TODO add ./bin/cli/noufu_cli.exe

ifeq ($(SDL), 1)
 CXXFLAGS += -D USE_SDL
 LINKFLAGS += -lSDL2
 OBJ_PATH = ./obj/sdl/
 TARGET = ./bin/gui/sdl/noufu.exe
else
 LINKFLAGS += -lgdi32
 OBJ_PATH = ./obj/gdi/
 TARGET = ./bin/gui/gdi/noufu.exe
 SDL=0
endif

$(info SDL=$(SDL))
$(info TARGET=$(TARGET))

OBJ1 := CPUOpcodes.o \
        CPU.o \
        InterruptManager.o \
        MemoryController.o \
        Timer.o \
        JoyPad.o \
        SimpleGPU.o \
        Emulator.o \
        EmulatorConfig.o \
        GameBoyWindows.o \
        WinMain.o
OBJ := $(patsubst %,$(OBJ_PATH)%,$(OBJ1))

all: $(TARGET)
	@echo [INFO] Finished building [$(TARGET)]

$(TARGET): $(OBJ)
	@echo [INFO] Creating Binary Executable [$(TARGET)]
	@$(CXX) -o $@ $^ $(WIN_FLAGS) $(LINKFLAGS)

$(OBJ_PATH)%.o: $(SRC_PATH)%.cpp
	@echo [CXX] $<
	@$(CXX) $(CXXFLAGS) -o $@ -c $< -I $(INC_PATH)

clean:
	del /q obj\gdi\*.o
	del /q obj\sdl\*.o
	del /q bin\gui\sdl\noufu.exe
	del /q bin\gui\gdi\noufu.exe
	del /q bin\cli\noufu_cli.exe
