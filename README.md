# <ruby>脳<rp>(</rp><rt>のう</rt><rp>)</rp></ruby><ruby>腐<rp>(</rp><rt>ふ</rt><rp>)</rp></ruby>

WIP Gameboy emulator written in C++

## Build Instructions

Requirements:

- TDM-GCC v9.2.0 (x64)
- fmt v8.0.0 (preinstalled in `include/`)
- SDL v2.0.12 (Optional)

To build, type (remove `SDL=1` if you don't have SDL installed):
```
mingw32-make SDL=1
```

To clean, type
```
mingw32-make clean
```