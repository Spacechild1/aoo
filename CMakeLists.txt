cmake_minimum_required (VERSION 2.8)

set(PROJECT "aoo")
message(STATUS "Project: ${PROJECT}")
project(${PROJECT})

include (CheckCCompilerFlag)
include (CheckCXXCompilerFlag)

if(UNIX AND NOT APPLE AND NOT MINGW)
	set(LINUX TRUE)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	set(CMAKE_COMPILER_IS_CLANG 1)
endif()

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

if(LINUX AND CMAKE_COMPILER_IS_GNUCXX)
	option(STATIC_LIBS "link with static libraries (libstdc++ and libgcc)" ON)
endif()
if(MINGW)
	option(STATIC_LIBS "link with static libraries (libstdc++, libgcc and phread)" ON)
	set(CMAKE_EXECUTABLE_SUFFIX ".exe")
endif()

set(CMAKE_INSTALL_PREFIX "" CACHE INTERNAL "Prefix prepended to install directories")

# logging
set(LOGLEVEL 2 CACHE STRING "LOGLEVEL")
message(STATUS "LOGLEVEL: ${LOGLEVEL}")
add_definitions(-DLOGLEVEL=${LOGLEVEL})

# Windows paths
if (WIN32 OR MINGW)
    # check if "Program Files (x86)" exists (64-bit Windows) and if we compile for 32-bit
    set(_pf_x86 "ProgramFiles(x86)")
    if (DEFINED ENV{${_pf_x86}} AND (CMAKE_SIZEOF_VOID_P EQUAL 4))
        set(PROGRAMFILES $ENV{${_pf_x86}})
    else()
        set(PROGRAMFILES $ENV{PROGRAMFILES})
    endif()
    set(APPDATA $ENV{APPDATA})
    set(LOCALAPPDATA $ENV{LOCALAPPDATA})
endif()

# compiler flags
if (CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANG)
    add_definitions(-fvisibility=hidden)

    CHECK_CXX_COMPILER_FLAG(-msse HAS_CXX_SSE)
    if (HAS_CXX_SSE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse")
    endif()

    CHECK_CXX_COMPILER_FLAG(-msse2 HAS_CXX_SSE2)
    if (HAS_CXX_SSE2)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse2")
    endif()

    CHECK_CXX_COMPILER_FLAG(-msse3 HAS_CXX_SSE3)
    if (HAS_CXX_SSE3)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse3")
    endif()

    CHECK_CXX_COMPILER_FLAG(-msse4 HAS_CXX_SSE4)
    if (HAS_CXX_SSE4)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4")
    endif()

    CHECK_CXX_COMPILER_FLAG(-mfpmath=sse HAS_CXX_FPMATH_SSE)
    if (HAS_CXX_FPMATH_SSE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfpmath=sse")
    endif()

    if(NATIVE)
        add_definitions(-march=native)
    endif()

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -ffast-math -funroll-loops -fomit-frame-pointer")

    if(CMAKE_COMPILER_IS_CLANG)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    endif()
endif()
if (MINGW)
    set(CMAKE_CXX_COMPILER g++)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mstackrealign")
endif()
if (LINUX)
    add_definitions("-fPIC")
endif()

# dependencies
include_directories(deps)

# oscpack
option(SYSTEM_OSCPACK "use system provided oscpack library" OFF)
if (SYSTEM_OSCPACK)
    set(OSCPACK_LIB "oscpack" CACHE STRING "oscpack linker flags")
else()
    set(OSCPACK_LIB "oscpack")
    add_subdirectory("deps/oscpack")
endif()

# md5
option(SYSTEM_MD5 "use system provided md5 library" OFF)
if (SYSTEM_MD5)
    set(MD5_LIB "md5" CACHE STRING "md5 linker flags")
else()
    set(MD5_LIB "md5")
    add_subdirectory("deps/md5")
endif()

# Opus
option(CODEC_OPUS "use Opus codec" ON)
if (CODEC_OPUS)
    add_definitions("-DUSE_CODEC_OPUS=1")

    option(SYSTEM_OPUS "use system provided Opus library" OFF)
    if (SYSTEM_OPUS)
        set(OPUS_LIB "opus" CACHE STRING "Opus linker flags")
    else()
        include_directories("deps/opus/include")
        find_library(OPUS_LIB "opus" HINTS "deps/opus/lib" REQUIRED)
    endif()
    message(STATUS "Opus library: ${OPUS_LIB}")
endif()

# platform specific linker flags
if (LINUX)
    list(APPEND LIBS "-pthread")
    if(STATIC_LIBS)
        list(APPEND LIBS "-static-libstdc++" "-static-libgcc")
    endif()
    set(CMAKE_BUILD_RPATH_USE_ORIGIN ON)
endif()
if (MINGW)
    if (STATIC_LIBS)
        list(APPEND LIBS "-static-libstdc++" "-static-libgcc" "-static -lpthread")
    else()
        list(APPEND LIBS "-lpthread")
    endif()
endif()
if (APPLE)
    list(APPEND LIBS "-lpthread")
endif()

# AOO library
set(AOO "aoo")
set(AOONET "aoonet")
set(AOO_STATIC "aoo_static")
set(AOONET_STATIC "aoonet_static")
include_directories(lib)
add_subdirectory(lib)

# Pd
option(PD "build Pd external" ON)
if (PD)
    add_subdirectory(pd)
endif()