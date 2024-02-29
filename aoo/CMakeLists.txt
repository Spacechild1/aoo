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

set(OPTIONS
    -DAOO_CUSTOM_ALLOCATOR=$<BOOL:${AOO_CUSTOM_ALLOCATOR}>
    -DAOO_MAX_PACKET_SIZE=${AOO_MAX_PACKET_SIZE}
    -DAOO_DEBUG_MEMORY=$<BOOL:${AOO_DEBUG_MEMORY}>
    -DAOO_DEBUG_DATA=$<BOOL:${AOO_DEBUG_DATA}>
    -DAOO_DEBUG_RESEND=$<BOOL:${AOO_DEBUG_RESEND}>
    -DAOO_DEBUG_DLL=$<BOOL:${AOO_DEBUG_DLL}>
    -DAOO_DEBUG_RESAMPLER=$<BOOL:${AOO_DEBUG_RESAMPLER}>
    -DAOO_DEBUG_STREAM_MESSAGE=$<BOOL:${AOO_DEBUG_STREAM_MESSAGE}>
    -DAOO_DEBUG_JITTER_BUFFER=$<BOOL:${AOO_DEBUG_JITTER_BUFFER}>
    -DAOO_DEBUG_RELAY=$<BOOL:${AOO_DEBUG_RELAY}>
    -DAOO_DEBUG_CLIENT_MESSAGE=$<BOOL:${AOO_DEBUG_CLIENT_MESSAGE}>
    -DAOO_CLIENT_SIMULATE=$<BOOL:${AOO_CLIENT_SIMULATE}>
)

set(INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../include/aoo")
set(COMMON "${CMAKE_CURRENT_SOURCE_DIR}/../common")
set(DEPS "${CMAKE_CURRENT_SOURCE_DIR}/../deps")

file(GLOB HEADERS "${INCLUDE}/*.h" "${INCLUDE}/*.hpp"
     "${INCLUDE}/codec/aoo_pcm.h" "src/*.hpp" "${COMMON}/*.hpp")

file(GLOB SOURCES "src/*.cpp" "src/codec/null.cpp" "src/codec/pcm.cpp" "${COMMON}/*.cpp")

if (NOT AOO_SYSTEM_OSCPACK)
    # only build required files in "osc" folder
    set(OSC "${DEPS}/oscpack/osc")
    list(APPEND HEADERS
        "${OSC}/OscException.h"
        "${OSC}/OscHostEndianness.h"
        "${OSC}/OscOutboundPacketStream.h"
        "${OSC}/OscReceivedElements.h"
        "${OSC}/OscTypes.h")
    list(APPEND SOURCES
        "${OSC}/OscOutboundPacketStream.cpp"
        "${OSC}/OscReceivedElements.cpp"
        "${OSC}/OscTypes.cpp")
endif()

if (AOO_USE_CODEC_OPUS)
    list(APPEND HEADERS "${INCLUDE}/codec/aoo_opus.h")
    list(APPEND SOURCES "src/codec/opus.cpp")
endif()

if (AOO_NET)
    file(GLOB NET_HEADERS "src/net/*.hpp")

    file(GLOB NET_SOURCES "src/net/*.cpp")

    list(APPEND HEADERS ${NET_HEADERS})
    list(APPEND SOURCES ${NET_SOURCES})

    if (NOT AOO_SYSTEM_MD5)
        list(APPEND HEADERS "${DEPS}/md5/md5.h")
        list(APPEND SOURCES "${DEPS}/md5/md5.c")
    endif()
endif()

# AOO static library
# NOTE: always build static library because we need it for Pd, SC, tests, etc.
# AOO_BUILD_STATIC_LIBRARY only installs it
option(AOO_BUILD_STATIC_LIBRARY "build static AOO library" ON)

add_library(AOO_STATIC STATIC ${HEADERS} ${SOURCES})
if (NOT MSVC)
    # for MSVC we keep the "aoo_static" name to avoid name clash with
    # "aoo.lib" import library
    set_target_properties(AOO_STATIC PROPERTIES OUTPUT_NAME "aoo")
endif()

target_compile_definitions(AOO_STATIC PUBLIC ${OPTIONS} -DAOO_STATIC)
target_link_libraries(AOO_STATIC ${AOO_OSCPACK_LDFLAGS} ${AOO_OPUS_LDFLAGS})
if (AOO_NET)
    target_link_libraries(AOO_STATIC ${AOO_MD5_LDFLAGS})
endif()

# AOO shared library
option(AOO_BUILD_SHARED_LIBRARY "build shared AOO library" ON)

if (AOO_BUILD_SHARED_LIBRARY)
    add_library(AOO SHARED ${HEADERS} ${SOURCES})
    set_target_properties(AOO PROPERTIES OUTPUT_NAME "aoo")

    target_compile_definitions(AOO PUBLIC -DAOO_BUILD ${OPTIONS})
    if (WIN32)
        target_compile_definitions(AOO PUBLIC -DDLL_EXPORT)
        target_link_libraries(AOO "ws2_32")
    endif()
    target_link_libraries(AOO ${AOO_OPUS_LDFLAGS} ${LIBS})
endif()

if (AOO_BUILD_SHARED_LIBRARY OR AOO_BUILD_STATIC_LIBRARY)
    # install library files
    if (AOO_BUILD_SHARED_LIBRARY)
        list(APPEND TARGET_LIST AOO)
    endif()
    if (AOO_BUILD_STATIC_LIBRARY)
        list(APPEND TARGET_LIST AOO_STATIC)
    endif()
    install(TARGETS ${TARGET_LIST}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    # install public headers (note the trailing slash!)
    install(DIRECTORY "${INCLUDE}/"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/aoo")
endif()
