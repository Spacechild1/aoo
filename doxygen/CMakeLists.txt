find_package(Doxygen)

if (NOT DOXYGEN_FOUND)
     message(FATAL_ERROR "Doxygen is needed to build the documentation.")
endif()

# Because want to run doxygen in the current build directory, we need to
# provide absolute paths to the INPUT property and therefore generate the
# Doxyfile with configure_file().

# items must be separated by whitespace!
set(DOXYGEN_INPUT
    "${CMAKE_CURRENT_SOURCE_DIR}/../README.md \
${CMAKE_CURRENT_SOURCE_DIR}/../INSTALL.md \
${CMAKE_CURRENT_SOURCE_DIR}/../include \
${CMAKE_CURRENT_SOURCE_DIR}/../include/codec"
)

set(doxyfile "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile")

configure_file("Doxyfile.in" ${doxyfile} @ONLY)

# TODO: only build target when necessary
add_custom_target(doxygen ALL
    COMMAND ${DOXYGEN_EXECUTABLE} ${doxyfile}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating API documentation with Doxygen"
    VERBATIM
)

install(DIRECTORY
    "${CMAKE_CURRENT_BINARY_DIR}/html"
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)
