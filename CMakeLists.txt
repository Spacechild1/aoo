cmake_minimum_required (VERSION 3.15)

# minimum macOS deployment target; must come before project()!
if (APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum OSX deployment version")
endif()

message(STATUS "Project: aoo")
project(aoo)

set(AOO_VERSION_MAJOR 2)
set(AOO_VERSION_MINOR 0)
set(AOO_VERSION_PATCH 0)
set(AOO_VERSION ${AOO_VERSION_MAJOR}.${AOO_VERSION_MINOR}.${AOO_VERSION_PATCH})

include(GNUInstallDirs)
include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)
include(cmake/test_atomic.cmake)
include(cmake/test_linker_flag.cmake)

message(STATUS "\n*** Global settings ***\n")

if(UNIX AND NOT APPLE AND NOT MINGW)
    set(LINUX TRUE)
endif()

# some MinGW setups don't define WIN32!
if (MINGW AND NOT WIN32)
    message(WARNING "Your MinGW setup does not define WIN32")
    set(WIN32 TRUE)
endif()

# check for Clang or AppleClang
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_COMPILER_IS_CLANG 1)
endif()
message(STATUS "CMAKE_CXX_COMPILER_ID: ${CMAKE_CXX_COMPILER_ID}")

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

message(STATUS "CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")

if(LINUX AND CMAKE_COMPILER_IS_GNUCXX)
    option(AOO_STATIC_LIBS "link with static libraries (libstdc++ and libgcc)" ON)
endif()

if (MINGW)
    option(AOO_STATIC_LIBS "link with static libraries (libstdc++, libgcc and phread)" ON)
    set(CMAKE_EXECUTABLE_SUFFIX ".exe")
endif()

# Windows paths
if (WIN32)
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

# C++17 required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (LINUX)
    set(CMAKE_BUILD_RPATH_USE_ORIGIN ON)
endif()

# built everything (including Opus) with -fPIC
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

#-----------------------------------------------------------------
# feature tests
#-----------------------------------------------------------------
if (CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANG)
    check_cxx_compiler_flag("-msse" HAVE_CXX_SSE)
    check_cxx_compiler_flag("-msse2" HAVE_CXX_SSE2)
    check_cxx_compiler_flag("-msse3" HAVE_CXX_SSE3)
    check_cxx_compiler_flag("-msse4" HAVE_CXX_SSE4)
    check_cxx_compiler_flag("-mfpmath=sse" HAVE_CXX_FPMATH_SSE)

    test_linker_flag("-latomic" AOO_HAVE_LIB_ATOMIC)

    # atomic double support
    test_atomic(ATOMIC_DOUBLE "atomic_double.cpp")

    # atomic 64-bit integer support
    test_atomic(ATOMIC_INT64 "atomic_int64.cpp")

    # pthread_rwlock_t support
    if (NOT WIN32)
        if (NOT DEFINED AOO_HAVE_PTHREAD_RWLOCK)
            message(STATUS "Testing support for pthread_rwlock_t")
            if (NOT ESP_PLATFORM)
                set(_PTHREAD_LIB "pthread")
            endif()

            try_compile(RESULT_VAR
                "${CMAKE_CURRENT_BINARY_DIR}"
                "${CMAKE_CURRENT_SOURCE_DIR}/cmake/pthread_rwlock.cpp"
                OUTPUT_VARIABLE COMPILE_OUTPUT
                CXX_STANDARD 17
                LINK_LIBRARIES ${_PTHREAD_LIB})

            if (RESULT_VAR)
                message(STATUS "- ok")
            else()
                message(STATUS "- failed")
                message(VERBOSE ${COMPILE_OUTPUT})
            endif()

            set(AOO_HAVE_PTHREAD_RWLOCK ${RESULT_VAR} CACHE INTERNAL "pthread_rwlock_t support")
        endif()
    endif()
endif()

# check for sa_len in sockaddr
if (NOT DEFINED AOO_HAVE_SA_LEN)
    # FIXME: for some reason ESP-IDF cannot find any system headers inside try_compile().
    # We know that on the ESP32 platform sockaddr has the sa_lan member.
    if (ESP_PLATFORM)
        set(AOO_HAVE_SA_LEN TRUE CACHE INTERNAL "'sockaddr' has 'sa_len' member")
    else()
        message(STATUS "Testing for 'sa_len' member in 'sockaddr'")

        try_compile(RESULT_VAR
            "${CMAKE_CURRENT_BINARY_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/sa_len.cpp"
            COMPILE_DEFINITIONS -DESP_PLATFORM=1
            OUTPUT_VARIABLE COMPILE_OUTPUT)

        if (RESULT_VAR)
            message(STATUS "- yes")
        else()
            message(STATUS "- no")
            message(VERBOSE ${COMPILE_OUTPUT})
        endif()

        set(AOO_HAVE_SA_LEN ${RESULT_VAR} CACHE INTERNAL "'sockaddr' has 'sa_len' member")
    endif()
endif()

#-----------------------------------------------------------------
# build options
#-----------------------------------------------------------------

option(AOO_USE_OPUS "use Opus codec" ON)

option(AOO_BUILD_PD_EXTERNAL "build Pd external" ON)
option(AOO_BUILD_SERVER "build AOO server application" ON)
option(AOO_BUILD_SHARED_LIBRARY "build shared AOO library" ON)
option(AOO_BUILD_STATIC_LIBRARY "build static AOO library" ON)
option(AOO_BUILD_TESTS "build test suite" ON)

#-----------------------------------------------------------------
# dependencies
#-----------------------------------------------------------------

# Opus
if (AOO_USE_OPUS)
    option(AOO_LOCAL_OPUS "use local Opus library" ON)
    if (AOO_LOCAL_OPUS)
        message(STATUS "\n*** Opus library ***\n")
        add_subdirectory("deps/opus" EXCLUDE_FROM_ALL)
    else()
        find_package(Opus REQUIRED)
    endif()
endif()

#-----------------------------------------------------------------
# AOO library
#-----------------------------------------------------------------
message(STATUS "\n*** AOO library ***\n")
add_subdirectory("aoo")

#-----------------------------------------------------------------
# Pd external
#-----------------------------------------------------------------
if (AOO_BUILD_PD_EXTERNAL)
    if (NOT AOO_NET)
        message(FATAL_ERROR "Pd external requires AOO_NET")
    endif()
    message(STATUS "\n*** Pd external ***\n")
    add_subdirectory("pd")
endif()

#-----------------------------------------------------------------
# aooserver binary
#-----------------------------------------------------------------
if (AOO_BUILD_SERVER)
    if (NOT AOO_NET)
        message(FATAL_ERROR "AOO server requires AOO_NET")
    endif()
    message(STATUS "\n*** AOO server ***\n")
    add_subdirectory("server")
endif()

#-----------------------------------------------------------------
# test suite
#-----------------------------------------------------------------
if (AOO_BUILD_TESTS)
    message(STATUS "\n*** Test suite ***\n")
    add_subdirectory("tests")
endif()
