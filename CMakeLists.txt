cmake_minimum_required (VERSION 3.15)

# minimum macOS deployment target; must come before project()!
if (APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum OSX deployment version")
endif()

set(aoo_version_major 2)
set(aoo_version_minor 0)
set(aoo_version_patch 0)
set(aoo_version ${aoo_version_major}.${aoo_version_minor}.${aoo_version_patch})

message(STATUS "Project: aoo ${aoo_version}")
project(aoo VERSION ${aoo_version})

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

if (NOT APPLE)
    set(CMAKE_INSTALL_RPATH $ORIGIN)
endif()

if (LINUX)
    set(CMAKE_BUILD_RPATH_USE_ORIGIN ON)
endif()

# built everything (including Opus) with -fPIC
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if (MINGW)
    set(CMAKE_EXECUTABLE_SUFFIX ".exe")
endif()

if (LINUX AND CMAKE_COMPILER_IS_GNUCXX)
    option(AOO_STATIC_RUNTIME "link with static runtime libraries (libstdc++ and libgcc)" OFF)
endif()

if (MINGW AND CMAKE_COMPILER_IS_GNUCXX)
    option(AOO_STATIC_RUNTIME "link with static runtime libraries (libstdc++, libgcc and libphread)" ON)
endif()

if (DEFINED AOO_STATIC_RUNTIME)
    message(STATUS "Build with static runtime libraries: ${AOO_STATIC_RUNTIME}")
endif()

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
    test_atomic(double AOO_HAVE_ATOMIC_DOUBLE)
    if (NOT AOO_HAVE_ATOMIC_DOUBLE)
        message(STATUS "No built-in support for atomic doubles - "
            "will be emulated in software")
    endif()

    # atomic 64-bit integer support
    test_atomic(int64 AOO_HAVE_ATOMIC_INT64)
    if (NOT AOO_HAVE_ATOMIC_INT64)
        message(STATUS "No built-in support for atomic 64-bit integers - "
            "will be emulated in software")
    endif()

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

option(AOO_BUILD_PD_EXTERNAL "build Pd external" OFF)
option(AOO_BUILD_SERVER "build AOO server program" OFF)
option(AOO_BUILD_SHARED_LIBRARY "build shared AOO library" OFF)
option(AOO_BUILD_TESTS "build test suite" OFF)
option(AOO_INSTALL_PKG_CONFIG_MODULE "install pkg-config module" ON)
option(AOO_INSTALL_CMAKE_CONFIG_MODULE "install CMake package config module" ON)

if (AOO_BUILD_SHARED_LIBRARY OR BUILD_SHARED_LIBS)
    # propagate to dependencies
    set(BUILD_SHARED_LIBS ON)
    set(AOO_BUILD_SHARED_LIBRARY ON)
endif()

#-----------------------------------------------------------------
# dependencies
#-----------------------------------------------------------------

# Opus
if (AOO_USE_OPUS)
    option(AOO_LOCAL_OPUS "use local Opus library" ON)
    if (AOO_LOCAL_OPUS)
        message(STATUS "\n*** Opus library ***\n")
        add_subdirectory("deps/opus" EXCLUDE_FROM_ALL)
        if (NOT BUILD_SHARED_LIBS)
            # HACK: make sure that we don't export any symbols
            target_compile_definitions(opus PRIVATE OPUS_EXPORT=extern)
        endif()
    else()
        find_package(Opus REQUIRED)
    endif()
endif()

#-----------------------------------------------------------------
# hide all symbols unless explicitly exported
# NOTE: do this *after* Opus to avoid warnings
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

#-----------------------------------------------------------------
# AOO library
#-----------------------------------------------------------------
message(STATUS "\n*** AOO library ***\n")
add_subdirectory("aoo")

#-----------------------------------------------------------------
# Pd external
#-----------------------------------------------------------------
if (AOO_BUILD_PD_EXTERNAL)
    message(STATUS "\n*** Pd external ***\n")
    if (AOO_BUILD_SHARED_LIBRARY)
        # FIXME: the externals use common (private) code
        message(WARNING "AOO_BUILD_PD_EXTERNAL=ON requires AOO_BUILD_SHARED_LIBRARY=OFF")
    else()
        if (NOT AOO_NET)
            message(FATAL_ERROR "Pd external requires AOO_NET")
        endif()
        add_subdirectory(pd)
    endif()
endif()

#-----------------------------------------------------------------
# aooserver program
#-----------------------------------------------------------------
if (AOO_BUILD_SERVER)
    message(STATUS "\n*** AOO server ***\n")
    if (AOO_BUILD_SHARED_LIBRARY)
        # FIXME: aooserver uses common (private) code
        message(WARNING "AOO_BUILD_SERVER=ON requires AOO_BUILD_SHARED_LIBRARY=OFF")
    else()
        if (NOT AOO_NET)
            message(FATAL_ERROR "AOO server requires AOO_NET")
        endif()
        add_subdirectory(server)
    endif()
endif()

#-----------------------------------------------------------------
# test suite
#-----------------------------------------------------------------
if (AOO_BUILD_TESTS)
    message(STATUS "\n*** Test suite ***\n")
    if (AOO_BUILD_SHARED_LIBRARY)
        # FIXME: get rid of common (private) code
        message(WARNING "AOO_BUILD_TESTS=ON requires AOO_BUILD_SHARED_LIBRARY=OFF")
    else()
        add_subdirectory("tests")
    endif()
endif()
