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

# Prefer pkg-config over find_library() because it is more likely
# to be found. Last time I've checked, many distros only provide
# Opus pkg-config modules.

find_package(PkgConfig QUIET)

if (PkgConfig_FOUND)
    pkg_check_modules(Opus IMPORTED_TARGET opus)
    if (Opus_FOUND)
        add_library(Opus::opus INTERFACE IMPORTED)
        target_link_libraries(Opus::opus INTERFACE PkgConfig::Opus)
    endif()
    return()
endif()

# CMake package
find_package(Opus REQUIRED)
