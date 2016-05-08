@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x64
mkdir build
pushd build
cl -Zi -FC ..\win32_handmade.cpp user32.lib gdi32.lib
popd

