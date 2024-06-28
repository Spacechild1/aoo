set(STATIC_LIBAOO TRUE)
if (STATIC_LIBAOO)
    set(test_libs aoo aoo_common)
else()
    set(test_libs aoo aoo_common)
endif()

# C interface test
add_executable(test_c_interface "test_c_interface.c")
if (NOT MSVC)
    set(TEST_C_STANDARD c89)
    # C89 does not have "inline" and GCC/Clang warn about unused static non-inline functions
    target_compile_options(test_c_interface PUBLIC -Wno-unused-function)
    # force C standard
    target_compile_options(test_c_interface PUBLIC -std=${TEST_C_STANDARD} -pedantic)
endif()
target_link_libraries(test_c_interface PRIVATE ${test_libs})

# RT memory pool test
add_executable(test_rt_memory_pool "test_rt_memory_pool.cpp")
target_link_libraries(test_rt_memory_pool PRIVATE ${test_libs})

# relay test
add_executable(test_relay "test_relay.cpp")
target_link_libraries(test_relay PRIVATE ${test_libs})
