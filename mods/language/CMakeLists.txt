add_library(bruce_mod_language
    src/mod.cpp
    src/language.cpp
    src/language_run.cpp
    src/language_core_binding.cpp)
bruce_enable_sanitizers(bruce_mod_language)
target_include_directories(bruce_mod_language PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(bruce_mod_language PUBLIC bruce_mod_runtime)
target_compile_options(bruce_mod_language PRIVATE -Wall -Wextra -Werror)
