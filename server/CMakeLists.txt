add_executable(aooserver "main.cpp")

if (BUILD_SHARED_LIBS)
    target_sources(aooserver PRIVATE
        "../common/net_utils.cpp"
        "../common/net_utils.hpp"
        "../common/sync.cpp"
        "../common/sync.hpp")

    if (WIN32)
        target_link_libraries(aooserver PRIVATE ws2_32)
    endif()
endif()

target_link_libraries(aooserver PRIVATE aoo aoo_common aoo_binary)

# TODO: install PDB file (Visual Studio)
install(TARGETS aooserver)
