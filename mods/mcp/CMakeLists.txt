find_package(Threads REQUIRED)

add_library(bruce_mod_mcp STATIC src/mod.cpp src/mcp_inspector.cpp
    src/mcp_inspector_server.cpp src/mcp_inspector_assets.cpp)
bruce_enable_sanitizers(bruce_mod_mcp)
target_include_directories(bruce_mod_mcp
    PUBLIC ${PROJECT_SOURCE_DIR}/include
    PRIVATE ${PROJECT_SOURCE_DIR}/third_party/cpp-httplib)
target_link_libraries(bruce_mod_mcp
    PUBLIC bruce_mod_runtime
    PRIVATE bruce_phase_dag_nanocell Threads::Threads)
target_compile_options(bruce_mod_mcp PRIVATE -Wall -Wextra -Werror)
