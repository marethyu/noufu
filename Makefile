CXX = g++
CXXFLAGS = -std=c++2a -Wall -fmax-errors=5
WIN_FLAGS = -mwindows -Wl,-subsystem,windows --machine-windows
LINKFLAGS := -lSDL2 -lgdi32 -lcomdlg32

.PHONY: all
.PHONY: clean

SRC_PATH := ./src/
OBJ_PATH := ./obj/
INC_PATH := ./include/

# TODO add ./bin/cli/noufu_cli.exe
TARGET = ./bin/gui/noufu.exe

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
	del /q obj\*.o
	del /q bin\gui\noufu.exe
	del /q bin\cli\noufu_cli.exe