# AOO library

#-------------------------------------------------------------
# AOO compile time options
#-------------------------------------------------------------

if (CMAKE_COMPILER_IS_GNUCXX)
    # report stack usage
    option(AOO_STACK_USAGE "report stack usage" OFF)
    mark_as_advanced(AOO_STACK_USAGE)
endif()

# sample size
set(AOO_SAMPLE_SIZE 32 CACHE STRING "Audio sample size")
set_property(CACHE AOO_SAMPLE_SIZE PROPERTY STRINGS "32;64")
message(STATUS "Audio sample size: ${AOO_SAMPLE_SIZE} bits")

# logging
set(AOO_LOG_LEVEL "Warning" CACHE STRING "compile time log level")
set_property(CACHE AOO_LOG_LEVEL PROPERTY STRINGS "None;Error;Warning;Verbose;Debug")
message(STATUS "Log level: ${AOO_LOG_LEVEL}")

# opus support
message(STATUS "Use Opus codec: ${AOO_USE_OPUS}")

# networking support
option(AOO_NET "Build with internal networking support" ON)
message(STATUS "Use internal networking (AOO NET): ${AOO_NET}")

# IPv6 support
# NB: set this on the top level because it is required by
# common/net_utils.h, which is also used in the Pd external.
option(AOO_USE_IPV6 "Build with IPv6 support" ON)
mark_as_advanced(AOO_USE_IPV6)
message(STATUS "IPv6 support: ${AOO_USE_IPV6}")

# compile time options
option(AOO_CUSTOM_ALLOCATOR "build with custom allocator support" OFF)
mark_as_advanced(AOO_CUSTOM_ALLOCATOR)
message(STATUS "Use custom allocator: ${AOO_CUSTOM_ALLOCATOR}")

set(AOO_MAX_PACKET_SIZE 4096 CACHE STRING "max. UDP packet size")
mark_as_advanced(AOO_MAX_PACKET_SIZE)
message(STATUS "Max. UDP packet size: ${AOO_MAX_PACKET_SIZE}")

# compile time debugging options
option(AOO_DEBUG_MEMORY "debug memory usage" OFF)
mark_as_advanced(AOO_DEBUG_MEMORY)

option(AOO_DEBUG_DATA "debug data transmission" OFF)
mark_as_advanced(AOO_DEBUG_DATA)

option(AOO_DEBUG_RESEND "debug data retransmission" OFF)
mark_as_advanced(AOO_DEBUG_RESEND)

option(AOO_DEBUG_DLL "debug time DLL filter" OFF)
mark_as_advanced(AOO_DEBUG_DLL)

option(AOO_DEBUG_RESAMPLER "debug resampler" OFF)
mark_as_advanced(AOO_DEBUG_RESAMPLER)

option(AOO_DEBUG_JITTER_BUFFER "debug jitter buffer" OFF)
mark_as_advanced(AOO_DEBUG_JITTER_BUFFER)

option(AOO_DEBUG_STREAM_MESSAGE "debug jitter buffer" OFF)
mark_as_advanced(AOO_DEBUG_STREAM_MESSAGE)

option(AOO_DEBUG_RELAY "debug relay" OFF)
mark_as_advanced(AOO_DEBUG_RELAY)

option(AOO_DEBUG_CLIENT_MESSAGE "debug client message" OFF)
mark_as_advanced(AOO_DEBUG_CLIENT_MESSAGE)

option(AOO_CLIENT_SIMULATE "network simulation in the client" OFF)
mark_as_advanced(AOO_CLIENT_SIMULATE)

#-----------------------------------------------------------------
# common properties (libaoo, Pd external, examples, tests, etc.)
#-----------------------------------------------------------------

# properties shared by all targets in this repository,
# but shich should not be exported to library consumers.
add_library(aoo_common INTERFACE)

# AOO_LOG_LEVEL is also used by the Pd external and some programs
target_compile_definitions(aoo_common INTERFACE AOO_LOG_LEVEL=kAooLogLevel${AOO_LOG_LEVEL})

if (CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANG)
    if (AOO_STACK_USAGE)
        target_compile_options(aoo_common INTERFACE -fstack-usage)
    endif()

    # warnings/errors
    target_compile_options(aoo_common INTERFACE -Wall -Wextra)
    # disable some useless and noisy warnings
    target_compile_options(aoo_common INTERFACE
        -Wno-unused-parameter -Wno-unused-variable -Wno-sign-compare)
    # some warnings should be errors
    target_compile_options(aoo_common INTERFACE -Werror=return-type)

    # optimization flags
    target_compile_options(aoo_common INTERFACE
        -ffast-math -funroll-loops -fomit-frame-pointer)

    if (HAVE_CXX_SSE)
        target_compile_options(aoo_common INTERFACE -msse)
    endif()

    if (HAVE_CXX_SSE2)
        target_compile_options(aoo_common INTERFACE -msse2)
    endif()

    if (HAVE_CXX_SSE3)
        target_compile_options(aoo_common INTERFACE -msse3)
    endif()

    # people still own old machines that don't support SSE4
    if (FALSE AND HAVE_CXX_SSE4)
        target_compile_options(aoo_common INTERFACE -msse4)
    endif()

    if (HAVE_CXX_FPMATH_SSE)
        target_compile_options(aoo_common INTERFACE -mfpmath=sse)
    endif()

    option(AOO_NATIVE "optimize for this machine (not portable!)" OFF)
    if (AOO_NATIVE)
        target_compile_options(aoo_common INTERFACE -march=native)
    endif()
endif()

if (MSVC)
    target_compile_options(aoo_common INTERFACE /Zc:__cplusplus /fp:fast)

    # disable some warnings
    # C4267 / C4244: conversion with possible loss of data
    # C4101: unreferenced local variable
    # C4018: signed/unsigned mismatch
    # C4996: unsafe functions (e.g. strcpy() or sscanf())
    target_compile_options(aoo_common INTERFACE
        /wd4267 /wd4244 /wd4101 /wd4018 /wd4996)
endif()

if (MINGW)
    target_compile_options(aoo_common INTERFACE -mstackrealign)
endif()

# common include directories
target_include_directories(aoo_common INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../deps>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../deps/oscpack>)

# clang++:
if (CMAKE_COMPILER_IS_CLANG)
    target_link_options(aoo_common INTERFACE -stdlib=libc++)
endif()

if (LINUX)
    if(AOO_STATIC_LIBS)
        target_link_options(aoo_common INTERFACE
            -static-libstdc++ -static-libgcc)
    endif()
endif()

if (MINGW)
    # HACK: somehow --strip resp. install/strip target do not work on MSys2,
    # so we manually strip the symbols at link time - but only in Release mode.
    target_link_options(aoo_common INTERFACE $<$<CONFIG:RELEASE>:-s>)
    if (AOO_STATIC_LIBS)
        target_link_options(aoo_common INTERFACE
            -static-libstdc++ -static-libgcc -static -lpthread)
    endif()
endif()

#-------------------------------------------------------------
# AOO library
#-------------------------------------------------------------

add_library(aoo
    # public headers
    "../include/aoo.h"
    "../include/aoo_client.h"
    "../include/aoo_client.hpp"
    "../include/aoo_codec.h"
    "../include/aoo_config.h"
    "../include/aoo_controls.h"
    "../include/aoo_defines.h"
    "../include/aoo_events.h"
    "../include/aoo_requests.h"
    "../include/aoo_server.h"
    "../include/aoo_server.hpp"
    "../include/aoo_sink.h"
    "../include/aoo_sink.hpp"
    "../include/aoo_source.h"
    "../include/aoo_source.hpp"
    "../include/aoo_types.h"
    "../include/codec/aoo_null.h"
    "../include/codec/aoo_pcm.h"
    # sources
    "src/aoo.cpp"
    "src/binmsg.hpp"
    "src/data_frame.cpp"
    "src/data_frame.hpp"
    "src/detail.hpp"
    "src/events.hpp"
    "src/memory.hpp"
    "src/packet_buffer.cpp"
    "src/packet_buffer.hpp"
    "src/resampler.cpp"
    "src/resampler.hpp"
    "src/rt_memory_pool.hpp"
    "src/sink.cpp"
    "src/sink.hpp"
    "src/source.cpp"
    "src/source.hpp"
    "src/time_dll.hpp"
    "src/codec/null.cpp"
    "src/codec/pcm.cpp"
    # common sources
    "../common/bit_utils.hpp"
    "../common/copyable_atomic.hpp"
    "../common/lockfree.hpp"
    "../common/log.hpp"
    "../common/net_utils.cpp"
    "../common/net_utils.hpp"
    "../common/priority_queue.hpp"
    "../common/sync.cpp"
    "../common/sync.hpp"
    "../common/time.cpp"
    "../common/time.hpp"
    "../common/utils.hpp"
    # only build required files in "osc" folder
    "../deps/oscpack/osc/OscException.h"
    "../deps/oscpack/osc/OscHostEndianness.h"
    "../deps/oscpack/osc/OscOutboundPacketStream.cpp"
    "../deps/oscpack/osc/OscOutboundPacketStream.h"
    "../deps/oscpack/osc/OscReceivedElements.cpp"
    "../deps/oscpack/osc/OscReceivedElements.h"
    "../deps/oscpack/osc/OscTypes.cpp"
    "../deps/oscpack/osc/OscTypes.h")

target_include_directories(aoo PRIVATE ".")

if (AOO_NET)
    target_sources(aoo PRIVATE
        "src/net/client.cpp"
        "src/net/client.hpp"
        "src/net/client_endpoint.cpp"
        "src/net/client_endpoint.hpp"
        "src/net/client_events.hpp"
        "src/net/detail.cpp"
        "src/net/detail.hpp"
        "src/net/event.hpp"
        "src/net/message_buffer.cpp"
        "src/net/message_buffer.hpp"
        "src/net/osc_stream_receiver.hpp"
        "src/net/peer.cpp"
        "src/net/peer.hpp"
        "src/net/ping_timer.hpp"
        "src/net/server.cpp"
        "src/net/server.hpp"
        "src/net/server_events.hpp"
        "src/net/tcp_server.cpp"
        "src/net/tcp_server.hpp"
        "src/net/udp_server.cpp"
        "src/net/udp_server.hpp"
        # md5
        "../deps/md5/md5.c"
        "../deps/md5/md5.h")
endif()

if (AOO_USE_OPUS)
    target_sources(aoo PRIVATE
        "../include/codec/aoo_opus.h"
        "src/codec/opus.cpp")
endif()

target_compile_definitions(aoo PRIVATE
    AOO_BUILD
    # compile time options
    AOO_CUSTOM_ALLOCATOR=$<BOOL:${AOO_CUSTOM_ALLOCATOR}>
    AOO_MAX_PACKET_SIZE=${AOO_MAX_PACKET_SIZE}
    AOO_DEBUG_MEMORY=$<BOOL:${AOO_DEBUG_MEMORY}>
    AOO_DEBUG_DATA=$<BOOL:${AOO_DEBUG_DATA}>
    AOO_DEBUG_RESEND=$<BOOL:${AOO_DEBUG_RESEND}>
    AOO_DEBUG_DLL=$<BOOL:${AOO_DEBUG_DLL}>
    AOO_DEBUG_RESAMPLER=$<BOOL:${AOO_DEBUG_RESAMPLER}>
    AOO_DEBUG_STREAM_MESSAGE=$<BOOL:${AOO_DEBUG_STREAM_MESSAGE}>
    AOO_DEBUG_JITTER_BUFFER=$<BOOL:${AOO_DEBUG_JITTER_BUFFER}>
    AOO_DEBUG_RELAY=$<BOOL:${AOO_DEBUG_RELAY}>
    AOO_DEBUG_CLIENT_MESSAGE=$<BOOL:${AOO_DEBUG_CLIENT_MESSAGE}>
    AOO_CLIENT_SIMULATE=$<BOOL:${AOO_CLIENT_SIMULATE}>
    # features
    $<$<BOOL:${AOO_HAVE_ATOMIC_DOUBLE}>:AOO_HAVE_ATOMIC_DOUBLE>
    $<$<BOOL:${AOO_HAVE_ATOMIC_INT64}>:AOO_HAVE_ATOMIC_INT64>
    $<$<BOOL:${AOO_HAVE_PTHREAD_RWLOCK}>:AOO_HAVE_PTHREAD_RWLOCK>
)

target_link_libraries(aoo PRIVATE aoo_common)

# link with "libatomic" if available and necessary
if (AOO_HAVE_LIB_ATOMIC AND
        (NOT AOO_HAVE_ATOMIC_DOUBLE OR NOT AOO_HAVE_ATOMIC_INT64))
    target_link_options(aoo PRIVATE -latomic)
endif()

set_target_properties(aoo PROPERTIES
    SOVERSION ${aoo_version_major}
    VERSION ${aoo_version})

# static library
if (NOT BUILD_SHARED_LIBS AND MSVC)
    # for MSVC we add a "_static" suffix to avoid a name clash
    # with the "aoo.lib" import library
    set_target_properties(aoo PROPERTIES OUTPUT_NAME "aoo_static")
endif()

add_library(AOO::aoo ALIAS aoo)

#-------------------------------------------------------------
# public properties (for libaoo consumers)
#-------------------------------------------------------------

target_compile_definitions(aoo PUBLIC
    $<$<BOOL:${BUILD_SHARED_LIBS}>:AOO_SHARED>
    AOO_NET=$<BOOL:${AOO_NET}>
    AOO_USE_IPV6=$<BOOL:${AOO_USE_IPV6}>
    AOO_USE_OPUS=$<BOOL:${AOO_USE_OPUS}>
    AOO_SAMPLE_SIZE=${AOO_SAMPLE_SIZE}
    $<$<BOOL:${AOO_HAVE_SA_LEN}>:AOO_HAVE_SA_LEN>)

target_include_directories(aoo PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/aoo>)

if (LINUX)
    target_compile_options(aoo_common INTERFACE -pthread)
    target_link_libraries(aoo PUBLIC -pthread)
endif()

if (MINGW)
    if (NOT AOO_STATIC_LIBS)
        target_link_libraries(aoo PUBLIC pthread)
    endif()
    if (AOO_USE_OPUS)
        target_link_libraries(aoo PUBLIC ssp) # for fortified functions
    endif()
endif()

if (APPLE)
    target_link_libraries(aoo PUBLIC pthread)
endif()

if (WIN32)
    target_link_libraries(aoo PUBLIC ws2_32)
endif()

if (AOO_USE_OPUS)
    target_link_libraries(aoo PUBLIC Opus::opus)
endif()

#-------------------------------------------------------------
# AOO installation
#-------------------------------------------------------------

# install library files
install(TARGETS aoo aoo_common
    EXPORT AooTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT AooDevelopment
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT AooRuntime
            NAMELINK_COMPONENT AooDevelopment
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT AooRuntime
)

# TODO: install PDB file (Visual Studio)

# install public headers (note the trailing slash!)
install(DIRECTORY "../include/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/aoo"
    COMPONENT AooDevelopment)
