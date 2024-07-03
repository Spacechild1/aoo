#--------------------------------------------------------------
# FindOpus
#--------------------------------------------------------------
#
# Finds the Opus library
#
# Imported Targets:
#
# Opus::opus - the Opus library
#
#---------------------------------------------------------------

# Prefer cmake config over pkg-config because it is more portable.
# In practice, many distros only provide a pkg-config file...

find_package(PkgConfig QUIET)

# first try cmake config
find_package(Opus CONFIG QUIET)
# FIXME: for some reason, find_package() does not set Opus_FOUND to 1
# if called via find_dependency() in AooConfig.cmake - but only with MSVC!
# Here's a workaround:
if (OPUS_FOUND)
    set(Opus_FOUND 1)
endif()

if (NOT Opus_FOUND)
    # then try pkg-config
    if (PkgConfig_FOUND)
        pkg_check_modules(Opus IMPORTED_TARGET opus)
        if (Opus_FOUND)
            add_library(Opus::opus INTERFACE IMPORTED)
            target_link_libraries(Opus::opus INTERFACE PkgConfig::Opus)
        endif()
    endif()
endif()

if (NOT Opus_FOUND)
    # TODO: fall back to find_path() and find_library()

    set(Opus_NOT_FOUND_MESSAGE "Could not find Opus! Set Opus_DIR or make sure that opus.pc can be found.")
    if (Opus_FIND_REQUIRED)
        message(FATAL_ERROR ${Opus_NOT_FOUND_MESSAGE})
    else()
        message(STATUS ${Opus_NOT_FOUND_MESSAGE})
    endif()
endif()
