# <ruby>脳<rp>(</rp><rt>のう</rt><rp>)</rp></ruby><ruby>腐<rp>(</rp><rt>ふ</rt><rp>)</rp></ruby>

WIP Gameboy emulator written in C++

## Preview

![](media/mario-bg-preview.mp4)

## Build Instructions

Requirements:

- TDM-GCC v9.2.0 (x64)
- fmt v8.0.0 (already preinstalled in `include/3rdparty`)
- SDL v2.0.12 (optional)

Run `setup.bat` first before anything else.

To build, type
```
mingw32-make SDL=1
```
Remove `SDL=1` if you don't have SDL installed. Use `DEBUG=1` for building a debug release.

To clean, type
```
mingw32-make clean
```

## Colour Palettes

Colours can be modified in `config.ini`.

GameBoy Green (default)
```
Color0=E0.F8.D0
Color1=88.C0.70
Color2=34.68.56
Color3=08.18.20
```

Greyscale
```
Color0=FF.FF.FF
Color1=A9.A9.A9
Color2=54.54.54
Color3=00.00.00
```

SpazeHaze
```
Color0=F8.E3.C4
Color1=CC.34.95
Color2=6B.1F.B1
Color3=0B.06.30
```

Mist GB
```
Color0=C4.F0.C2
Color1=5A.B9.A8
Color2=1E.60.6E
Color3=2D.1B.00
```