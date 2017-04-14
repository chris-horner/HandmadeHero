cmake_minimum_required(VERSION 3.3)
project(HandmadeHero)
add_definitions(-DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_WIN32=1)
add_executable(win32_handmade src/win32_handmade.cpp)
