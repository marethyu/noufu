@echo off

if not exist "obj/cli" mkdir "obj/cli"
if not exist "bin/cli" mkdir "bin/cli"

if not exist "obj/gui/sdl" mkdir "obj/gui/sdl"
if not exist "bin/gui/sdl" mkdir "bin/gui/sdl"

if not exist "obj/gui/gdi" mkdir "obj/gui/gdi"
if not exist "bin/gui/gdi" mkdir "bin/gui/gdi"

copy /Y libs\SDL2.dll bin\gui\sdl
