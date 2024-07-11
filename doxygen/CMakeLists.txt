find_package(Doxygen)

if (NOT DOXYGEN_FOUND)
     message(FATAL_ERROR "Doxygen is needed to build the documentation.")
endif()

# Because want to run doxygen in the current build directory, we need to
# provide absolute paths to the INPUT property and therefore generate the
# Doxyfile with configure_file().

# doxygen input paths must be separated by whitespace!
set(DOXYGEN_INPUT
    "${CMAKE_CURRENT_SOURCE_DIR}/../README.md \
${CMAKE_CURRENT_SOURCE_DIR}/../INSTALL.md \
${CMAKE_CURRENT_SOURCE_DIR}/../doc/aoo_protocol.md \
${CMAKE_CURRENT_SOURCE_DIR}/../include \
${CMAKE_CURRENT_SOURCE_DIR}/../include/codec"
)

set(MAIN_PAGE "${CMAKE_CURRENT_SOURCE_DIR}/../README.md")

set(doxyfile "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile")

configure_file("Doxyfile.in" ${doxyfile} @ONLY)

file(GLOB _headers "../include/*.h" "../include/*.hpp" "../include/codec/*.h")

# This is the actual command that runs doxygen. We add the input files as
# dependencies, so the docs are only built if any files have changed or
# index.html does not exist yet.
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/html/index.html"
    COMMAND ${DOXYGEN_EXECUTABLE} ${doxyfile}
    COMMENT "Generate API documentation with Doxygen"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS "${doxyfile}" "../README.md" "../INSTALL.md"
            "../doc/aoo_protocol.md" ${_headers}
    VERBATIM
)

# The actual build target which depends on the command output above.
add_custom_target(doxygen ALL
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/html/index.html"
)

install(DIRECTORY
    "${CMAKE_CURRENT_BINARY_DIR}/html"
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)
