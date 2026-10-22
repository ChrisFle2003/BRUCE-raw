add_library(bruce_mod_nanocell
    src/nanocell.cpp
    src/mod.cpp)
bruce_enable_sanitizers(bruce_mod_nanocell)
target_include_directories(bruce_mod_nanocell PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(bruce_mod_nanocell PUBLIC bruce_mod_runtime)
target_compile_options(bruce_mod_nanocell PRIVATE -Wall -Wextra -Werror)

add_library(bruce_phase_dag_nanocell STATIC
    src/phase_dag_adapter_v2.cpp
    src/phase_dag_adapter_v3.cpp)
bruce_enable_sanitizers(bruce_phase_dag_nanocell)
target_include_directories(bruce_phase_dag_nanocell PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(bruce_phase_dag_nanocell PUBLIC bruce_mod_nanocell bruce_core)
target_compile_options(bruce_phase_dag_nanocell PRIVATE -Wall -Wextra -Werror)

# The root enables CTest after loading modules; its default is BUILD_TESTING=ON.
if(NOT DEFINED BUILD_TESTING OR BUILD_TESTING)
    enable_testing()
    add_executable(bruce_phase_dag_payload_v3_tests
        ${CMAKE_SOURCE_DIR}/tests/mods/nanocell/phase_dag_payload_v3_tests.cpp)
    bruce_enable_sanitizers(bruce_phase_dag_payload_v3_tests)
    target_link_libraries(bruce_phase_dag_payload_v3_tests PRIVATE bruce_phase_dag_nanocell)
    target_compile_options(bruce_phase_dag_payload_v3_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME phase_dag_payload_v3_contracts COMMAND bruce_phase_dag_payload_v3_tests)
    bruce_classify_sanitizer_tests()
endif()
