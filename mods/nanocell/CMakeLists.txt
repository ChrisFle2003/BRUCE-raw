add_library(bruce_mod_nanocell
    src/nanocell.cpp
    src/mod.cpp)
target_include_directories(bruce_mod_nanocell PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(bruce_mod_nanocell PUBLIC bruce_mod_runtime)
target_compile_options(bruce_mod_nanocell PRIVATE -Wall -Wextra -Werror)
