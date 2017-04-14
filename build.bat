@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
IF NOT EXIST build mkdir build
pushd build
cl -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_WIN32=1 -Zi -FC ..\src\win32_handmade.cpp user32.lib gdi32.lib
popd

