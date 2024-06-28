add_executable(aooserver "main.cpp")
target_link_libraries(aooserver PRIVATE aoo aoo_common aoo_binary)

# TODO: install PDB file (Visual Studio)
install(TARGETS aooserver)
