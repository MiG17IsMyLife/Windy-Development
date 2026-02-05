cmake_minimum_required(VERSION 3.10)

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_C_COMPILER /usr/bin/i686-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER /usr/bin/i686-w64-mingw32-g++)
set(CMAKE_AR /usr/bin/i686-w64-mingw32-ar)
set(CMAKE_RC_COMPILER /usr/bin/i686-w64-mingw32-windres)

project(Windy C CXX)

set(PROJECT_NAME Windy)

# Ensure linker finds libraries in lib/x86 (SDL3) and Debug (FAudio)
link_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/x86"
    "${CMAKE_CURRENT_SOURCE_DIR}/Debug"
)

################################################################################
# Source groups
################################################################################
set(src
    "src/main.cpp"
)
source_group("src" FILES ${src})

set(src__api__graphics
    "src/api/graphics/glhooks.cpp"
    "src/api/graphics/glhooks.h"
    "src/api/graphics/glxbridge.cpp"
    "src/api/graphics/glxbridge.h"
    "src/api/graphics/x11bridge.cpp"
    "src/api/graphics/x11bridge.h"
)
source_group("src\\api\\graphics" FILES ${src__api__graphics})

set(src__api__libc
    "src/api/libc/libcbridge.cpp"
    "src/api/libc/libcbridge.h"
    "src/api/libc/pthread_barrier.cpp"
    "src/api/libc/pthread_cond.cpp"
    "src/api/libc/pthread_emu.cpp"
    "src/api/libc/pthread_emu.h"
    "src/api/libc/pthread_internal.h"
    "src/api/libc/pthread_mapper.cpp"
    "src/api/libc/pthread_mutex.cpp"
    "src/api/libc/pthread_once.cpp"
    "src/api/libc/pthread_rwlock.cpp"
    "src/api/libc/pthread_semaphore.cpp"
    "src/api/libc/pthread_spin.cpp"
    "src/api/libc/pthread_thread.cpp"
    "src/api/libc/pthread_tls.cpp"
)
source_group("src\\api\\libc" FILES ${src__api__libc})

set(src__api__linux
    "src/api/linux/filesystembridge.cpp"
    "src/api/linux/filesystembridge.h"
    "src/api/linux/gccbridge.cpp"
    "src/api/linux/gccbridge.h"
    "src/api/linux/hardwarebridge.cpp"
    "src/api/linux/hardwarebridge.h"
    "src/api/linux/ipcbridge.cpp"
    "src/api/linux/ipcbridge.h"
    "src/api/linux/linuxtypes.h"
    "src/api/linux/processbridge.cpp"
    "src/api/linux/processbridge.h"
)
source_group("src\\api\\linux" FILES ${src__api__linux})

set(src__api__sega
    "src/api/sega/libsegaapi.c"
    "src/api/sega/libsegaapi.h"
)
source_group("src\\api\\sega" FILES ${src__api__sega})

set(src__core
    "src/core/common.h"
    "src/core/config.cpp"
    "src/core/config.h"
    "src/core/elfloader.cpp"
    "src/core/elfloader.h"
    "src/core/gamedata.h"
    "src/core/iniparser.cpp"
    "src/core/iniparser.h"
    "src/core/linuxstack.cpp"
    "src/core/linuxstack.h"
    "src/core/log.cpp"
    "src/core/log.h"
    "src/core/memorymanager.cpp"
    "src/core/memorymanager.h"
    "src/core/patches.cpp"
    "src/core/patches.h"
    "src/core/symbolresolver.cpp"
    "src/core/symbolresolver.h"
)
source_group("src\\core" FILES ${src__core})

set(src__hardware
    "src/hardware/baseboard.cpp"
    "src/hardware/baseboard.h"
    "src/hardware/eepromboard.cpp"
    "src/hardware/eepromboard.h"
    "src/hardware/jvsboard.cpp"
    "src/hardware/jvsboard.h"
    "src/hardware/lindberghdevice.cpp"
    "src/hardware/lindberghdevice.h"
    "src/hardware/pcidata.h"
    "src/hardware/securityboard.cpp"
    "src/hardware/securityboard.h"
    "src/hardware/serialboard.cpp"
    "src/hardware/serialboard.h"
)
source_group("src\\hardware" FILES ${src__hardware})

set(ALL_FILES
    ${src}
    ${src__api__graphics}
    ${src__api__libc}
    ${src__api__linux}
    ${src__api__sega}
    ${src__core}
    ${src__hardware}
)

################################################################################
# MinHook
################################################################################
set(MINHOOK_SOURCES
    "src/minhook/src/buffer.c"
    "src/minhook/src/hook.c"
    "src/minhook/src/trampoline.c"
    "src/minhook/src/hde/hde32.c"
)
add_library(minhook STATIC ${MINHOOK_SOURCES})
target_include_directories(minhook PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/minhook/include")

################################################################################
# Target
################################################################################
add_executable(${PROJECT_NAME} ${ALL_FILES})
target_link_libraries(${PROJECT_NAME} PRIVATE minhook)



set(ROOT_NAMESPACE Windy)

# if(WIN32 AND NOT MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall
        -Wextra
        -fpermissive
    )

    # Increase stack size to 4MB to prevent stack overflow on startup
    target_link_options(${PROJECT_NAME} PRIVATE
        -Wl,--stack,4194304
    )

    # Statically link standard MinGW libraries
    target_link_options(${PROJECT_NAME} PRIVATE
        -static-libgcc
        -static-libstdc++
    )

    # Force static linking of winpthread by using the absolute path to the archive
    # This prevents the linker from picking up the shared library (dll) version
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "/usr/i686-w64-mingw32/lib/libwinpthread.a"
    )

    # Set C++ standard if needed, e.g. C++17
    set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)

    # Definitions for MinGW/Linux
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        "WIN32" "_CONSOLE" "UNICODE" "_UNICODE"
    )
# endif()

################################################################################
# Compile and link options
################################################################################
# if(MSVC)
#     if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
#         target_compile_options(${PROJECT_NAME} PRIVATE
#             $<$<CONFIG:Release>:
#                 /Oi;
#                 /Gy
#             >
#             /permissive-;
#             /sdl;
#             /W3;
#             ${DEFAULT_CXX_DEBUG_INFORMATION_FORMAT};
#             ${DEFAULT_CXX_EXCEPTION_HANDLING}
#         )
#     elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x86")
#         target_compile_options(${PROJECT_NAME} PRIVATE
#             $<$<CONFIG:Release>:
#                 /Oi;
#                 /Gy
#             >
#             /permissive-;
#             /sdl;
#             /W3;
#             ${DEFAULT_CXX_DEBUG_INFORMATION_FORMAT};
#             ${DEFAULT_CXX_EXCEPTION_HANDLING}
#         )
#     endif()
#     if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
#         target_link_options(${PROJECT_NAME} PRIVATE
#             $<$<CONFIG:Release>:
#                 /OPT:REF;
#                 /OPT:ICF
#             >
#             /DEBUG;
#             /SUBSYSTEM:CONSOLE
#         )
#     elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x86")
#         target_link_options(${PROJECT_NAME} PRIVATE
#             $<$<CONFIG:Release>:
#                 /OPT:REF;
#                 /OPT:ICF
#             >
#             /FIXED;
#             /DEBUG;
#             /SUBSYSTEM:CONSOLE;
#             /DYNAMICBASE:NO
#         )
#     endif()
# endif()

################################################################################
# Include directories
################################################################################
target_include_directories(${PROJECT_NAME} PRIVATE
    "/usr/i686-w64-mingw32/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

################################################################################
# Dependencies
################################################################################
################################################################################
# Dependencies
################################################################################

set(ADDITIONAL_LIBRARY_DEPENDENCIES
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/x86/SDL3.dll"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/x86/SDL3_ttf.dll"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/x86/FAudio.dll"
    "ws2_32"
    "opengl32"
    "winmm"
    "ole32"
)

target_link_libraries(${PROJECT_NAME} PRIVATE ${ADDITIONAL_LIBRARY_DEPENDENCIES})

