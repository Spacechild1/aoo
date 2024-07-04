find_package(Doxygen)

if (NOT DOXYGEN_FOUND)
     message(FATAL_ERROR "Doxygen is needed to build the documentation.")
endif()

add_custom_target(doc ALL
      COMMAND ${DOXYGEN_EXECUTABLE} "doxygen/Doxyfile"
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
      COMMENT "Generating API documentation with Doxygen"
      VERBATIM
)

install(DIRECTORY "html" "latex" OPTIONAL
    DESTINATION ${CMAKE_INSTALL_DOCDIR})
