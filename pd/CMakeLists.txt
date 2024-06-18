# Pd directories:
if (NOT PD_DIR)
    if (WIN32)
        set(_pddir "${PROGRAMFILES}/Pd")
        if (NOT PD_INCLUDEDIR AND NOT (EXISTS "${_pddir}"))
            message(FATAL_ERROR "Could not find Pd installation - please set PD_INCLUDEDIR or PD_DIR")
        endif()
        set(PD_INCLUDEDIR "${_pddir}/src" CACHE PATH "Pd header files")
        set(PD_BINDIR "${_pddir}/bin" CACHE PATH "Pd executables")
    elseif(APPLE)
        file(GLOB _globresult "/Applications/Pd*.app")
        if (_globresult)
            list(GET _globresult 0 _pddir)
        endif()
        if (NOT PD_INCLUDEDIR AND NOT (EXISTS "${_pddir}"))
            message(FATAL_ERROR "Could not find Pd installation - please set PD_INCLUDEDIR")
        endif()
        set(PD_INCLUDEDIR "${_pddir}/Contents/Resources/src" CACHE PATH "Pd header files")
    else()
        set(PD_INCLUDEDIR "/usr/include/pd" CACHE PATH "Pd header files")
    endif()
else()
    set(PD_INCLUDEDIR "${PD_DIR}/src" CACHE PATH "Pd include directory" FORCE)
    if (WIN32)
        set(PD_BINDIR "${PD_DIR}/bin" CACHE PATH "Pd binary directory" FORCE)
    endif()
endif()

message(STATUS "Pd include directory: ${PD_INCLUDEDIR}")
if (WIN32)
    message(STATUS "Pd binary directory: ${PD_BINDIR}")
endif()

# install directory:
if (WIN32)
    set(PD_INSTALLDIR "${APPDATA}/Pd" CACHE PATH "Pd install directoy")
elseif(APPLE)
    set(PD_INSTALLDIR "~/Library/Pd" CACHE PATH "Pd install directory")
else()
    set(PD_INSTALLDIR "/usr/local/lib/pd-externals" CACHE PATH "Pd install directory")
endif()
message(STATUS "Pd install directory: ${PD_INSTALLDIR}")

# the Pd external
add_library(pd MODULE
    "src/aoo_client.cpp"
    "src/aoo_common.hpp"
    "src/aoo_common.cpp"
    "src/aoo_node.cpp"
    "src/aoo_receive~.cpp"
    "src/aoo_send~.cpp"
    "src/aoo_server.cpp"
    "src/aoo_setup.cpp")

target_include_directories(pd PRIVATE ${PD_INCLUDEDIR})

target_link_libraries(pd PRIVATE aoo_static aoo_common)

# multi-instance support
option(PD_MULTI_INSTANCE "Pd multi-instance support (for libpd)" OFF)
message(STATUS "Pd multi-instance support: ${PD_MULTI_INSTANCE}")
if (PD_MULTI_INSTANCE)
    target_compile_definitions(pd PRIVATE PDINSTANCE)
endif()

# plugin extension
set(PD_EXTENSION "" CACHE STRING "Pd plugin extension")
if (NOT PD_EXTENSION)
    # default extensions
    if (WIN32)
        set(SUFFIX ".dll")
    elseif(APPLE)
        set(SUFFIX ".pd_darwin")
    elseif(LINUX)
        set(SUFFIX ".pd_linux")
    else()
        set(SUFFIX "${CMAKE_SHARED_MODULE_SUFFIX}")
    endif()
else()
    set(SUFFIX ".${PD_EXTENSION}")
endif()
message(STATUS "Pd extension: ${SUFFIX}")

# output name
set_target_properties(pd PROPERTIES
    PREFIX "" # no prefix!
    OUTPUT_NAME "aoo"
    SUFFIX ${SUFFIX})

# platform specific definitions and options
if (WIN32)
    if (MINGW)
        target_link_options(pd PRIVATE --enable-auto-import)
        target_link_libraries(pd PRIVATE "${PD_BINDIR}/pd.dll")
    else()
        target_link_libraries(pd PRIVATE "${PD_BINDIR}/pd.lib")
    endif()
elseif(APPLE)
    target_link_options(pd PRIVATE -undefined suppress -flat_namespace -bundle)
else()
    target_link_options(pd PRIVATE -rdynamic -fPIC)
endif()

# installation
# TODO: install PDB file (Visual Studio)
install(TARGETS pd DESTINATION "${PD_INSTALLDIR}/aoo")
install(FILES "aoo_send~-help.pd" "aoo_receive~-help.pd"
    "aoo_client-help.pd" "aoo_server-help.pd"
    "mc_aoo_send~.pd" "mc_aoo_receive~.pd"
    "../README.md" "../LICENSE"
    DESTINATION "${PD_INSTALLDIR}/aoo")
