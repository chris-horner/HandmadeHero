@echo off
IF NOT EXIST build mkdir build
pushd build
cl -MT -nologo -Gm- -GR- -EHa- -Od -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_WIN32=1 -Z7 -FC ..\src\win32_handmade.cpp /link -opt:ref user32.lib gdi32.lib
popd

