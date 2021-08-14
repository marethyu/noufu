CXX = g++
CXXFLAGS = -D GUI_MODE -std=c++17 -Wall -fmax-errors=5
WIN_FLAGS = -mwindows -Wl,-subsystem,windows --machine-windows
LINKFLAGS := -lSDL2 -lcomdlg32

.PHONY: all
.PHONY: clean

SRC_PATH := ./src/
OBJ_PATH := ./obj/
INC_PATH := ./include/

# TODO add ./bin/cli/noufu_cli.exe
TARGET = ./bin/gui/noufu_gui.exe

OBJ1 := CPUOpcodes.o \
        CPU.o \
        InterruptManager.o \
        MemoryController.o \
        Timer.o \
        JoyPad.o \
        SimpleGPU.o \
        Emulator.o \
        GameBoyWindows.o \
        loguru.o \
        EmulatorConfig.o \
        Util.o \
        WinMain.o
OBJ := $(patsubst %,$(OBJ_PATH)%,$(OBJ1))

all: $(TARGET)
	@echo [INFO] Finished building [$(TARGET)]

$(TARGET): $(OBJ)
	@echo [INFO] Creating Binary Executable [$(TARGET)]
	@$(CXX) $(WIN_FLAGS) -o $@ $^ $(LINKFLAGS)

$(OBJ_PATH)%.o: $(SRC_PATH)%.cpp
	@echo [CXX] $<
	@$(CXX) $(CXXFLAGS) -o $@ -c $< -I $(INC_PATH)

clean:
	del /q obj\*.o
	del /q bin\gui\noufu_gui.exe
	del /q bin\cli\noufu_cli.exe