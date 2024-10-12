# SuperCollider API:
set(SC_INCLUDE_DIR "" CACHE PATH "SC include directory")
message(STATUS "SC include directory: ${SC_INCLUDE_DIR}")

# install directory
if (WIN32)
    set(SC_INSTALL_DIR "${LOCALAPPDATA}/SuperCollider/Extensions/" CACHE PATH "SC install directory")
elseif(APPLE)
    set(SC_INSTALL_DIR "~/Library/Application Support/SuperCollider/Extensions/" CACHE PATH "SC install directoy")
else()
    set(SC_INSTALL_DIR "~/.local/share/SuperCollider/Extensions/" CACHE PATH "SC install directoy")
endif()
message(STATUS "SC install directory: ${SC_INSTALL_DIR}")

option(SC_SUPERNOVA "Build Supernova plugins" OFF)
message(STATUS "Supernova: ${SC_SUPERNOVA}")

set(sc_sources
    "src/Aoo.cpp" "src/Aoo.hpp"
    "src/AooClient.cpp" "src/AooClient.hpp"
    "src/AooNode.cpp" "src/AooNode.hpp"
    "src/AooReceive.cpp" "src/AooReceive.hpp"
    "src/AooSend.cpp" "src/AooSend.hpp"
    "src/AooServer.cpp" "src/AooServer.hpp"
    "src/rt_shared_ptr.hpp"
)

if (BUILD_SHARED_LIBS)
    list(APPEND sc_sources
        "../common/net_utils.cpp"
        "../common/net_utils.hpp"
        "../common/log.hpp"
        "../common/sync.cpp"
        "../common/sync.hpp"
        "../common/time.cpp"
        "../common/time.hpp"
    )
endif()

set(sc_include
    "${SC_INCLUDE_DIR}/include/plugin_interface"
    "${SC_INCLUDE_DIR}/include/common"
    "${SC_INCLUDE_DIR}/plugin_interface"
    "${SC_INCLUDE_DIR}/common"
)

# plugin extension
if (WIN32 OR APPLE)
    set(SUFFIX ".scx")
else()
    set(SUFFIX "${CMAKE_SHARED_MODULE_SUFFIX}")
endif()

# the SC extension
add_library(sc MODULE ${sc_sources})

target_include_directories(sc PRIVATE ${sc_include})

target_link_libraries(sc PRIVATE aoo aoo_common aoo_binary)

# output name
set_target_properties(sc PROPERTIES
    PREFIX "" # no prefix!
    OUTPUT_NAME "aoo"
    SUFFIX ${SUFFIX}
)

# the Supernova extension
if (SC_SUPERNOVA)
    add_library(supernova MODULE ${sc_sources})

    target_include_directories(supernova PRIVATE ${sc_include})

    target_link_libraries(supernova PRIVATE aoo aoo_common aoo_binary)

    # output name
    set_target_properties(supernova PROPERTIES
        PREFIX "" # no prefix!
        OUTPUT_NAME "aoo_supernova"
        SUFFIX ${SUFFIX}
    )
endif()

# platform specific definitions and options
if (WIN32 AND BUILD_SHARED_LIBS)
    target_link_libraries(sc PRIVATE ws2_32)
    if (SC_SUPERNOVA)
        target_link_libraries(supernova PRIVATE ws2_32)
    endif()
endif()

# installation
install(TARGETS sc DESTINATION "${SC_INSTALL_DIR}/aoo/plugins")
if (SC_SUPERNOVA)
    install(TARGETS supernova DESTINATION "${SC_INSTALL_DIR}/aoo/plugins")
endif()

install(DIRECTORY "classes" DESTINATION "${SC_INSTALL_DIR}/aoo")

install(DIRECTORY "HelpSource" DESTINATION "${SC_INSTALL_DIR}/aoo")

install(FILES "../README.md" "../LICENSE" DESTINATION "${SC_INSTALL_DIR}/aoo")
