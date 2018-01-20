@echo off

set CommonCompilerFlags=-MT -nologo -Gm- -GR- -EHa- -Od -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -wd4456 -wd4244 -wd4459 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_WIN32=1 -FC -Z7
set CommonLinkerFlags= -opt:ref user32.lib gdi32.lib winmm.lib

IF NOT EXIST build mkdir build
pushd build

REM 32-but build
REM cl %CommonCompilerFlags% ..\src\win32_handmade.cpp /link -subsystem:windows,5.1 %CommonLinkerFlags%

REM 64-bit build
cl %CommonCompilerFlags% ..\src\win32_handmade.cpp /link %CommonLinkerFlags%
popd

