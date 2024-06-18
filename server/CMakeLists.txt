add_executable(aooserver "main.cpp")
target_link_libraries(aooserver PRIVATE aoo_static aoo_common)

# TODO: install PDB file (Visual Studio)
install(TARGETS aooserver)
