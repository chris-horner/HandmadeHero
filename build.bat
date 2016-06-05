@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x64
IF NOT EXIST build mkdir build
pushd build
cl -DHANDMADE_WIN32=1 -Zi -FC ..\win32_handmade.cpp user32.lib gdi32.lib
popd

