add_library(aoo_md5 OBJECT "md5.c" "md5.h")

target_link_libraries(aoo_md5 PRIVATE aoo_common)
