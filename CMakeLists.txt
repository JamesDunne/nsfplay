cmake_minimum_required(VERSION 3.27)
project(nsfconv)

set(CMAKE_CXX_STANDARD 17)

add_executable(nsfconv

    vcm/group.cpp
    vcm/value.cpp
    xgm/fileutil.cpp
    xgm/devices/Audio/MedianFilter.cpp
    xgm/devices/Audio/echo.cpp
    xgm/devices/Audio/filter.cpp
    xgm/devices/Audio/rconv.cpp
    xgm/devices/CPU/nes_cpu.cpp
    xgm/devices/Memory/nes_bank.cpp
    xgm/devices/Memory/nes_mem.cpp
    xgm/devices/Memory/nsf2_vectors.cpp
    xgm/devices/Memory/ram64k.cpp
    xgm/devices/Misc/detect.cpp
    xgm/devices/Misc/log_cpu.cpp
    xgm/devices/Misc/nes_detect.cpp
    xgm/devices/Misc/nsf2_irq.cpp
    xgm/devices/Sound/nes_apu.cpp
    xgm/devices/Sound/nes_dmc.cpp
    xgm/devices/Sound/nes_fds.cpp
    xgm/devices/Sound/nes_fme7.cpp
    xgm/devices/Sound/nes_mmc5.cpp
    xgm/devices/Sound/nes_n106.cpp
    xgm/devices/Sound/nes_vrc6.cpp
    xgm/devices/Sound/nes_vrc7.cpp
    xgm/devices/Sound/legacy/emu2149.c
    xgm/devices/Sound/legacy/emu2212.c
    xgm/devices/Sound/legacy/emu2413.c
    xgm/player/nsf/nsf.cpp
    xgm/player/nsf/nsfconfig.cpp
    xgm/player/nsf/nsfplay.cpp
    xgm/player/nsf/pls/ppls.cpp
    xgm/player/nsf/pls/sstream.cpp
        nsfconv/main.cpp
)
include_directories(nsfconv
    vcm/
    xgm/
    xgm/devices/
    xgm/devices/Audio/
    xgm/devices/CPU/
    xgm/devices/Memory/
    xgm/devices/Misc/
    xgm/devices/Sound/
    xgm/player/
    xgm/player/nsf/
)