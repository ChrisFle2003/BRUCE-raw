cmake_minimum_required(VERSION 3.20)
project(BRUCE VERSION 0.102.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(cmake/BruceSanitizers.cmake)
find_package(OpenSSL REQUIRED COMPONENTS Crypto SSL)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GMPXX REQUIRED IMPORTED_TARGET gmpxx)
option(BRUCE_BUILD_MOD_LANGUAGE "Build the built-in deterministic Language-Mod" ON)
option(BRUCE_BUILD_MOD_NANOCELL "Build the built-in deterministic NanoCell-Mod" ON)
option(BRUCE_BUILD_MOD_MCP "Build the built-in Mcp inspector boundary" ON)
if(BRUCE_BUILD_MOD_MCP AND NOT BRUCE_BUILD_MOD_NANOCELL)
    message(FATAL_ERROR "McpRequiresNanoCell")
endif()
add_library(bruce_core
    src/application/common_runtime_host_v1.cpp
    src/application/common_runtime_host_v2.cpp
    src/application/common_runtime_host_v2_batch.cpp
    src/application/common_runtime_compatibility_v1.cpp
    src/application/rule_history.cpp
    src/application/rule_proposal.cpp
    src/core/json.cpp
    src/application/experience_source.cpp
    src/application/experience_routing.cpp
    src/application/compatibility_transfer.cpp
    src/application/experience_transfer.cpp
    src/application/experience_task.cpp
    src/application/session_budget.cpp
    src/application/execution_session.cpp
    src/application/application_gates.cpp
    src/application/application_replay.cpp
    src/application/application_task.cpp
    src/application/research_contract.cpp
    src/application/research_gate_render.cpp
    src/application/research_gates.cpp
    src/application/grid_research.cpp
    src/application/grid_spec.cpp
    src/application/grid_resume.cpp
    src/application/grid_batch.cpp
    src/application/grid.cpp
    src/application/grid_stage_sweep.cpp
    src/application/grid_stage_verify.cpp
    src/application/portfolio_version.cpp
    src/application/fraction_compare.cpp
    src/coordinate/address.cpp
    src/compatibility/selection.cpp
    src/compatibility/snapshot.cpp
    src/math/domain_cache.cpp
    src/math/expression_normal_form.cpp
    src/math/expression.cpp
    src/application/expression_research.cpp
    src/math/exact.cpp
    src/math/numbers.cpp
    src/math/power_affine.cpp
    src/math/rational_linear.cpp
    src/application/rational_linear_research.cpp
    src/math/polynomial.cpp
    src/application/research.cpp
    src/application/replay.cpp
    src/application/compatibility.cpp
    src/application/exact_research.cpp
    src/application/exact_planner.cpp
    src/application/polynomial_research.cpp
    src/application/portfolio_order.cpp
    src/application/portfolio_normalization.cpp
    src/application/portfolio_research.cpp
    src/application/exact_replay.cpp
    src/experience/store.cpp
    src/evidence/bundle.cpp
    src/evidence/chain.cpp
    src/dag_scheduler/dag_scheduler_v1.cpp
    src/dag_scheduler/dag_scheduler_v2.cpp
    src/dag_scheduler/dag_scheduler_v4.cpp
    src/phase_dag/phase_dag.cpp
    src/phase_dag/phase_dag_v2.cpp
    src/phase_dag/phase_dag_v3.cpp
    src/signed_affine_dag/signed_affine_dag_v1.cpp
    src/signed_affine_relation/signed_affine_relation_v1.cpp
    src/toy_relation/toy_relation_v1.cpp
    src/token/task_spec.cpp
    src/tree/registry.cpp
    src/tree/normalizer.cpp
    src/mod/runtime_package.cpp
    src/mod/runtime_trust_store_v2.cpp
    src/mod/package_inspection_v2.cpp
    src/mod/logic_mod_plugin_v1.cpp
    src/mod/logic_mod_worker_v1.cpp
    src/mod/logic_mod_worker_v1_internal.cpp
    src/mod/logic_mod_worker_attempt_v6.cpp
    src/mod/logic_mod_supervision_log_v6.cpp
    src/mod/logic_mod_payload_v1.cpp
    src/mod/logic_mod_result_artifact_v1.cpp
    src/mod/logic_mod_artifact_dag_v2.cpp
    src/mod/logic_mod_artifact_dag_v3.cpp
    src/mod/logic_mod_artifact_dag_v4.cpp
    src/mod/logic_mod_artifact_dag_v5.cpp
    src/mod/logic_mod_artifact_dag_v6.cpp
    src/mod/logic_mod_artifact_dag_v6_plan.cpp
    src/runtime/runtime_orchestrator_v1.cpp
    src/runtime/admission_diagnostics_v1.cpp
    src/observability/controller_observability_crosswalk_v1.cpp
    src/observability/controller_observability_cli_v1.cpp
    src/mod/logic_mod_artifact_chain_v1.cpp
    src/mod/logic_mod_result_inspector_v1.cpp
    src/mod/runtime_registry.cpp
    src/mod/runtime_mod_store_internal.cpp
    src/mod/logic_mod_registry_v1.cpp
    src/mod/installed_mod_host_v2.cpp
    src/mod/local_builder.cpp
    src/release/release_signature_v1.cpp
    src/application/tokenized_task_run.cpp
    src/io/local_files.cpp
    src/artifact/artifact_manifest_v1.cpp
    src/artifact/artifact_store_v1.cpp
    src/artifact/artifact_store_v2.cpp
    src/artifact/artifact_store_v3.cpp
    src/artifact/artifact_store_v4.cpp
    src/artifact/artifact_payload_v1.cpp
    src/artifact/artifact_payload_external_v1.cpp
    src/artifact/artifact_payload_stream_v1.cpp
    src/artifact/v5_manifest_id_v1.cpp
    src/artifact/payload_flow_v2.cpp
    src/artifact/execute_handoff_lineage_v1.cpp
    src/artifact/execute_handoff_inspector_v1.cpp
    src/artifact/payload_run_inspector_v1.cpp
    src/artifact/payload_flow_inspector_v2.cpp
    src/artifact/payload_run_stream_inspector_v1.cpp)
bruce_enable_sanitizers(bruce_core)
target_sources(bruce_core PRIVATE src/worker/worker_boundary_v1.cpp src/worker/worker_run_log_v1.cpp src/worker/worker_supervisor_v1.cpp src/worker/worker_run_inspector_v1.cpp src/worker/worker_abort_control_v1.cpp)
target_include_directories(bruce_core PUBLIC include)
target_link_libraries(bruce_core PRIVATE OpenSSL::Crypto OpenSSL::SSL)
target_link_libraries(bruce_core PUBLIC PkgConfig::GMPXX Threads::Threads)
target_compile_options(bruce_core PRIVATE -Wall -Wextra -Werror)

add_library(bruce_mod_runtime src/mod/mod_api.cpp)
bruce_enable_sanitizers(bruce_mod_runtime)
target_include_directories(bruce_mod_runtime PUBLIC include)
target_link_libraries(bruce_mod_runtime PUBLIC bruce_core)
target_compile_options(bruce_mod_runtime PRIVATE -Wall -Wextra -Werror)

if(BRUCE_BUILD_MOD_LANGUAGE)
    add_subdirectory(mods/language)
endif()
if(BRUCE_BUILD_MOD_NANOCELL)
    add_subdirectory(mods/nanocell)
endif()

if(BRUCE_BUILD_MOD_MCP)
    add_subdirectory(mods/mcp)
endif()

add_library(bruce_mod_catalog src/mod/catalog.cpp src/mod/unified_catalog_v2.cpp)
bruce_enable_sanitizers(bruce_mod_catalog)
target_include_directories(bruce_mod_catalog PUBLIC include)
target_link_libraries(bruce_mod_catalog PUBLIC bruce_mod_runtime)
if(BRUCE_BUILD_MOD_LANGUAGE)
    target_compile_definitions(bruce_mod_catalog PRIVATE BRUCE_BUILD_MOD_LANGUAGE=1)
    target_link_libraries(bruce_mod_catalog PRIVATE bruce_mod_language)
else()
    target_compile_definitions(bruce_mod_catalog PRIVATE BRUCE_BUILD_MOD_LANGUAGE=0)
endif()
if(BRUCE_BUILD_MOD_NANOCELL)
    target_compile_definitions(bruce_mod_catalog PRIVATE BRUCE_BUILD_MOD_NANOCELL=1)
    target_link_libraries(bruce_mod_catalog PRIVATE bruce_mod_nanocell)
else()
    target_compile_definitions(bruce_mod_catalog PRIVATE BRUCE_BUILD_MOD_NANOCELL=0)
endif()
if(BRUCE_BUILD_MOD_MCP)
    target_compile_definitions(bruce_mod_catalog PRIVATE BRUCE_BUILD_MOD_MCP=1)
    target_link_libraries(bruce_mod_catalog PRIVATE bruce_mod_mcp)
else()
    target_compile_definitions(bruce_mod_catalog PRIVATE BRUCE_BUILD_MOD_MCP=0)
endif()
target_compile_options(bruce_mod_catalog PRIVATE -Wall -Wextra -Werror)
# A single source of truth for runtime and installed artifact identity.
configure_file(cmake/version.hpp.in generated/bruce/version.hpp @ONLY)
file(GLOB_RECURSE BRUCE_SOURCE_INPUTS CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "include/*.hpp" "src/*.cpp" "src/*.hpp" "mods/*.cpp" "mods/CMakeLists.txt" "apps/*.cpp" "apps/*.hpp" "cmake/*"
    "scripts/*.py" "tests/*.cpp" "tests/*.py" "tests/fixtures/*")
list(APPEND BRUCE_SOURCE_INPUTS CMakeLists.txt DEPENDENCIES.md LICENSE NOTICE THIRD_PARTY_NOTICES.md
    third_party/cpp-httplib/httplib.h third_party/cpp-httplib/LICENSE
    mods/mcp/src/mcp_inspector_server.hpp
    mods/mcp/src/mcp_inspector.hpp
    mods/mcp/src/mcp_inspector_assets.hpp)
list(SORT BRUCE_SOURCE_INPUTS)
set(BRUCE_SOURCE_INDEX "")
foreach(input IN LISTS BRUCE_SOURCE_INPUTS)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${input}")
    file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/${input}" input_hash)
    string(APPEND BRUCE_SOURCE_INDEX "${input_hash}  ${input}\n")
endforeach()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/source-files.sha256" "${BRUCE_SOURCE_INDEX}")
file(SHA256 "${CMAKE_CURRENT_BINARY_DIR}/source-files.sha256" BRUCE_SOURCE_INDEX_SHA256)
configure_file(cmake/build-info.json.in build-info.json.in @ONLY)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/metadata/$<CONFIG>/build-info.json"
    INPUT "${CMAKE_CURRENT_BINARY_DIR}/build-info.json.in")
add_executable(bruce apps/bruce_cli.cpp)
bruce_enable_sanitizers(bruce)
add_executable(bruce-mod-builder apps/local_mod_builder.cpp)
add_executable(bruce-release-tool apps/bruce_release_tool.cpp)
bruce_enable_sanitizers(bruce-release-tool)
target_link_libraries(bruce-release-tool PRIVATE bruce_core OpenSSL::Crypto)
target_compile_options(bruce-release-tool PRIVATE -Wall -Wextra -Werror)
bruce_enable_sanitizers(bruce-mod-builder)
target_link_libraries(bruce-mod-builder PRIVATE bruce_core)
target_compile_options(bruce-mod-builder PRIVATE -Wall -Wextra -Werror)
target_include_directories(bruce PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(bruce PRIVATE bruce_core bruce_mod_catalog)
if(BRUCE_BUILD_MOD_LANGUAGE)
    target_compile_definitions(bruce PRIVATE BRUCE_BUILD_MOD_LANGUAGE=1)
else()
    target_compile_definitions(bruce PRIVATE BRUCE_BUILD_MOD_LANGUAGE=0)
endif()
if(BRUCE_BUILD_MOD_NANOCELL)
    target_compile_definitions(bruce PRIVATE BRUCE_BUILD_MOD_NANOCELL=1)
    target_link_libraries(bruce PRIVATE bruce_phase_dag_nanocell)
else()
    target_compile_definitions(bruce PRIVATE BRUCE_BUILD_MOD_NANOCELL=0)
endif()
if(BRUCE_BUILD_MOD_MCP)
    target_compile_definitions(bruce PRIVATE BRUCE_BUILD_MOD_MCP=1)
    target_link_libraries(bruce PRIVATE bruce_mod_mcp)
else()
    target_compile_definitions(bruce PRIVATE BRUCE_BUILD_MOD_MCP=0)
endif()
target_compile_options(bruce PRIVATE -Wall -Wextra -Werror)
include(CTest)
if(BUILD_TESTING)
    add_executable(bruce_release_signature_v1_tests tests/unit/release_signature_v1_tests.cpp)
    bruce_enable_sanitizers(bruce_release_signature_v1_tests)
    target_link_libraries(bruce_release_signature_v1_tests PRIVATE bruce_core)
    target_compile_options(bruce_release_signature_v1_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME release_signature_v1_contracts COMMAND bruce_release_signature_v1_tests)
    add_executable(bruce-mod-builder-report-fault
        apps/local_mod_builder.cpp tests/integration/local_builder_report_fault.cpp)
    bruce_enable_sanitizers(bruce-mod-builder-report-fault)
    target_link_libraries(bruce-mod-builder-report-fault PRIVATE bruce_core)
    target_compile_definitions(bruce-mod-builder-report-fault PRIVATE
        json_string=json_string_report_fault)
    target_compile_options(bruce-mod-builder-report-fault PRIVATE -Wall -Wextra -Werror)
    add_executable(bruce_common_runtime_host_v2_tests tests/unit/common_runtime_host_v2_tests.cpp)
    bruce_enable_sanitizers(bruce_common_runtime_host_v2_tests)
    target_link_libraries(bruce_common_runtime_host_v2_tests PRIVATE bruce_core)
    target_compile_options(bruce_common_runtime_host_v2_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME common_runtime_host_v2_contracts COMMAND bruce_common_runtime_host_v2_tests)
    add_executable(bruce_common_runtime_host_v2_batch_tests tests/unit/common_runtime_host_v2_batch_tests.cpp)
    bruce_enable_sanitizers(bruce_common_runtime_host_v2_batch_tests)
    target_link_libraries(bruce_common_runtime_host_v2_batch_tests PRIVATE bruce_core)
    target_compile_options(bruce_common_runtime_host_v2_batch_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME common_runtime_host_v2_batch_contracts COMMAND bruce_common_runtime_host_v2_batch_tests)
    add_executable(bruce_common_runtime_host_v1_tests tests/unit/common_runtime_host_v1_tests.cpp)
    bruce_enable_sanitizers(bruce_common_runtime_host_v1_tests)
    target_link_libraries(bruce_common_runtime_host_v1_tests PRIVATE bruce_core)
    target_compile_options(bruce_common_runtime_host_v1_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME common_runtime_host_v1_contracts COMMAND bruce_common_runtime_host_v1_tests)
    add_executable(bruce_common_runtime_compatibility_v1_tests tests/unit/common_runtime_compatibility_v1_tests.cpp)
    bruce_enable_sanitizers(bruce_common_runtime_compatibility_v1_tests)
    target_link_libraries(bruce_common_runtime_compatibility_v1_tests PRIVATE bruce_core)
    target_compile_options(bruce_common_runtime_compatibility_v1_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME common_runtime_compatibility_v1_contracts COMMAND bruce_common_runtime_compatibility_v1_tests)
    add_executable(bruce_common_runtime_matrix_binding_tests tests/unit/common_runtime_matrix_binding_tests.cpp)
    bruce_enable_sanitizers(bruce_common_runtime_matrix_binding_tests)
    target_link_libraries(bruce_common_runtime_matrix_binding_tests PRIVATE bruce_core)
    target_compile_options(bruce_common_runtime_matrix_binding_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME common_runtime_matrix_binding COMMAND bruce_common_runtime_matrix_binding_tests)
    add_executable(bruce_artifact_store_v1_tests tests/unit/artifact_store_v1_tests.cpp)
    bruce_enable_sanitizers(bruce_artifact_store_v1_tests)
    target_link_libraries(bruce_artifact_store_v1_tests PRIVATE bruce_core)
    target_compile_options(bruce_artifact_store_v1_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME artifact_store_v1_contracts COMMAND bruce_artifact_store_v1_tests)

    add_executable(bruce_artifact_store_v2_tests tests/unit/artifact_store_v2_tests.cpp)
    bruce_enable_sanitizers(bruce_artifact_store_v2_tests)
    target_link_libraries(bruce_artifact_store_v2_tests PRIVATE bruce_core)
    target_compile_options(bruce_artifact_store_v2_tests PRIVATE -Wall -Wextra -Werror)
    target_link_options(bruce_artifact_store_v2_tests PRIVATE -Wl,--wrap=fsync)
    add_test(NAME artifact_store_v2_contracts COMMAND bruce_artifact_store_v2_tests)
    set_tests_properties(artifact_store_v2_contracts PROPERTIES TIMEOUT 90)
    add_executable(bruce_artifact_store_v4_tests tests/unit/artifact_store_v4_tests.cpp)
    bruce_enable_sanitizers(bruce_artifact_store_v4_tests)
    target_link_libraries(bruce_artifact_store_v4_tests PRIVATE bruce_core)
    target_compile_options(bruce_artifact_store_v4_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME artifact_store_v4_contracts COMMAND bruce_artifact_store_v4_tests)
    set_tests_properties(artifact_store_v4_contracts PROPERTIES TIMEOUT 90)

    add_executable(bruce_artifact_store_v3_tests tests/unit/artifact_store_v3_tests.cpp)
    bruce_enable_sanitizers(bruce_artifact_store_v3_tests)
    target_link_libraries(bruce_artifact_store_v3_tests PRIVATE bruce_core)
    target_compile_options(bruce_artifact_store_v3_tests PRIVATE -Wall -Wextra -Werror)
    target_link_options(bruce_artifact_store_v3_tests PRIVATE -Wl,--wrap=fsync)
    add_test(NAME artifact_store_v3_contracts COMMAND bruce_artifact_store_v3_tests)
    set_tests_properties(artifact_store_v3_contracts PROPERTIES TIMEOUT 90)
    add_executable(bruce_artifact_payload_v1_tests tests/unit/artifact_payload_v1_tests.cpp)
    bruce_enable_sanitizers(bruce_artifact_payload_v1_tests)
    target_link_libraries(bruce_artifact_payload_v1_tests PRIVATE bruce_core)
    target_compile_options(bruce_artifact_payload_v1_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME artifact_payload_v1_contracts COMMAND bruce_artifact_payload_v1_tests)
    add_executable(bruce_artifact_payload_external_v1_tests tests/unit/artifact_payload_external_v1_tests.cpp)
    bruce_enable_sanitizers(bruce_artifact_payload_external_v1_tests)
    target_link_libraries(bruce_artifact_payload_external_v1_tests PRIVATE bruce_core)
    target_compile_options(bruce_artifact_payload_external_v1_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME artifact_payload_external_v1_contracts COMMAND bruce_artifact_payload_external_v1_tests)
    add_executable(bruce_artifact_payload_stream_v1_tests tests/unit/artifact_payload_stream_v1_tests.cpp)
    bruce_enable_sanitizers(bruce_artifact_payload_stream_v1_tests)
    target_link_libraries(bruce_artifact_payload_stream_v1_tests PRIVATE bruce_core)
    target_compile_options(bruce_artifact_payload_stream_v1_tests PRIVATE -Wall -Wextra -Werror)
    target_link_options(bruce_artifact_payload_stream_v1_tests PRIVATE -Wl,--wrap=read -Wl,--wrap=write)
    add_test(NAME artifact_payload_stream_v1_contracts COMMAND bruce_artifact_payload_stream_v1_tests)
    add_executable(bruce_payload_flow_v2_tests tests/unit/payload_flow_v2_tests.cpp)
    bruce_enable_sanitizers(bruce_payload_flow_v2_tests)
    target_link_libraries(bruce_payload_flow_v2_tests PRIVATE bruce_core)
    target_compile_options(bruce_payload_flow_v2_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME payload_flow_v2_contracts COMMAND bruce_payload_flow_v2_tests)
    add_executable(bruce_execute_handoff_lineage_v1_tests tests/unit/execute_handoff_lineage_v1_tests.cpp)
    bruce_enable_sanitizers(bruce_execute_handoff_lineage_v1_tests)
    target_link_libraries(bruce_execute_handoff_lineage_v1_tests PRIVATE bruce_core)
    target_compile_options(bruce_execute_handoff_lineage_v1_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME execute_handoff_lineage_v1_contracts COMMAND bruce_execute_handoff_lineage_v1_tests)
    add_executable(bruce_execute_handoff_inspector_v1_tests tests/unit/execute_handoff_inspector_v1_tests.cpp)
    bruce_enable_sanitizers(bruce_execute_handoff_inspector_v1_tests)
    target_link_libraries(bruce_execute_handoff_inspector_v1_tests PRIVATE bruce_core)
    target_compile_options(bruce_execute_handoff_inspector_v1_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME execute_handoff_inspector_v1_contracts COMMAND bruce_execute_handoff_inspector_v1_tests)
    add_executable(bruce_artifact_store_v1_cli_tests tests/unit/artifact_store_v1_cli_tests.cpp)
    bruce_enable_sanitizers(bruce_artifact_store_v1_cli_tests)
    target_link_libraries(bruce_artifact_store_v1_cli_tests PRIVATE bruce_core)
    target_compile_options(bruce_artifact_store_v1_cli_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME artifact_store_v1_cli_contracts COMMAND bruce_artifact_store_v1_cli_tests)
    add_executable(bruce_artifact_store_v2_cli_tests tests/unit/artifact_store_v2_cli_tests.cpp)
    bruce_enable_sanitizers(bruce_artifact_store_v2_cli_tests)
    target_link_libraries(bruce_artifact_store_v2_cli_tests PRIVATE bruce_core)
    target_compile_options(bruce_artifact_store_v2_cli_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME artifact_store_v2_cli_contracts COMMAND bruce_artifact_store_v2_cli_tests)
    add_executable(bruce_artifact_store_v3_cli_tests tests/unit/artifact_store_v3_cli_tests.cpp)
    bruce_enable_sanitizers(bruce_artifact_store_v3_cli_tests)
    target_link_libraries(bruce_artifact_store_v3_cli_tests PRIVATE bruce_core)
    target_compile_options(bruce_artifact_store_v3_cli_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME artifact_store_v3_cli_contracts COMMAND bruce_artifact_store_v3_cli_tests)
    add_executable(bruce_worker_boundary_v1_tests tests/unit/worker_boundary_v1_tests.cpp)
    bruce_enable_sanitizers(bruce_worker_boundary_v1_tests)
    target_link_libraries(bruce_worker_boundary_v1_tests PRIVATE bruce_core)
    target_compile_options(bruce_worker_boundary_v1_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME worker_boundary_v1_contracts COMMAND bruce_worker_boundary_v1_tests)
    add_executable(bruce_worker_run_log_v1_tests tests/unit/worker_run_log_v1_tests.cpp)
    bruce_enable_sanitizers(bruce_worker_run_log_v1_tests)
    target_link_libraries(bruce_worker_run_log_v1_tests PRIVATE bruce_core)
    target_compile_options(bruce_worker_run_log_v1_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME worker_run_log_v1_contracts COMMAND bruce_worker_run_log_v1_tests)
    add_executable(bruce_worker_supervisor_v1_tests tests/unit/worker_supervisor_v1_tests.cpp)
    bruce_enable_sanitizers(bruce_worker_supervisor_v1_tests)
    target_link_libraries(bruce_worker_supervisor_v1_tests PRIVATE bruce_core)
    target_compile_options(bruce_worker_supervisor_v1_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME worker_supervisor_v1_contracts COMMAND bruce_worker_supervisor_v1_tests)
    add_executable(bruce_payload_run_stream_inspector_v1_tests tests/unit/payload_run_stream_inspector_v1_tests.cpp)
    bruce_enable_sanitizers(bruce_payload_run_stream_inspector_v1_tests)
    target_link_libraries(bruce_payload_run_stream_inspector_v1_tests PRIVATE bruce_core)
    target_compile_options(bruce_payload_run_stream_inspector_v1_tests PRIVATE -Wall -Wextra -Werror)
    target_link_options(bruce_payload_run_stream_inspector_v1_tests PRIVATE -Wl,--wrap=read -Wl,--wrap=_ZNSt6chrono3_V212steady_clock3nowEv)
    add_test(NAME payload_run_stream_inspector_v1_contracts COMMAND bruce_payload_run_stream_inspector_v1_tests)
    add_executable(bruce_payload_run_inspector_v1_tests tests/unit/payload_run_inspector_v1_tests.cpp)
    bruce_enable_sanitizers(bruce_payload_run_inspector_v1_tests)
    target_link_libraries(bruce_payload_run_inspector_v1_tests PRIVATE bruce_core)
    target_compile_options(bruce_payload_run_inspector_v1_tests PRIVATE -Wall -Wextra -Werror)
    target_link_options(bruce_payload_run_inspector_v1_tests PRIVATE -Wl,--wrap=read -Wl,--wrap=_ZNSt6chrono3_V212steady_clock3nowEv)
    add_test(NAME payload_run_inspector_v1_contracts COMMAND bruce_payload_run_inspector_v1_tests)
    add_executable(bruce_payload_flow_inspector_v2_tests tests/unit/payload_flow_inspector_v2_tests.cpp)
    bruce_enable_sanitizers(bruce_payload_flow_inspector_v2_tests)
    target_link_libraries(bruce_payload_flow_inspector_v2_tests PRIVATE bruce_core)
    target_compile_options(bruce_payload_flow_inspector_v2_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME payload_flow_inspector_v2_contracts COMMAND bruce_payload_flow_inspector_v2_tests)
    add_executable(bruce_worker_run_inspector_v1_tests tests/unit/worker_run_inspector_v1_tests.cpp)
    bruce_enable_sanitizers(bruce_worker_run_inspector_v1_tests)
    target_link_libraries(bruce_worker_run_inspector_v1_tests PRIVATE bruce_core)
    target_compile_options(bruce_worker_run_inspector_v1_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME worker_run_inspector_v1_contracts COMMAND bruce_worker_run_inspector_v1_tests)
    add_executable(bruce_worker_abort_control_v1_tests tests/unit/worker_abort_control_v1_tests.cpp)
    bruce_enable_sanitizers(bruce_worker_abort_control_v1_tests)
    target_link_libraries(bruce_worker_abort_control_v1_tests PRIVATE bruce_core)
    target_compile_options(bruce_worker_abort_control_v1_tests PRIVATE -Wall -Wextra -Werror)
    target_link_options(bruce_worker_abort_control_v1_tests PRIVATE -Wl,--wrap=read -Wl,--wrap=write -Wl,--wrap=fsync)
    add_test(NAME worker_abort_control_v1_contracts COMMAND bruce_worker_abort_control_v1_tests)
    add_executable(bruce_local_builder_tests tests/unit/local_builder_tests.cpp)
    bruce_enable_sanitizers(bruce_local_builder_tests)
    target_link_libraries(bruce_local_builder_tests PRIVATE bruce_core)
    target_compile_definitions(bruce_local_builder_tests PRIVATE BRUCE_RUNTIME_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_mods")
    target_compile_options(bruce_local_builder_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME local_builder_contracts COMMAND bruce_local_builder_tests)
    add_executable(bruce_local_logic_builder_tests tests/unit/local_logic_builder_tests.cpp)
    bruce_enable_sanitizers(bruce_local_logic_builder_tests)
    target_link_libraries(bruce_local_logic_builder_tests PRIVATE bruce_core)
    target_compile_definitions(bruce_local_logic_builder_tests PRIVATE BRUCE_RUNTIME_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_mods")
    target_compile_options(bruce_local_logic_builder_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME local_logic_builder_contracts COMMAND bruce_local_logic_builder_tests)
    add_executable(bruce_runtime_registry_tests tests/unit/runtime_registry_tests.cpp)
    bruce_enable_sanitizers(bruce_runtime_registry_tests)
    target_link_libraries(bruce_runtime_registry_tests PRIVATE bruce_core OpenSSL::Crypto)
    target_compile_options(bruce_runtime_registry_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME runtime_registry_contracts COMMAND bruce_runtime_registry_tests)
    add_executable(bruce_runtime_trust_store_v2_tests tests/unit/runtime_trust_store_v2_tests.cpp)
    # A real pre-migration binary proves the V2 fence and journal barriers keep
    # both legacy registry implementations from opening or mutating a V2 root.
    add_executable(bruce_trust_v1_reader_fixture
        tests/fixtures/trust_v1/main.cpp
        tests/fixtures/trust_v1/runtime_registry.cpp
        tests/fixtures/trust_v1/logic_mod_registry_v1.cpp)
    target_include_directories(bruce_trust_v1_reader_fixture PRIVATE src/mod)
    target_compile_definitions(bruce_trust_v1_reader_fixture PRIVATE
        BRUCE_TRUST_V1_FIXTURE_API=1)
    target_link_libraries(bruce_trust_v1_reader_fixture PRIVATE bruce_core)
    bruce_enable_sanitizers(bruce_trust_v1_reader_fixture)
    add_dependencies(bruce_runtime_trust_store_v2_tests bruce_trust_v1_reader_fixture)
    target_compile_definitions(bruce_runtime_trust_store_v2_tests PRIVATE
        BRUCE_TRUST_V1_READER_FIXTURE="$<TARGET_FILE:bruce_trust_v1_reader_fixture>")
    bruce_enable_sanitizers(bruce_runtime_trust_store_v2_tests)
    target_link_libraries(bruce_runtime_trust_store_v2_tests PRIVATE bruce_core)
    target_compile_options(bruce_runtime_trust_store_v2_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME runtime_trust_store_v2_contracts COMMAND bruce_runtime_trust_store_v2_tests)
    list(APPEND _bruce_sanitizer_compatible runtime_trust_store_v2_contracts)
    add_executable(bruce_logic_mod_registry_v1_tests tests/unit/logic_mod_registry_v1_tests.cpp)
    bruce_enable_sanitizers(bruce_logic_mod_registry_v1_tests)
    target_link_libraries(bruce_logic_mod_registry_v1_tests PRIVATE bruce_core)
    target_compile_options(bruce_logic_mod_registry_v1_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME logic_mod_registry_v1_contracts COMMAND bruce_logic_mod_registry_v1_tests)
    add_executable(bruce_installed_mod_host_v2_tests tests/unit/installed_mod_host_v2_tests.cpp)
    bruce_enable_sanitizers(bruce_installed_mod_host_v2_tests)
    target_link_libraries(bruce_installed_mod_host_v2_tests PRIVATE bruce_core)
    target_compile_options(bruce_installed_mod_host_v2_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME installed_mod_host_v2_contracts COMMAND bruce_installed_mod_host_v2_tests)
    add_executable(bruce_mod_update_v2_tests tests/unit/mod_update_v2_tests.cpp)
    bruce_enable_sanitizers(bruce_mod_update_v2_tests)
    target_link_libraries(bruce_mod_update_v2_tests PRIVATE bruce_core)
    target_compile_definitions(bruce_mod_update_v2_tests PRIVATE BRUCE_RUNTIME_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_mods")
    target_compile_options(bruce_mod_update_v2_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME mod_update_v2_contracts COMMAND bruce_mod_update_v2_tests)
    add_executable(bruce_runtime_mod_cli_tests tests/unit/runtime_mod_cli_tests.cpp)
    bruce_enable_sanitizers(bruce_runtime_mod_cli_tests)
    add_executable(bruce_logic_mod_plugin_v1_tests tests/unit/logic_mod_plugin_v1_tests.cpp)
    bruce_enable_sanitizers(bruce_logic_mod_plugin_v1_tests)
    target_link_libraries(bruce_logic_mod_plugin_v1_tests PRIVATE bruce_core)
    target_compile_options(bruce_logic_mod_plugin_v1_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME logic_mod_plugin_v1_contracts COMMAND bruce_logic_mod_plugin_v1_tests)
    add_executable(logic_mod_result_artifact_v1_tests tests/unit/logic_mod_result_artifact_v1_tests.cpp)
    bruce_enable_sanitizers(logic_mod_result_artifact_v1_tests)
    target_link_libraries(logic_mod_result_artifact_v1_tests PRIVATE bruce_core)
    target_compile_options(logic_mod_result_artifact_v1_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME logic_mod_result_artifact_v1_contracts COMMAND logic_mod_result_artifact_v1_tests)
    add_executable(logic_mod_result_inspector_v1_tests tests/unit/logic_mod_result_inspector_v1_tests.cpp)
    bruce_enable_sanitizers(logic_mod_result_inspector_v1_tests)
    target_link_libraries(logic_mod_result_inspector_v1_tests PRIVATE bruce_core)
    target_compile_options(logic_mod_result_inspector_v1_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME logic_mod_result_inspector_v1_contracts COMMAND logic_mod_result_inspector_v1_tests)
    add_test(NAME logic_mod_result_inspector_record_fifo_v1_contracts COMMAND logic_mod_result_inspector_v1_tests --record-fifo)
    add_test(NAME logic_mod_result_inspector_record_symlink_fifo_v1_contracts COMMAND logic_mod_result_inspector_v1_tests --record-symlink-fifo)
    add_test(NAME logic_mod_result_inspector_record_symlink_regular_v1_contracts COMMAND logic_mod_result_inspector_v1_tests --record-symlink-regular)
    set_tests_properties(logic_mod_result_inspector_record_fifo_v1_contracts
        logic_mod_result_inspector_record_symlink_fifo_v1_contracts
        logic_mod_result_inspector_record_symlink_regular_v1_contracts PROPERTIES TIMEOUT 5)
    add_test(NAME logic_mod_result_artifact_fifo_preflight_v1_contracts COMMAND logic_mod_result_artifact_v1_tests --fifo-preflight)
    add_test(NAME logic_mod_result_artifact_fifo_swap_v1_contracts COMMAND logic_mod_result_artifact_v1_tests --fifo-swap)
    set_tests_properties(logic_mod_result_artifact_fifo_preflight_v1_contracts logic_mod_result_artifact_fifo_swap_v1_contracts PROPERTIES TIMEOUT 5)
    add_executable(bruce_logic_mod_payload_v1_tests tests/unit/logic_mod_payload_v1_tests.cpp)
    bruce_enable_sanitizers(bruce_logic_mod_payload_v1_tests)
    target_link_libraries(bruce_logic_mod_payload_v1_tests PRIVATE bruce_core)
    target_compile_options(bruce_logic_mod_payload_v1_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME logic_mod_payload_v1_contracts COMMAND bruce_logic_mod_payload_v1_tests)
    add_test(NAME logic_mod_payload_fifo_lock_v1_contracts COMMAND bruce_logic_mod_payload_v1_tests --fifo-lock)
    add_test(NAME logic_mod_payload_fifo_manifest_v1_contracts COMMAND bruce_logic_mod_payload_v1_tests --fifo-manifest)
    set_tests_properties(logic_mod_payload_fifo_lock_v1_contracts logic_mod_payload_fifo_manifest_v1_contracts PROPERTIES TIMEOUT 5)
    add_executable(bruce_logic_mod_worker_fixture tests/fixtures/logic_mod_worker_v1.cpp)
    target_compile_options(bruce_logic_mod_worker_fixture PRIVATE -Wall -Wextra -Werror)
    target_link_options(bruce_logic_mod_worker_fixture PRIVATE -static -s)
    add_dependencies(bruce_installed_mod_host_v2_tests bruce_logic_mod_worker_fixture)
    target_compile_definitions(bruce_installed_mod_host_v2_tests PRIVATE
        BRUCE_INSTALLED_LOGIC_WORKER="$<TARGET_FILE:bruce_logic_mod_worker_fixture>")
    add_executable(bruce_logic_mod_exec_escape_fixture tests/fixtures/logic_mod_worker_v1.cpp)
    target_compile_options(bruce_logic_mod_exec_escape_fixture PRIVATE -Wall -Wextra -Werror)
    target_compile_definitions(bruce_logic_mod_exec_escape_fixture PRIVATE BRUCE_LOGIC_EXEC_ESCAPE_PATH="$<TARGET_FILE:bruce_logic_mod_worker_fixture>")
    target_link_options(bruce_logic_mod_exec_escape_fixture PRIVATE -static -s)
    add_dependencies(bruce_logic_mod_exec_escape_fixture bruce_logic_mod_worker_fixture)
    add_executable(bruce_logic_mod_chain_worker_fixture tests/unit/logic_mod_artifact_chain_v1_tests.cpp)
    target_compile_definitions(bruce_logic_mod_chain_worker_fixture PRIVATE BRUCE_CHAIN_WORKER_FIXTURE=1)
    target_compile_options(bruce_logic_mod_chain_worker_fixture PRIVATE -Wall -Wextra -Werror)
    target_link_options(bruce_logic_mod_chain_worker_fixture PRIVATE -static -s)
    add_executable(bruce_logic_mod_dag_worker_fixture tests/unit/logic_mod_artifact_dag_v2_tests.cpp)
    target_compile_definitions(bruce_logic_mod_dag_worker_fixture PRIVATE BRUCE_CHAIN_WORKER_FIXTURE=1)
    target_compile_options(bruce_logic_mod_dag_worker_fixture PRIVATE -Wall -Wextra -Werror)
    target_link_options(bruce_logic_mod_dag_worker_fixture PRIVATE -static -s)
    add_executable(logic_mod_artifact_dag_v2_tests tests/unit/logic_mod_artifact_dag_v2_tests.cpp)
    bruce_enable_sanitizers(logic_mod_artifact_dag_v2_tests)
    target_link_libraries(logic_mod_artifact_dag_v2_tests PRIVATE bruce_core)
    target_compile_options(logic_mod_artifact_dag_v2_tests PRIVATE -Wall -Wextra -Werror)
    target_compile_definitions(logic_mod_artifact_dag_v2_tests PRIVATE BRUCE_DAG_WORKER="$<TARGET_FILE:bruce_logic_mod_dag_worker_fixture>")
    add_dependencies(logic_mod_artifact_dag_v2_tests bruce_logic_mod_dag_worker_fixture)
    add_test(NAME logic_mod_artifact_dag_v2_contracts COMMAND logic_mod_artifact_dag_v2_tests)
    add_test(NAME logic_mod_artifact_dag_v2_preflight_contracts COMMAND logic_mod_artifact_dag_v2_tests --preflight)
    set_tests_properties(logic_mod_artifact_dag_v2_contracts logic_mod_artifact_dag_v2_preflight_contracts PROPERTIES TIMEOUT 30)
    add_executable(bruce_logic_mod_dag_v3_worker_fixture tests/unit/logic_mod_artifact_dag_v3_tests.cpp)
    target_compile_definitions(bruce_logic_mod_dag_v3_worker_fixture PRIVATE BRUCE_DAG_V3_WORKER_FIXTURE=1)
    target_compile_options(bruce_logic_mod_dag_v3_worker_fixture PRIVATE -Wall -Wextra -Werror)
    target_link_options(bruce_logic_mod_dag_v3_worker_fixture PRIVATE -static -s)
    add_executable(logic_mod_artifact_dag_v3_tests tests/unit/logic_mod_artifact_dag_v3_tests.cpp)
    bruce_enable_sanitizers(logic_mod_artifact_dag_v3_tests)
    target_link_libraries(logic_mod_artifact_dag_v3_tests PRIVATE bruce_core)
    target_compile_options(logic_mod_artifact_dag_v3_tests PRIVATE -Wall -Wextra -Werror)
    target_compile_definitions(logic_mod_artifact_dag_v3_tests PRIVATE
        BRUCE_DAG_WORKER="$<TARGET_FILE:bruce_logic_mod_dag_worker_fixture>"
        BRUCE_DAG_V3_WORKER="$<TARGET_FILE:bruce_logic_mod_dag_v3_worker_fixture>")
    add_dependencies(logic_mod_artifact_dag_v3_tests bruce_logic_mod_dag_worker_fixture bruce_logic_mod_dag_v3_worker_fixture)
    add_test(NAME logic_mod_artifact_dag_v3_contracts COMMAND logic_mod_artifact_dag_v3_tests)
    set_tests_properties(logic_mod_artifact_dag_v3_contracts PROPERTIES TIMEOUT 30)
    add_executable(logic_mod_artifact_dag_v4_tests tests/unit/logic_mod_artifact_dag_v4_tests.cpp)
    bruce_enable_sanitizers(logic_mod_artifact_dag_v4_tests)
    target_link_libraries(logic_mod_artifact_dag_v4_tests PRIVATE bruce_core)
    target_compile_options(logic_mod_artifact_dag_v4_tests PRIVATE -Wall -Wextra -Werror)
    target_compile_definitions(logic_mod_artifact_dag_v4_tests PRIVATE
        BRUCE_DAG_WORKER="$<TARGET_FILE:bruce_logic_mod_dag_worker_fixture>"
        BRUCE_DAG_V3_WORKER="$<TARGET_FILE:bruce_logic_mod_dag_v3_worker_fixture>")
    add_dependencies(logic_mod_artifact_dag_v4_tests bruce_logic_mod_dag_worker_fixture bruce_logic_mod_dag_v3_worker_fixture)
    add_test(NAME logic_mod_artifact_dag_v4_contracts COMMAND logic_mod_artifact_dag_v4_tests)
    set_tests_properties(logic_mod_artifact_dag_v4_contracts PROPERTIES TIMEOUT 30)
    add_executable(bruce_logic_mod_dag_v5_worker_fixture tests/unit/logic_mod_artifact_dag_v3_tests.cpp)
    target_compile_definitions(bruce_logic_mod_dag_v5_worker_fixture PRIVATE BRUCE_DAG_V3_WORKER_FIXTURE=1 BRUCE_DAG_V5_WORKER_FIXTURE=1)
    target_compile_options(bruce_logic_mod_dag_v5_worker_fixture PRIVATE -Wall -Wextra -Werror)
    target_link_options(bruce_logic_mod_dag_v5_worker_fixture PRIVATE -static -s)
    add_executable(logic_mod_artifact_dag_v5_tests tests/unit/logic_mod_artifact_dag_v5_tests.cpp)
    bruce_enable_sanitizers(logic_mod_artifact_dag_v5_tests)
    target_link_libraries(logic_mod_artifact_dag_v5_tests PRIVATE bruce_core)
    target_compile_options(logic_mod_artifact_dag_v5_tests PRIVATE -Wall -Wextra -Werror)
    target_compile_definitions(logic_mod_artifact_dag_v5_tests PRIVATE
        BRUCE_DAG_WORKER="$<TARGET_FILE:bruce_logic_mod_dag_worker_fixture>"
        BRUCE_DAG_V3_WORKER="$<TARGET_FILE:bruce_logic_mod_dag_v5_worker_fixture>")
    add_dependencies(logic_mod_artifact_dag_v5_tests bruce_logic_mod_dag_worker_fixture bruce_logic_mod_dag_v5_worker_fixture)
    add_test(NAME logic_mod_artifact_dag_v5_contracts COMMAND logic_mod_artifact_dag_v5_tests)
    set_tests_properties(logic_mod_artifact_dag_v5_contracts PROPERTIES TIMEOUT 30)
    add_executable(logic_mod_artifact_dag_v6_tests tests/unit/logic_mod_artifact_dag_v5_tests.cpp)
    bruce_enable_sanitizers(logic_mod_artifact_dag_v6_tests)
    target_link_libraries(logic_mod_artifact_dag_v6_tests PRIVATE bruce_core)
    target_compile_options(logic_mod_artifact_dag_v6_tests PRIVATE -Wall -Wextra -Werror)
    target_compile_definitions(logic_mod_artifact_dag_v6_tests PRIVATE
        BRUCE_DAG_WORKER="$<TARGET_FILE:bruce_logic_mod_dag_worker_fixture>"
        BRUCE_DAG_V3_WORKER="$<TARGET_FILE:bruce_logic_mod_dag_v5_worker_fixture>")
    add_dependencies(logic_mod_artifact_dag_v6_tests bruce_logic_mod_dag_worker_fixture bruce_logic_mod_dag_v5_worker_fixture)
    add_test(NAME logic_mod_artifact_dag_v6_contracts COMMAND logic_mod_artifact_dag_v6_tests)
    set_tests_properties(logic_mod_artifact_dag_v6_contracts PROPERTIES TIMEOUT 45)
    add_executable(runtime_orchestrator_v1_tests tests/unit/runtime_orchestrator_v1_tests.cpp)
    bruce_enable_sanitizers(runtime_orchestrator_v1_tests)
    target_link_libraries(runtime_orchestrator_v1_tests PRIVATE bruce_core)
    target_compile_options(runtime_orchestrator_v1_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME runtime_orchestrator_v1_contracts COMMAND runtime_orchestrator_v1_tests)
    add_executable(admission_diagnostics_v1_tests tests/unit/admission_diagnostics_v1_tests.cpp)
    bruce_enable_sanitizers(admission_diagnostics_v1_tests)
    target_link_libraries(admission_diagnostics_v1_tests PRIVATE bruce_core)
    target_compile_options(admission_diagnostics_v1_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME admission_diagnostics_v1_contracts COMMAND admission_diagnostics_v1_tests)
    add_executable(controller_observability_cli_v1_tests tests/unit/controller_observability_cli_v1_tests.cpp)
    bruce_enable_sanitizers(controller_observability_cli_v1_tests)
    target_link_libraries(controller_observability_cli_v1_tests PRIVATE bruce_core)
    target_compile_options(controller_observability_cli_v1_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME controller_observability_cli_v1_contracts COMMAND controller_observability_cli_v1_tests)
    add_executable(controller_observability_cli_v1_v6_tests tests/unit/controller_observability_cli_v1_v6_tests.cpp)
    bruce_enable_sanitizers(controller_observability_cli_v1_v6_tests)
    target_link_libraries(controller_observability_cli_v1_v6_tests PRIVATE bruce_core)
    target_compile_options(controller_observability_cli_v1_v6_tests PRIVATE -Wall -Wextra -Werror)
    target_compile_definitions(controller_observability_cli_v1_v6_tests PRIVATE BRUCE_DAG_WORKER="$<TARGET_FILE:bruce_logic_mod_dag_worker_fixture>")
    add_dependencies(controller_observability_cli_v1_v6_tests bruce_logic_mod_dag_worker_fixture)
    add_test(NAME controller_observability_cli_v1_v6_contracts COMMAND controller_observability_cli_v1_v6_tests)
    add_executable(controller_observability_crosswalk_v1_tests tests/unit/controller_observability_crosswalk_v1_tests.cpp)
    bruce_enable_sanitizers(controller_observability_crosswalk_v1_tests)
    target_link_libraries(controller_observability_crosswalk_v1_tests PRIVATE bruce_core)
    target_compile_options(controller_observability_crosswalk_v1_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME controller_observability_crosswalk_v1_contracts COMMAND controller_observability_crosswalk_v1_tests)
    add_executable(logic_mod_artifact_chain_v1_tests tests/unit/logic_mod_artifact_chain_v1_tests.cpp)
    bruce_enable_sanitizers(logic_mod_artifact_chain_v1_tests)
    target_link_libraries(logic_mod_artifact_chain_v1_tests PRIVATE bruce_core)
    target_compile_options(logic_mod_artifact_chain_v1_tests PRIVATE -Wall -Wextra -Werror)
    target_compile_definitions(logic_mod_artifact_chain_v1_tests PRIVATE
        BRUCE_CHAIN_WORKER="$<TARGET_FILE:bruce_logic_mod_chain_worker_fixture>"
        BRUCE_CHAIN_ESCAPE_WORKER="$<TARGET_FILE:bruce_logic_mod_exec_escape_fixture>")
    add_dependencies(logic_mod_artifact_chain_v1_tests bruce_logic_mod_chain_worker_fixture bruce_logic_mod_exec_escape_fixture)
    add_test(NAME logic_mod_artifact_chain_v1_contracts COMMAND logic_mod_artifact_chain_v1_tests)
    add_test(NAME logic_mod_artifact_chain_preflight_v1_contracts COMMAND logic_mod_artifact_chain_v1_tests --preflight)
    set_tests_properties(logic_mod_artifact_chain_v1_contracts logic_mod_artifact_chain_preflight_v1_contracts PROPERTIES TIMEOUT 30)
    add_executable(bruce_logic_mod_worker_v1_tests tests/unit/logic_mod_worker_v1_tests.cpp)
    bruce_enable_sanitizers(bruce_logic_mod_worker_v1_tests)
    target_link_libraries(bruce_logic_mod_worker_v1_tests PRIVATE bruce_core)
    target_include_directories(bruce_logic_mod_worker_v1_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/mod)
    target_compile_options(bruce_logic_mod_worker_v1_tests PRIVATE -Wall -Wextra -Werror)
    target_compile_definitions(bruce_logic_mod_worker_v1_tests PRIVATE BRUCE_LOGIC_EXEC_ESCAPE_WORKER="$<TARGET_FILE:bruce_logic_mod_exec_escape_fixture>")
    add_dependencies(bruce_logic_mod_worker_v1_tests bruce_logic_mod_exec_escape_fixture)
    add_test(NAME logic_mod_worker_v1_contracts COMMAND bruce_logic_mod_worker_v1_tests $<TARGET_FILE:bruce_logic_mod_worker_fixture>)
    set_tests_properties(logic_mod_worker_v1_contracts PROPERTIES TIMEOUT 30)
    add_executable(bruce_logic_mod_worker_attempt_v6_tests tests/unit/logic_mod_worker_attempt_v6_tests.cpp)
    bruce_enable_sanitizers(bruce_logic_mod_worker_attempt_v6_tests)
    target_link_libraries(bruce_logic_mod_worker_attempt_v6_tests PRIVATE bruce_core)
    target_include_directories(bruce_logic_mod_worker_attempt_v6_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/mod)
    target_compile_options(bruce_logic_mod_worker_attempt_v6_tests PRIVATE -Wall -Wextra -Werror)
    add_dependencies(bruce_logic_mod_worker_attempt_v6_tests bruce_logic_mod_worker_fixture)
    add_test(NAME logic_mod_worker_attempt_v6_contracts COMMAND bruce_logic_mod_worker_attempt_v6_tests $<TARGET_FILE:bruce_logic_mod_worker_fixture>)
    set_tests_properties(logic_mod_worker_attempt_v6_contracts PROPERTIES TIMEOUT 30)
    add_executable(bruce_logic_mod_supervision_log_v6_tests tests/unit/logic_mod_supervision_log_v6_tests.cpp)
    bruce_enable_sanitizers(bruce_logic_mod_supervision_log_v6_tests)
    target_link_libraries(bruce_logic_mod_supervision_log_v6_tests PRIVATE bruce_core)
    target_compile_options(bruce_logic_mod_supervision_log_v6_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME logic_mod_supervision_log_v6_contracts COMMAND bruce_logic_mod_supervision_log_v6_tests)
    add_executable(bruce_logic_mod_payload_worker_fixture tests/fixtures/logic_mod_worker_v1.cpp)
    target_compile_definitions(bruce_logic_mod_payload_worker_fixture PRIVATE BRUCE_LOGIC_PAYLOAD_WORKER=1)
    target_compile_options(bruce_logic_mod_payload_worker_fixture PRIVATE -Wall -Wextra -Werror)
    target_link_options(bruce_logic_mod_payload_worker_fixture PRIVATE -static -s)
    add_executable(bruce_logic_mod_payload_worker_v1_tests tests/unit/logic_mod_payload_worker_v1_tests.cpp)
    bruce_enable_sanitizers(bruce_logic_mod_payload_worker_v1_tests)
    target_link_libraries(bruce_logic_mod_payload_worker_v1_tests PRIVATE bruce_core)
    target_compile_options(bruce_logic_mod_payload_worker_v1_tests PRIVATE -Wall -Wextra -Werror)
    target_compile_definitions(bruce_logic_mod_payload_worker_v1_tests PRIVATE BRUCE_LOGIC_EXEC_ESCAPE_WORKER="$<TARGET_FILE:bruce_logic_mod_exec_escape_fixture>")
    target_compile_definitions(bruce_logic_mod_payload_worker_v1_tests PRIVATE BRUCE_LOGIC_PLAIN_WORKER="$<TARGET_FILE:bruce_logic_mod_worker_fixture>")
    add_dependencies(bruce_logic_mod_payload_worker_v1_tests bruce_logic_mod_payload_worker_fixture bruce_logic_mod_exec_escape_fixture)
    add_test(NAME logic_mod_payload_worker_v1_contracts COMMAND bruce_logic_mod_payload_worker_v1_tests $<TARGET_FILE:bruce_logic_mod_payload_worker_fixture>)
    set_tests_properties(logic_mod_payload_worker_v1_contracts PROPERTIES TIMEOUT 30)
    target_link_libraries(bruce_runtime_mod_cli_tests PRIVATE bruce_core bruce_mod_catalog)
    target_compile_definitions(bruce_runtime_mod_cli_tests PRIVATE BRUCE_RUNTIME_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_mods")
    target_compile_options(bruce_runtime_mod_cli_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME runtime_mod_cli_contracts COMMAND bruce_runtime_mod_cli_tests)
    add_executable(bruce_runtime_package_tests tests/unit/runtime_package_tests.cpp)
    bruce_enable_sanitizers(bruce_runtime_package_tests)
    target_link_libraries(bruce_runtime_package_tests PRIVATE bruce_core OpenSSL::Crypto)
    target_compile_definitions(bruce_runtime_package_tests PRIVATE BRUCE_RUNTIME_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_mods")
    target_compile_options(bruce_runtime_package_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME runtime_package_contracts COMMAND bruce_runtime_package_tests)
    add_executable(bruce_package_inspection_v2_tests tests/unit/package_inspection_v2_tests.cpp)
    bruce_enable_sanitizers(bruce_package_inspection_v2_tests)
    target_link_libraries(bruce_package_inspection_v2_tests PRIVATE bruce_core OpenSSL::Crypto)
    target_compile_definitions(bruce_package_inspection_v2_tests PRIVATE BRUCE_RUNTIME_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_mods")
    target_compile_options(bruce_package_inspection_v2_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME package_inspection_v2_contracts COMMAND bruce_package_inspection_v2_tests)
    add_executable(bruce_tree_registry_tests tests/unit/tree_registry_tests.cpp)
    bruce_enable_sanitizers(bruce_tree_registry_tests)
    target_link_libraries(bruce_tree_registry_tests PRIVATE bruce_core)
    target_compile_options(bruce_tree_registry_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME tree_registry_contracts COMMAND bruce_tree_registry_tests)
    add_executable(bruce_management_inspection_control_tests tests/unit/management_inspection_control_tests.cpp)
    bruce_enable_sanitizers(bruce_management_inspection_control_tests)
    target_link_libraries(bruce_management_inspection_control_tests PRIVATE bruce_core)
    target_compile_options(bruce_management_inspection_control_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME management_inspection_control COMMAND bruce_management_inspection_control_tests)
    add_executable(bruce_tokenized_task_run_tests tests/unit/tokenized_task_run_tests.cpp)
    bruce_enable_sanitizers(bruce_tokenized_task_run_tests)
    target_link_libraries(bruce_tokenized_task_run_tests PRIVATE bruce_core)
    target_compile_options(bruce_tokenized_task_run_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME tokenized_task_run_contracts COMMAND bruce_tokenized_task_run_tests)
    add_executable(bruce_task_spec_tests tests/unit/task_spec_tests.cpp)
    bruce_enable_sanitizers(bruce_task_spec_tests)
    target_link_libraries(bruce_task_spec_tests PRIVATE bruce_core)
    target_compile_options(bruce_task_spec_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME task_spec_contracts COMMAND bruce_task_spec_tests)
    if(BRUCE_BUILD_MOD_NANOCELL)
        add_executable(bruce_nanocell_tests tests/mods/nanocell/nanocell_tests.cpp)
        bruce_enable_sanitizers(bruce_nanocell_tests)
        target_link_libraries(bruce_nanocell_tests PRIVATE bruce_mod_nanocell)
        target_compile_options(bruce_nanocell_tests PRIVATE -Wall -Wextra -Werror)
        add_test(NAME nanocell_contracts COMMAND bruce_nanocell_tests)
        add_executable(bruce_nanocell_mod_tests tests/mods/nanocell/nanocell_mod_tests.cpp)
        bruce_enable_sanitizers(bruce_nanocell_mod_tests)
        target_link_libraries(bruce_nanocell_mod_tests PRIVATE bruce_mod_nanocell)
        target_compile_options(bruce_nanocell_mod_tests PRIVATE -Wall -Wextra -Werror)
        add_test(NAME nanocell_mod_contracts COMMAND bruce_nanocell_mod_tests)
        add_executable(bruce_phase_dag_nanocell_tests tests/mods/nanocell/phase_dag_nanocell_tests.cpp)
        bruce_enable_sanitizers(bruce_phase_dag_nanocell_tests)
        target_link_libraries(bruce_phase_dag_nanocell_tests PRIVATE bruce_phase_dag_nanocell)
        target_compile_options(bruce_phase_dag_nanocell_tests PRIVATE -Wall -Wextra -Werror)
        add_test(NAME phase_dag_nanocell_contracts COMMAND bruce_phase_dag_nanocell_tests)
    endif()
    if(BRUCE_BUILD_MOD_MCP AND BRUCE_BUILD_MOD_NANOCELL)
        add_executable(bruce_mcp_inspector_tests tests/mods/mcp/mcp_inspector_tests.cpp)
        bruce_enable_sanitizers(bruce_mcp_inspector_tests)
        target_link_libraries(bruce_mcp_inspector_tests PRIVATE bruce_mod_mcp bruce_mod_catalog)
        target_compile_options(bruce_mcp_inspector_tests PRIVATE -Wall -Wextra -Werror)
        add_test(NAME mcp_inspector_contracts COMMAND bruce_mcp_inspector_tests)
    endif()
    if(BRUCE_BUILD_MOD_LANGUAGE)
        add_executable(bruce_language_run_tests tests/mods/language/language_run_tests.cpp)
        bruce_enable_sanitizers(bruce_language_run_tests)
        target_link_libraries(bruce_language_run_tests PRIVATE bruce_mod_language)
        target_compile_options(bruce_language_run_tests PRIVATE -Wall -Wextra -Werror)
        add_test(NAME language_run_contracts COMMAND bruce_language_run_tests)
        add_executable(bruce_language_tests tests/mods/language/language_tests.cpp)
        bruce_enable_sanitizers(bruce_language_tests)
        target_link_libraries(bruce_language_tests PRIVATE bruce_mod_language)
        target_compile_options(bruce_language_tests PRIVATE -Wall -Wextra -Werror)
        add_test(NAME language_contracts COMMAND bruce_language_tests)
    endif()
    add_executable(bruce_resume_copy_control_tests tests/unit/resume_copy_control_tests.cpp)
    bruce_enable_sanitizers(bruce_resume_copy_control_tests)
    target_link_libraries(bruce_resume_copy_control_tests PRIVATE bruce_core)
    target_compile_options(bruce_resume_copy_control_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME resume_copy_control COMMAND bruce_resume_copy_control_tests)
    add_executable(bruce_session_work_budget_tests tests/unit/session_work_budget_tests.cpp)
    bruce_enable_sanitizers(bruce_session_work_budget_tests)
    target_link_libraries(bruce_session_work_budget_tests PRIVATE bruce_core)
    target_compile_options(bruce_session_work_budget_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME session_work_budget_contracts COMMAND bruce_session_work_budget_tests)
    add_executable(bruce_rule_history_tests tests/unit/rule_history_tests.cpp)
    bruce_enable_sanitizers(bruce_rule_history_tests)
    target_link_libraries(bruce_rule_history_tests PRIVATE bruce_core)
    add_test(NAME rule_history_contracts COMMAND bruce_rule_history_tests)
    add_executable(bruce_rule_proposal_tests tests/unit/rule_proposal_tests.cpp)
    bruce_enable_sanitizers(bruce_rule_proposal_tests)
    target_link_libraries(bruce_rule_proposal_tests PRIVATE bruce_core)
    add_test(NAME rule_proposal_contracts COMMAND bruce_rule_proposal_tests)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME release_tool_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_release_tool.py $<TARGET_FILE:bruce-release-tool>
        --build-dir ${CMAKE_CURRENT_BINARY_DIR} --baseline $<TARGET_FILE:bruce>
        --fixtures ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures --version ${PROJECT_VERSION} --config $<CONFIG>)
    set_tests_properties(release_tool_workflows PROPERTIES TIMEOUT 240)
    add_test(NAME builtin_mod_help_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_builtin_mod_help.py $<TARGET_FILE:bruce>
        --language $<IF:$<BOOL:${BRUCE_BUILD_MOD_LANGUAGE}>,on,off>
        --nanocell $<IF:$<BOOL:${BRUCE_BUILD_MOD_NANOCELL}>,on,off>
        --mcp $<IF:$<BOOL:${BRUCE_BUILD_MOD_MCP}>,on,off>)
    set(BRUCE_BUILTIN_MOD_MATRIX_PARALLEL "2" CACHE STRING
        "Maximum parallel child build jobs used by the built-in Mod matrix (1-16)")
    if(NOT BRUCE_BUILTIN_MOD_MATRIX_PARALLEL MATCHES "^[0-9]+$" OR
       BRUCE_BUILTIN_MOD_MATRIX_PARALLEL LESS 1 OR
       BRUCE_BUILTIN_MOD_MATRIX_PARALLEL GREATER 16)
        message(FATAL_ERROR "BRUCE_BUILTIN_MOD_MATRIX_PARALLEL must be an integer from 1 through 16")
    endif()
    set(_bruce_builtin_mod_matrix_command ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_builtin_mod_matrix.py
        --cmake ${CMAKE_COMMAND} --ctest ${CMAKE_CTEST_COMMAND}
        --source ${CMAKE_CURRENT_SOURCE_DIR}
        --build-root ${CMAKE_CURRENT_BINARY_DIR}/builtin-mod-matrix
        --build-type ${CMAKE_BUILD_TYPE}
        --parallel ${BRUCE_BUILTIN_MOD_MATRIX_PARALLEL})
    foreach(_bruce_launcher_token IN LISTS CMAKE_CXX_COMPILER_LAUNCHER)
        list(APPEND _bruce_builtin_mod_matrix_command
            "--compiler-launcher=${_bruce_launcher_token}")
    endforeach()
    add_test(NAME builtin_mod_build_matrix COMMAND ${_bruce_builtin_mod_matrix_command})
    set_tests_properties(builtin_mod_build_matrix PROPERTIES
        TIMEOUT 1800
        LABELS "builtin-mods;build-matrix"
        RESOURCE_LOCK bruce_mcp_inspector)
    add_test(NAME controller_observability_cli_v1_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_controller_observability_v1_cli.py $<TARGET_FILE:bruce>)
    add_test(NAME dag_scheduler_v1_cli_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_dag_scheduler_v1_cli.py $<TARGET_FILE:bruce>)
    add_test(NAME artifact_store_v1_cli_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_artifact_store_v1.py $<TARGET_FILE:bruce>)
    add_test(NAME artifact_store_v2_cli_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_artifact_store_v2.py $<TARGET_FILE:bruce>)
    set_tests_properties(artifact_store_v2_cli_workflows PROPERTIES TIMEOUT 90)
    add_test(NAME artifact_store_v4_cli_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_artifact_store_v4.py $<TARGET_FILE:bruce>)
    set_tests_properties(artifact_store_v4_cli_workflows PROPERTIES TIMEOUT 120)
    add_test(NAME artifact_store_v3_cli_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_artifact_store_v3.py $<TARGET_FILE:bruce>)
    set_tests_properties(artifact_store_v3_cli_workflows PROPERTIES TIMEOUT 120)
    add_test(NAME artifact_store_resolve_v1_cli_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_artifact_store_resolve_v1.py $<TARGET_FILE:bruce>)
    add_test(NAME dag_scheduler_v2_cli_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_dag_scheduler_v2_cli.py $<TARGET_FILE:bruce>)
    add_test(NAME dag_scheduler_v3_cli_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_dag_scheduler_v3_cli.py $<TARGET_FILE:bruce>)
    add_test(NAME dag_scheduler_v4_cli_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_dag_scheduler_v4_cli.py $<TARGET_FILE:bruce>)
    add_test(NAME toy_relation_v1_cli_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_toy_relation_v1_cli.py $<TARGET_FILE:bruce>)
    add_test(NAME signed_affine_dag_v1_cli_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_signed_affine_dag_v1_cli.py $<TARGET_FILE:bruce>)
    add_test(NAME signed_affine_relation_v1_cli_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_signed_affine_relation_v1_cli.py $<TARGET_FILE:bruce>)
    add_test(NAME with_host_deps_contract COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_with_host_deps.py
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/with-host-deps.sh)
    add_test(NAME with_host_deps_wrapper_boundaries COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_with_host_deps_wrapper.py)
    add_test(NAME current_source_verification_boundaries COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_verify_current_source_script.py)
    add_test(NAME task_spec_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_task_spec.py $<TARGET_FILE:bruce>)
    add_test(NAME runtime_mod_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_runtime_mods.py $<TARGET_FILE:bruce>)
    add_test(NAME logic_mod_lifecycle_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_logic_mod_lifecycle.py
        $<TARGET_FILE:bruce> $<TARGET_FILE:bruce-mod-builder> $<TARGET_FILE:bruce_logic_mod_worker_fixture>)
    add_test(NAME installed_logic_mod_run_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_installed_logic_mod_run.py
        $<TARGET_FILE:bruce> $<TARGET_FILE:bruce-mod-builder> $<TARGET_FILE:bruce_logic_mod_worker_fixture>)
    add_test(NAME mod_update_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_mod_update.py
        $<TARGET_FILE:bruce> $<TARGET_FILE:bruce-mod-builder> $<TARGET_FILE:bruce_logic_mod_worker_fixture>)
    add_test(NAME mod_package_inspection_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_mod_package_inspection.py
        $<TARGET_FILE:bruce> $<TARGET_FILE:bruce_logic_mod_worker_v1_tests> $<TARGET_FILE:bruce_logic_mod_worker_fixture>)
    add_test(NAME logic_mod_plugin_v1_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_logic_mod_plugin_v1.py $<TARGET_FILE:bruce> $<TARGET_FILE:bruce_logic_mod_worker_v1_tests> $<TARGET_FILE:bruce_logic_mod_worker_fixture>)
    add_test(NAME logic_mod_payload_v1_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_logic_mod_payload_v1.py $<TARGET_FILE:bruce> $<TARGET_FILE:bruce_logic_mod_payload_worker_v1_tests> $<TARGET_FILE:bruce_logic_mod_payload_worker_fixture>)
    add_test(NAME logic_mod_result_artifact_v1_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_logic_mod_result_artifact_v1.py $<TARGET_FILE:bruce> $<TARGET_FILE:bruce_logic_mod_payload_worker_v1_tests> $<TARGET_FILE:bruce_logic_mod_payload_worker_fixture>)
    add_test(NAME logic_mod_artifact_chain_v1_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_logic_mod_artifact_chain_v1.py
        $<TARGET_FILE:bruce> $<TARGET_FILE:bruce_logic_mod_payload_worker_v1_tests>
        $<TARGET_FILE:bruce_logic_mod_chain_worker_fixture> ${CMAKE_CXX_COMPILER})
    set_tests_properties(logic_mod_artifact_chain_v1_workflows PROPERTIES TIMEOUT 45)
    add_test(NAME logic_mod_artifact_dag_v2_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_logic_mod_artifact_dag_v2.py
        $<TARGET_FILE:bruce> $<TARGET_FILE:bruce_logic_mod_payload_worker_v1_tests>
        $<TARGET_FILE:bruce_logic_mod_dag_worker_fixture> ${CMAKE_CXX_COMPILER})
    set_tests_properties(logic_mod_artifact_dag_v2_workflows PROPERTIES TIMEOUT 45)
    add_test(NAME logic_mod_artifact_dag_v3_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_logic_mod_artifact_dag_v3.py
        $<TARGET_FILE:bruce> $<TARGET_FILE:bruce_logic_mod_payload_worker_v1_tests>
        $<TARGET_FILE:bruce_logic_mod_dag_worker_fixture>
        $<TARGET_FILE:bruce_logic_mod_dag_v3_worker_fixture> ${CMAKE_CXX_COMPILER})
    set_tests_properties(logic_mod_artifact_dag_v3_workflows PROPERTIES TIMEOUT 60)
    add_test(NAME logic_mod_artifact_dag_v4_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_logic_mod_artifact_dag_v4.py
        $<TARGET_FILE:bruce> $<TARGET_FILE:bruce_logic_mod_payload_worker_v1_tests>
        $<TARGET_FILE:bruce_logic_mod_dag_worker_fixture>
        $<TARGET_FILE:bruce_logic_mod_dag_v3_worker_fixture> ${CMAKE_CXX_COMPILER})
    set_tests_properties(logic_mod_artifact_dag_v4_workflows PROPERTIES TIMEOUT 60)
    add_test(NAME logic_mod_artifact_dag_v5_cli_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_logic_mod_artifact_dag_v5_cli.py
        $<TARGET_FILE:bruce> $<TARGET_FILE:bruce_logic_mod_payload_worker_v1_tests>
        $<TARGET_FILE:bruce_logic_mod_dag_worker_fixture> ${CMAKE_CXX_COMPILER})
    set_tests_properties(logic_mod_artifact_dag_v5_cli_workflows PROPERTIES TIMEOUT 60)
    add_test(NAME logic_mod_artifact_dag_v6_cli_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_logic_mod_artifact_dag_v6_cli.py
        $<TARGET_FILE:bruce> $<TARGET_FILE:bruce_logic_mod_payload_worker_v1_tests>
        $<TARGET_FILE:bruce_logic_mod_dag_worker_fixture>
        $<TARGET_FILE:bruce_logic_mod_dag_v3_worker_fixture> ${CMAKE_CXX_COMPILER})
    set_tests_properties(logic_mod_artifact_dag_v6_cli_workflows PROPERTIES TIMEOUT 60)
    add_test(NAME runtime_orchestrator_v1_cli_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_runtime_orchestrator_v1_cli.py
        $<TARGET_FILE:bruce> $<TARGET_FILE:bruce_logic_mod_payload_worker_v1_tests>
        $<TARGET_FILE:bruce_logic_mod_dag_worker_fixture>
        $<TARGET_FILE:bruce_logic_mod_dag_v5_worker_fixture> ${CMAKE_CXX_COMPILER})
    set_tests_properties(runtime_orchestrator_v1_cli_workflows PROPERTIES TIMEOUT 60)
    add_test(NAME host_lifecycle_contract_matrix_v1 COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify_host_lifecycle_contract_matrix_v1.py)
    add_test(NAME logic_mod_result_inspector_v1_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_logic_mod_result_inspector_v1.py
        $<TARGET_FILE:bruce> $<TARGET_FILE:bruce_logic_mod_payload_worker_v1_tests> $<TARGET_FILE:bruce_logic_mod_payload_worker_fixture>)
    set_tests_properties(logic_mod_result_inspector_v1_contracts logic_mod_result_inspector_v1_workflows PROPERTIES TIMEOUT 30)
    add_test(NAME local_builder_cli_contracts COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_local_builder_cli.py $<TARGET_FILE:bruce-mod-builder> $<TARGET_FILE:bruce-mod-builder-report-fault>)
    add_test(NAME local_logic_builder_cli_contracts COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_local_logic_builder_cli.py
        $<TARGET_FILE:bruce-mod-builder> $<TARGET_FILE:bruce-mod-builder-report-fault>
        $<TARGET_FILE:bruce> $<TARGET_FILE:bruce_logic_mod_worker_fixture>)
    add_test(NAME local_data_mod_example_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_local_data_mod_example.py $<TARGET_FILE:bruce-mod-builder> $<TARGET_FILE:bruce>)
    if(BRUCE_BUILD_MOD_MCP AND BRUCE_BUILD_MOD_NANOCELL)
        add_test(NAME mcp_inspector_workflows COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/mods/mcp/test_mcp_inspector.py $<TARGET_FILE:bruce>)
        set_tests_properties(mcp_inspector_workflows PROPERTIES
            LABELS "builtin-mods;mcp;integration"
            RESOURCE_LOCK bruce_mcp_inspector)
    endif()
    if(BRUCE_BUILD_MOD_NANOCELL)
        add_test(NAME nanocell_cli_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/mods/nanocell/test_nanocell.py $<TARGET_FILE:bruce>)
        add_test(NAME phase_dag_v2_cli_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/mods/nanocell/test_phase_dag_v2_cli.py $<TARGET_FILE:bruce>)
        add_test(NAME phase_dag_v3_cli_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/mods/nanocell/test_phase_dag_v3_cli.py $<TARGET_FILE:bruce>)
    endif()
    if(BRUCE_BUILD_MOD_LANGUAGE)
        add_test(NAME language_workflows COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/mods/language/test_language.py $<TARGET_FILE:bruce>)
    endif()
    add_test(NAME control_flow_acceptance COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify_control_flow.py $<TARGET_FILE:bruce>)
    add_test(NAME control_contract_matrix COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify_control_contract_matrix.py $<TARGET_FILE:bruce>)
    add_test(NAME control_contract_matrix_unit COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_control_contract_matrix.py)
    add_test(NAME common_runtime_compatibility_v1_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_common_runtime_compatibility_v1.py
        $<TARGET_FILE:bruce> $<TARGET_FILE:bruce_common_runtime_matrix_binding_tests>)
    add_test(NAME common_runtime_compatibility_v1_stage1_gate COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_common_runtime_compatibility_v1_stage1_gate.py
        $<TARGET_FILE:bruce>)
    add_test(NAME common_runtime_compatibility_v1_stage1_management COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_common_runtime_compatibility_v1_stage1_management.py
        $<TARGET_FILE:bruce>)
    add_test(NAME common_runtime_compatibility_v1_stage1_transfer COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_common_runtime_compatibility_v1_stage1_transfer.py
        $<TARGET_FILE:bruce>)
    add_test(NAME common_runtime_compatibility_v1_stage1_session COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_common_runtime_compatibility_v1_stage1_session.py
        $<TARGET_FILE:bruce>)
    add_test(NAME common_runtime_compatibility_v1_stage2_compare COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_common_runtime_compatibility_v1_stage2_compare.py
        $<TARGET_FILE:bruce>)
    add_test(NAME common_runtime_compatibility_v1_stage2_grid COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_common_runtime_compatibility_v1_stage2_grid.py
        $<TARGET_FILE:bruce>)
    add_test(NAME common_runtime_compatibility_v1_stage2_grid_batch COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_common_runtime_compatibility_v1_stage2_grid_batch.py
        $<TARGET_FILE:bruce>)
    add_test(NAME common_runtime_compatibility_v1_stage2_grid_resume COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_common_runtime_compatibility_v1_stage2_grid_resume.py
        $<TARGET_FILE:bruce>)
    add_test(NAME compatibility_transfer_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_compatibility_transfer.py $<TARGET_FILE:bruce>)
    add_test(NAME experience_transfer_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_experience_transfer.py $<TARGET_FILE:bruce>)
    add_test(NAME rule_store_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_rule_store.py $<TARGET_FILE:bruce>)
    add_test(NAME rule_review_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_rule_review.py $<TARGET_FILE:bruce>)
    add_test(NAME result_assessment_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_result_assessment.py $<TARGET_FILE:bruce>)
    add_test(NAME benchmark_runner_contracts COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_benchmark_runner.py)
    add_test(NAME resume_copy_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_resume_copy.py $<TARGET_FILE:bruce>)
    add_test(NAME serialized_byte_budget_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_serialized_byte_budget.py $<TARGET_FILE:bruce>)
    add_test(NAME cli_version COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_version.py $<TARGET_FILE:bruce> ${PROJECT_VERSION})
    add_test(NAME installed_cli_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify_install.py
        --build-dir ${CMAKE_CURRENT_BINARY_DIR} --baseline $<TARGET_FILE:bruce> --fixtures ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures --version ${PROJECT_VERSION} --config $<CONFIG>)
    # Both workflows run cmake --install, which writes the build's install manifest.
    set_tests_properties(release_tool_workflows installed_cli_workflows
        PROPERTIES RESOURCE_LOCK bruce_install_tree)
    add_test(NAME session_work_budget_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_session_work_budget.py $<TARGET_FILE:bruce>)
    add_test(NAME grid_partial_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_grid_partial.py $<TARGET_FILE:bruce>)
    add_test(NAME grid_budget_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_grid_budget.py $<TARGET_FILE:bruce>)
    add_test(NAME grid_axis_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_grid_axis.py $<TARGET_FILE:bruce>)
    add_executable(bruce_grid_research_tests tests/unit/grid_research_tests.cpp)
    bruce_enable_sanitizers(bruce_grid_research_tests)
    target_link_libraries(bruce_grid_research_tests PRIVATE bruce_core)
    target_compile_options(bruce_grid_research_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME grid_research_contracts COMMAND bruce_grid_research_tests)
    add_test(NAME grid_research_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_grid_research.py $<TARGET_FILE:bruce>)
    add_executable(bruce_grid_cache_tests tests/unit/grid_cache_tests.cpp)
    bruce_enable_sanitizers(bruce_grid_cache_tests)
    target_link_libraries(bruce_grid_cache_tests PRIVATE bruce_core)
    target_compile_options(bruce_grid_cache_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME grid_cache_contracts COMMAND bruce_grid_cache_tests)
    add_test(NAME grid_cache_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_grid_cache.py $<TARGET_FILE:bruce>)
    add_executable(bruce_grid_tests tests/unit/grid_tests.cpp)
    bruce_enable_sanitizers(bruce_grid_tests)
    target_link_libraries(bruce_grid_tests PRIVATE bruce_core)
    target_compile_options(bruce_grid_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME grid_contracts COMMAND bruce_grid_tests)
    add_executable(bruce_grid_stage_sweep_tests tests/unit/grid_stage_sweep_tests.cpp)
    bruce_enable_sanitizers(bruce_grid_stage_sweep_tests)
    target_link_libraries(bruce_grid_stage_sweep_tests PRIVATE bruce_core)
    target_compile_options(bruce_grid_stage_sweep_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME grid_stage_sweep_contracts COMMAND bruce_grid_stage_sweep_tests)
    add_executable(bruce_grid_stage_verify_tests tests/unit/grid_stage_verify_tests.cpp)
    bruce_enable_sanitizers(bruce_grid_stage_verify_tests)
    target_link_libraries(bruce_grid_stage_verify_tests PRIVATE bruce_core)
    target_compile_options(bruce_grid_stage_verify_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME grid_stage_verify_contracts COMMAND bruce_grid_stage_verify_tests)
    add_test(NAME grid_replay_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_grid_replay.py $<TARGET_FILE:bruce>)
    add_executable(bruce_witness_portfolio_tests tests/unit/witness_portfolio_tests.cpp)
    bruce_enable_sanitizers(bruce_witness_portfolio_tests)
    target_link_libraries(bruce_witness_portfolio_tests PRIVATE bruce_core)
    target_compile_options(bruce_witness_portfolio_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME witness_portfolio_contracts COMMAND bruce_witness_portfolio_tests)
    add_test(NAME witness_portfolio_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_witness_portfolio.py $<TARGET_FILE:bruce>)
    add_executable(bruce_witness_expression_tests tests/unit/witness_expression_tests.cpp)
    bruce_enable_sanitizers(bruce_witness_expression_tests)
    target_link_libraries(bruce_witness_expression_tests PRIVATE bruce_core)
    target_compile_options(bruce_witness_expression_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME witness_expression_contracts COMMAND bruce_witness_expression_tests)
    add_test(NAME witness_expression_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_witness_expression.py $<TARGET_FILE:bruce>)
    add_executable(bruce_rational_witness_tests tests/unit/rational_witness_tests.cpp)
    bruce_enable_sanitizers(bruce_rational_witness_tests)
    target_link_libraries(bruce_rational_witness_tests PRIVATE bruce_core)
    target_compile_options(bruce_rational_witness_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME rational_witness_contracts COMMAND bruce_rational_witness_tests)
    add_test(NAME rational_witness_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_rational_witness.py $<TARGET_FILE:bruce>)
    add_executable(bruce_domain_cache_portfolio_tests tests/unit/domain_cache_portfolio_tests.cpp)
    bruce_enable_sanitizers(bruce_domain_cache_portfolio_tests)
    target_link_libraries(bruce_domain_cache_portfolio_tests PRIVATE bruce_core)
    target_compile_options(bruce_domain_cache_portfolio_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME domain_cache_portfolio_contracts COMMAND bruce_domain_cache_portfolio_tests)
    add_executable(bruce_domain_cache_tests tests/unit/domain_cache_tests.cpp)
    bruce_enable_sanitizers(bruce_domain_cache_tests)
    target_link_libraries(bruce_domain_cache_tests PRIVATE bruce_core)
    target_compile_options(bruce_domain_cache_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME domain_cache_contracts COMMAND bruce_domain_cache_tests)
    add_executable(bruce_q_portfolio_tests tests/unit/q_portfolio_tests.cpp)
    bruce_enable_sanitizers(bruce_q_portfolio_tests)
    target_link_libraries(bruce_q_portfolio_tests PRIVATE bruce_core)
    target_compile_options(bruce_q_portfolio_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME q_portfolio_contracts COMMAND bruce_q_portfolio_tests)
    add_test(NAME domain_cache_portfolio_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_domain_cache_portfolio.py $<TARGET_FILE:bruce>)
    add_test(NAME domain_cache_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_domain_cache.py $<TARGET_FILE:bruce>)
    add_test(NAME modular_expression_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_modular_expression.py $<TARGET_FILE:bruce>)
    add_test(NAME modular_portfolio_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_modular_portfolio.py $<TARGET_FILE:bruce>)
    add_test(NAME q_portfolio_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_q_portfolio.py $<TARGET_FILE:bruce>)
    add_executable(bruce_q_expression_tests tests/unit/q_expression_tests.cpp)
    bruce_enable_sanitizers(bruce_q_expression_tests)
    target_link_libraries(bruce_q_expression_tests PRIVATE bruce_core)
    target_compile_options(bruce_q_expression_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME q_expression_contracts COMMAND bruce_q_expression_tests)
    add_test(NAME q_expression_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_q_expression.py $<TARGET_FILE:bruce>)
    add_executable(bruce_fraction_comparison_tests tests/unit/fraction_comparison_tests.cpp)
    bruce_enable_sanitizers(bruce_fraction_comparison_tests)
    target_link_libraries(bruce_fraction_comparison_tests PRIVATE bruce_core)
    target_compile_options(bruce_fraction_comparison_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME fraction_comparison_contracts COMMAND bruce_fraction_comparison_tests)
    add_test(NAME modular_comparison_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_modular_compare.py $<TARGET_FILE:bruce>)
    add_test(NAME fraction_comparison_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_fraction_compare.py $<TARGET_FILE:bruce>)
    add_executable(bruce_core_tests tests/unit/core_tests.cpp)
    bruce_enable_sanitizers(bruce_core_tests)
    target_link_libraries(bruce_core_tests PRIVATE bruce_core)
    target_compile_options(bruce_core_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME core_contracts COMMAND bruce_core_tests)
    add_test(NAME cli_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_cli.py $<TARGET_FILE:bruce>)
    if(BRUCE_BUILD_MOD_LANGUAGE AND BRUCE_BUILD_MOD_NANOCELL AND BRUCE_BUILD_MOD_MCP)
        add_test(NAME cli_documentation_determinism COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_cli_docs.py $<TARGET_FILE:bruce>)
    endif()
    add_test(NAME compatibility_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_compatibility.py $<TARGET_FILE:bruce>)
    add_executable(bruce_compatibility_tests tests/unit/compatibility_tests.cpp)
    bruce_enable_sanitizers(bruce_compatibility_tests)
    target_link_libraries(bruce_compatibility_tests PRIVATE bruce_core)
    target_compile_options(bruce_compatibility_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME compatibility_contracts COMMAND bruce_compatibility_tests)
    add_test(NAME exact_domain_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_exact_domains.py $<TARGET_FILE:bruce>)
    add_executable(bruce_exact_tests tests/unit/exact_domain_tests.cpp)
    bruce_enable_sanitizers(bruce_exact_tests)
    target_link_libraries(bruce_exact_tests PRIVATE bruce_core)
    target_compile_options(bruce_exact_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME exact_domain_contracts COMMAND bruce_exact_tests)
    add_executable(bruce_evidence_chain_tests tests/unit/evidence_chain_tests.cpp)
    bruce_enable_sanitizers(bruce_evidence_chain_tests)
    target_link_libraries(bruce_evidence_chain_tests PRIVATE bruce_core)
    target_compile_options(bruce_evidence_chain_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME evidence_chain_contracts COMMAND bruce_evidence_chain_tests)
    add_test(NAME evidence_chain_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_evidence_chain.py $<TARGET_FILE:bruce>)
    add_test(NAME power_research_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_power_research.py $<TARGET_FILE:bruce>)
    add_test(NAME adaptive_research_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_adaptive_research.py $<TARGET_FILE:bruce>)
    add_test(NAME polynomial_research_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_polynomial_research.py $<TARGET_FILE:bruce>)
    add_executable(bruce_polynomial_tests tests/unit/polynomial_tests.cpp)
    bruce_enable_sanitizers(bruce_polynomial_tests)
    target_link_libraries(bruce_polynomial_tests PRIVATE bruce_core)
    target_compile_options(bruce_polynomial_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME polynomial_contracts COMMAND bruce_polynomial_tests)
    add_test(NAME portfolio_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_portfolio.py $<TARGET_FILE:bruce>)
    add_executable(bruce_portfolio_tests tests/unit/portfolio_tests.cpp)
    bruce_enable_sanitizers(bruce_portfolio_tests)
    target_link_libraries(bruce_portfolio_tests PRIVATE bruce_core)
    target_compile_options(bruce_portfolio_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME portfolio_contracts COMMAND bruce_portfolio_tests)
    add_test(NAME rational_linear_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_rational_linear.py $<TARGET_FILE:bruce>)
    add_executable(bruce_rational_linear_tests tests/unit/rational_linear_tests.cpp)
    bruce_enable_sanitizers(bruce_rational_linear_tests)
    target_link_libraries(bruce_rational_linear_tests PRIVATE bruce_core)
    target_compile_options(bruce_rational_linear_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME rational_linear_contracts COMMAND bruce_rational_linear_tests)
    add_test(NAME rational_portfolio_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_rational_portfolio.py $<TARGET_FILE:bruce>)
    add_executable(bruce_rational_portfolio_tests tests/unit/rational_portfolio_tests.cpp)
    bruce_enable_sanitizers(bruce_rational_portfolio_tests)
    target_link_libraries(bruce_rational_portfolio_tests PRIVATE bruce_core)
    target_compile_options(bruce_rational_portfolio_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME rational_portfolio_contracts COMMAND bruce_rational_portfolio_tests)
    add_test(NAME adaptive_portfolio_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_adaptive_portfolio.py $<TARGET_FILE:bruce>)
    add_executable(bruce_adaptive_portfolio_tests tests/unit/adaptive_portfolio_tests.cpp)
    bruce_enable_sanitizers(bruce_adaptive_portfolio_tests)
    target_link_libraries(bruce_adaptive_portfolio_tests PRIVATE bruce_core)
    target_compile_options(bruce_adaptive_portfolio_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME adaptive_portfolio_contracts COMMAND bruce_adaptive_portfolio_tests)
    add_test(NAME experience_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_experience.py $<TARGET_FILE:bruce>)
    add_executable(bruce_experience_header_tests tests/unit/experience_header_tests.cpp)
    bruce_enable_sanitizers(bruce_experience_header_tests)
    target_include_directories(bruce_experience_header_tests PRIVATE include)
    target_compile_options(bruce_experience_header_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME experience_header_boundary COMMAND bruce_experience_header_tests)
    add_executable(bruce_experience_tests tests/unit/experience_tests.cpp)
    bruce_enable_sanitizers(bruce_experience_tests)
    target_link_libraries(bruce_experience_tests PRIVATE bruce_core)
    target_compile_options(bruce_experience_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME experience_contracts COMMAND bruce_experience_tests)
    add_test(NAME rational_quadratic_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_rational_quadratic.py $<TARGET_FILE:bruce>)
    add_executable(bruce_rational_quadratic_tests tests/unit/rational_quadratic_tests.cpp)
    bruce_enable_sanitizers(bruce_rational_quadratic_tests)
    target_link_libraries(bruce_rational_quadratic_tests PRIVATE bruce_core)
    target_compile_options(bruce_rational_quadratic_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME rational_quadratic_contracts COMMAND bruce_rational_quadratic_tests)
    add_test(NAME quadratic_portfolio_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_quadratic_portfolio.py $<TARGET_FILE:bruce>)
    add_executable(bruce_quadratic_portfolio_tests tests/unit/quadratic_portfolio_tests.cpp)
    bruce_enable_sanitizers(bruce_quadratic_portfolio_tests)
    target_link_libraries(bruce_quadratic_portfolio_tests PRIVATE bruce_core)
    target_compile_options(bruce_quadratic_portfolio_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME quadratic_portfolio_contracts COMMAND bruce_quadratic_portfolio_tests)
    add_test(NAME expression_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_expression.py $<TARGET_FILE:bruce>)
    add_executable(bruce_expression_tests tests/unit/expression_tests.cpp)
    bruce_enable_sanitizers(bruce_expression_tests)
    target_link_libraries(bruce_expression_tests PRIVATE bruce_core)
    target_compile_options(bruce_expression_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME expression_contracts COMMAND bruce_expression_tests)
    add_test(NAME expression_equivalence_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_expression_equivalence.py $<TARGET_FILE:bruce>)
    add_executable(bruce_expression_equivalence_tests tests/unit/expression_equivalence_tests.cpp)
    bruce_enable_sanitizers(bruce_expression_equivalence_tests)
    target_link_libraries(bruce_expression_equivalence_tests PRIVATE bruce_core)
    target_compile_options(bruce_expression_equivalence_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME expression_equivalence_contracts COMMAND bruce_expression_equivalence_tests)
    add_test(NAME expression_portfolio_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_expression_portfolio.py $<TARGET_FILE:bruce>)
    add_executable(bruce_expression_portfolio_tests tests/unit/expression_portfolio_tests.cpp)
    bruce_enable_sanitizers(bruce_expression_portfolio_tests)
    target_link_libraries(bruce_expression_portfolio_tests PRIVATE bruce_core)
    target_compile_options(bruce_expression_portfolio_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME expression_portfolio_contracts COMMAND bruce_expression_portfolio_tests)
    add_test(NAME five_experience_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_five_experience.py $<TARGET_FILE:bruce>)
    add_executable(bruce_five_experience_tests tests/unit/five_experience_tests.cpp)
    bruce_enable_sanitizers(bruce_five_experience_tests)
    target_link_libraries(bruce_five_experience_tests PRIVATE bruce_core)
    target_compile_options(bruce_five_experience_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME five_experience_contracts COMMAND bruce_five_experience_tests)
    add_test(NAME indexed_expression_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_indexed_expression.py $<TARGET_FILE:bruce>)
    add_executable(bruce_indexed_expression_tests tests/unit/indexed_expression_tests.cpp)
    bruce_enable_sanitizers(bruce_indexed_expression_tests)
    target_link_libraries(bruce_indexed_expression_tests PRIVATE bruce_core)
    target_compile_options(bruce_indexed_expression_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME indexed_expression_contracts COMMAND bruce_indexed_expression_tests)
    add_test(NAME indexed_portfolio_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_indexed_portfolio.py $<TARGET_FILE:bruce>)
    add_executable(bruce_indexed_portfolio_tests tests/unit/indexed_portfolio_tests.cpp)
    bruce_enable_sanitizers(bruce_indexed_portfolio_tests)
    target_link_libraries(bruce_indexed_portfolio_tests PRIVATE bruce_core)
    target_compile_options(bruce_indexed_portfolio_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME indexed_portfolio_contracts COMMAND bruce_indexed_portfolio_tests)
    add_test(NAME expression_operators_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_expression_operators.py $<TARGET_FILE:bruce>)
    add_executable(bruce_expression_operators_tests tests/unit/expression_operators_tests.cpp)
    bruce_enable_sanitizers(bruce_expression_operators_tests)
    target_link_libraries(bruce_expression_operators_tests PRIVATE bruce_core)
    target_compile_options(bruce_expression_operators_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME expression_operators_contracts COMMAND bruce_expression_operators_tests)
    add_test(NAME portfolio_operators_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_portfolio_operators.py $<TARGET_FILE:bruce>)
    add_executable(bruce_portfolio_operators_tests tests/unit/portfolio_operators_tests.cpp)
    bruce_enable_sanitizers(bruce_portfolio_operators_tests)
    target_link_libraries(bruce_portfolio_operators_tests PRIVATE bruce_core)
    target_compile_options(bruce_portfolio_operators_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME portfolio_operators_contracts COMMAND bruce_portfolio_operators_tests)
    add_executable(bruce_portfolio_powers_tests tests/unit/portfolio_powers_tests.cpp)
    bruce_enable_sanitizers(bruce_portfolio_powers_tests)
    target_link_libraries(bruce_portfolio_powers_tests PRIVATE bruce_core)
    target_compile_options(bruce_portfolio_powers_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME portfolio_powers_contracts COMMAND bruce_portfolio_powers_tests)
    add_test(NAME portfolio_powers_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_portfolio_powers.py $<TARGET_FILE:bruce>)
    add_executable(bruce_cached_expression_tests tests/unit/cached_expression_tests.cpp)
    bruce_enable_sanitizers(bruce_cached_expression_tests)
    target_link_libraries(bruce_cached_expression_tests PRIVATE bruce_core)
    target_compile_options(bruce_cached_expression_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME cached_expression_contracts COMMAND bruce_cached_expression_tests)
    add_executable(bruce_cached_portfolio_tests tests/unit/cached_portfolio_tests.cpp)
    bruce_enable_sanitizers(bruce_cached_portfolio_tests)
    target_link_libraries(bruce_cached_portfolio_tests PRIVATE bruce_core)
    target_compile_options(bruce_cached_portfolio_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME cached_portfolio_contracts COMMAND bruce_cached_portfolio_tests)
    add_test(NAME cached_portfolio_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_cached_portfolio.py $<TARGET_FILE:bruce>)
    add_test(NAME cached_expression_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_cached_expression.py $<TARGET_FILE:bruce>)
    add_test(NAME expression_powers_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_expression_powers.py $<TARGET_FILE:bruce>)
    add_executable(bruce_expression_powers_tests tests/unit/expression_powers_tests.cpp)
    bruce_enable_sanitizers(bruce_expression_powers_tests)
    target_link_libraries(bruce_expression_powers_tests PRIVATE bruce_core)
    target_compile_options(bruce_expression_powers_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME expression_powers_contracts COMMAND bruce_expression_powers_tests)
    add_test(NAME grid_resume_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_grid_resume.py $<TARGET_FILE:bruce>)
    add_test(NAME grid_experience_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_grid_experience.py $<TARGET_FILE:bruce>)
    add_test(NAME experience_replace_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_experience_replace.py $<TARGET_FILE:bruce>)
    add_test(NAME grid_experience_budget_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_grid_experience_budget.py $<TARGET_FILE:bruce>)
    add_executable(bruce_json_encoding_tests tests/unit/json_encoding_tests.cpp src/core/json.cpp)
    bruce_enable_sanitizers(bruce_json_encoding_tests)
    target_include_directories(bruce_json_encoding_tests PRIVATE include)
    target_compile_options(bruce_json_encoding_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME json_encoding_isolation COMMAND bruce_json_encoding_tests)
    add_executable(bruce_research_gate_isolation_tests tests/unit/research_gate_isolation_tests.cpp src/application/research_gates.cpp)
    bruce_enable_sanitizers(bruce_research_gate_isolation_tests)
    target_include_directories(bruce_research_gate_isolation_tests PRIVATE include)
    target_compile_options(bruce_research_gate_isolation_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME research_gate_isolation COMMAND bruce_research_gate_isolation_tests)
    add_executable(bruce_research_contract_tests tests/unit/research_contract_tests.cpp)
    bruce_enable_sanitizers(bruce_research_contract_tests)
    target_link_libraries(bruce_research_contract_tests PRIVATE bruce_core)
    target_compile_options(bruce_research_contract_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME research_contract_matrix COMMAND bruce_research_contract_tests)
    add_test(NAME research_contract_catalog COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_research_contract.py $<TARGET_FILE:bruce>)
    add_test(NAME bruce_mcp_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_bruce_mcp.py $<TARGET_FILE:bruce>)
    add_test(NAME bruce_local_ai_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_bruce_ai.py)
    list(APPEND _bruce_sanitizer_excluded bruce_local_ai_workflows)
    set(_bruce_sanitizer_reason_bruce_local_ai_workflows
        "Python assistant with loopback mocks and fixture executables only - no instrumented C++ process")
    add_executable(bruce_application_task_tests tests/unit/application_task_tests.cpp)
    bruce_enable_sanitizers(bruce_application_task_tests)
    target_link_libraries(bruce_application_task_tests PRIVATE bruce_core)
    target_compile_options(bruce_application_task_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME application_task_adapters COMMAND bruce_application_task_tests)

    add_executable(bruce_mod_api_tests tests/unit/mod_api_tests.cpp)
    bruce_enable_sanitizers(bruce_mod_api_tests)
    target_link_libraries(bruce_mod_api_tests PRIVATE bruce_mod_runtime)
    target_compile_options(bruce_mod_api_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME mod_api_contracts COMMAND bruce_mod_api_tests)
    add_executable(bruce_mod_catalog_tests tests/unit/mod_catalog_tests.cpp)
    bruce_enable_sanitizers(bruce_mod_catalog_tests)
    target_link_libraries(bruce_mod_catalog_tests PRIVATE bruce_mod_catalog)
    target_compile_definitions(bruce_mod_catalog_tests PRIVATE
        BRUCE_BUILD_MOD_LANGUAGE=$<BOOL:${BRUCE_BUILD_MOD_LANGUAGE}>
        BRUCE_BUILD_MOD_NANOCELL=$<BOOL:${BRUCE_BUILD_MOD_NANOCELL}>
        BRUCE_BUILD_MOD_MCP=$<BOOL:${BRUCE_BUILD_MOD_MCP}>)
    target_compile_options(bruce_mod_catalog_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME mod_catalog_contracts COMMAND bruce_mod_catalog_tests)
    add_executable(bruce_unified_mod_catalog_v2_tests tests/unit/unified_mod_catalog_v2_tests.cpp)
    bruce_enable_sanitizers(bruce_unified_mod_catalog_v2_tests)
    target_link_libraries(bruce_unified_mod_catalog_v2_tests PRIVATE bruce_mod_catalog)
    target_compile_options(bruce_unified_mod_catalog_v2_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME unified_mod_catalog_v2_contracts COMMAND bruce_unified_mod_catalog_v2_tests)
    add_test(NAME mod_build_language_off COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_mod_build.py
        --cmake ${CMAKE_COMMAND} --source ${CMAKE_CURRENT_SOURCE_DIR}
        --build ${CMAKE_CURRENT_BINARY_DIR}/mod-build-language-off)
    add_test(NAME application_task_catalog COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_application_task.py $<TARGET_FILE:bruce>)
    add_test(NAME run_inspection_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_run_inspection.py $<TARGET_FILE:bruce>)
    add_executable(bruce_execution_session_tests tests/unit/execution_session_tests.cpp)
    bruce_enable_sanitizers(bruce_execution_session_tests)
    target_link_libraries(bruce_execution_session_tests PRIVATE bruce_core)
    target_compile_options(bruce_execution_session_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME execution_session_states COMMAND bruce_execution_session_tests)
    add_test(NAME execution_session_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_execution_session.py $<TARGET_FILE:bruce>)
    add_executable(bruce_grid_replay_control_tests tests/unit/grid_replay_control_tests.cpp)
    bruce_enable_sanitizers(bruce_grid_replay_control_tests)
    target_link_libraries(bruce_grid_replay_control_tests PRIVATE bruce_core)
    target_compile_options(bruce_grid_replay_control_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME grid_replay_control COMMAND bruce_grid_replay_control_tests)
    add_executable(bruce_research_replay_control_tests tests/unit/research_replay_control_tests.cpp)
    bruce_enable_sanitizers(bruce_research_replay_control_tests)
    target_link_libraries(bruce_research_replay_control_tests PRIVATE bruce_core)
    target_compile_options(bruce_research_replay_control_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME research_replay_control COMMAND bruce_research_replay_control_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures)
    add_executable(bruce_grid_research_replay_control_tests tests/unit/grid_research_replay_control_tests.cpp)
    bruce_enable_sanitizers(bruce_grid_research_replay_control_tests)
    target_link_libraries(bruce_grid_research_replay_control_tests PRIVATE bruce_core)
    target_compile_options(bruce_grid_research_replay_control_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME grid_research_replay_control COMMAND bruce_grid_research_replay_control_tests)
    add_executable(bruce_execution_resume_import_control_tests tests/unit/execution_resume_import_control_tests.cpp)
    bruce_enable_sanitizers(bruce_execution_resume_import_control_tests)
    target_link_libraries(bruce_execution_resume_import_control_tests PRIVATE bruce_core)
    target_compile_options(bruce_execution_resume_import_control_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME execution_resume_import_control COMMAND bruce_execution_resume_import_control_tests)
    add_executable(bruce_execution_compatibility_source_control_tests tests/unit/execution_compatibility_source_control_tests.cpp)
    bruce_enable_sanitizers(bruce_execution_compatibility_source_control_tests)
    target_link_libraries(bruce_execution_compatibility_source_control_tests PRIVATE bruce_core)
    target_compile_options(bruce_execution_compatibility_source_control_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME execution_compatibility_source_control COMMAND bruce_execution_compatibility_source_control_tests)
    add_executable(bruce_execution_grid_source_control_tests tests/unit/execution_grid_source_control_tests.cpp)
    bruce_enable_sanitizers(bruce_execution_grid_source_control_tests)
    target_link_libraries(bruce_execution_grid_source_control_tests PRIVATE bruce_core)
    target_compile_options(bruce_execution_grid_source_control_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME execution_grid_source_control COMMAND bruce_execution_grid_source_control_tests)
    add_executable(bruce_execution_experience_control_tests tests/unit/execution_experience_control_tests.cpp)
    bruce_enable_sanitizers(bruce_execution_experience_control_tests)
    target_link_libraries(bruce_execution_experience_control_tests PRIVATE bruce_core)
    target_compile_options(bruce_execution_experience_control_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME execution_experience_control COMMAND bruce_execution_experience_control_tests)
    add_executable(bruce_grid_serialization_control_tests tests/unit/grid_serialization_control_tests.cpp)
    bruce_enable_sanitizers(bruce_grid_serialization_control_tests)
    target_link_libraries(bruce_grid_serialization_control_tests PRIVATE bruce_core)
    target_compile_options(bruce_grid_serialization_control_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME grid_serialization_control COMMAND bruce_grid_serialization_control_tests)
    add_executable(bruce_local_files_tests tests/unit/local_files_tests.cpp)
    bruce_enable_sanitizers(bruce_local_files_tests)
    target_link_libraries(bruce_local_files_tests PRIVATE bruce_core)
    target_compile_options(bruce_local_files_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME local_files_contracts COMMAND bruce_local_files_tests)
    add_executable(bruce_export_control_tests tests/unit/export_control_tests.cpp)
    bruce_enable_sanitizers(bruce_export_control_tests)
    target_link_libraries(bruce_export_control_tests PRIVATE bruce_core)
    target_compile_options(bruce_export_control_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME export_control COMMAND bruce_export_control_tests)
    add_executable(bruce_input_read_control_tests tests/unit/input_read_control_tests.cpp)
    bruce_enable_sanitizers(bruce_input_read_control_tests)
    target_link_libraries(bruce_input_read_control_tests PRIVATE bruce_core)
    target_compile_options(bruce_input_read_control_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME input_read_control COMMAND bruce_input_read_control_tests)
    add_executable(bruce_serialization_control_tests tests/unit/serialization_control_tests.cpp)
    bruce_enable_sanitizers(bruce_serialization_control_tests)
    target_link_libraries(bruce_serialization_control_tests PRIVATE bruce_core)
    target_compile_options(bruce_serialization_control_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME serialization_control COMMAND bruce_serialization_control_tests)
    add_executable(bruce_experience_replay_control_tests tests/unit/experience_replay_control_tests.cpp)
    bruce_enable_sanitizers(bruce_experience_replay_control_tests)
    target_link_libraries(bruce_experience_replay_control_tests PRIVATE bruce_core)
    target_compile_options(bruce_experience_replay_control_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME experience_replay_control COMMAND bruce_experience_replay_control_tests)
    add_executable(bruce_compatibility_replay_control_tests tests/unit/compatibility_replay_control_tests.cpp)
    bruce_enable_sanitizers(bruce_compatibility_replay_control_tests)
    target_link_libraries(bruce_compatibility_replay_control_tests PRIVATE bruce_core)
    target_compile_options(bruce_compatibility_replay_control_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME compatibility_replay_control COMMAND bruce_compatibility_replay_control_tests)
    add_executable(bruce_comparison_replay_control_tests tests/unit/comparison_replay_control_tests.cpp)
    bruce_enable_sanitizers(bruce_comparison_replay_control_tests)
    target_link_libraries(bruce_comparison_replay_control_tests PRIVATE bruce_core)
    target_compile_options(bruce_comparison_replay_control_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME comparison_replay_control COMMAND bruce_comparison_replay_control_tests)
    add_executable(bruce_dag_scheduler_v1_tests tests/unit/dag_scheduler_v1_tests.cpp)
    bruce_enable_sanitizers(bruce_dag_scheduler_v1_tests)
    target_link_libraries(bruce_dag_scheduler_v1_tests PRIVATE bruce_core)
    target_compile_options(bruce_dag_scheduler_v1_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME dag_scheduler_v1_contracts COMMAND bruce_dag_scheduler_v1_tests)
    add_executable(bruce_dag_scheduler_v2_tests tests/unit/dag_scheduler_v2_tests.cpp)
    bruce_enable_sanitizers(bruce_dag_scheduler_v2_tests)
    target_link_libraries(bruce_dag_scheduler_v2_tests PRIVATE bruce_core)
    target_compile_options(bruce_dag_scheduler_v2_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME dag_scheduler_v2_contracts COMMAND bruce_dag_scheduler_v2_tests)
    add_executable(bruce_dag_scheduler_v4_tests tests/unit/dag_scheduler_v4_tests.cpp)
    bruce_enable_sanitizers(bruce_dag_scheduler_v4_tests)
    target_link_libraries(bruce_dag_scheduler_v4_tests PRIVATE bruce_core)
    target_compile_options(bruce_dag_scheduler_v4_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME dag_scheduler_v4_contracts COMMAND bruce_dag_scheduler_v4_tests)
    add_executable(bruce_dag_scheduler_v3_cli_tests tests/unit/dag_scheduler_v3_cli_tests.cpp)
    bruce_enable_sanitizers(bruce_dag_scheduler_v3_cli_tests)
    target_link_libraries(bruce_dag_scheduler_v3_cli_tests PRIVATE bruce_core)
    target_compile_options(bruce_dag_scheduler_v3_cli_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME dag_scheduler_v3_cli_contracts COMMAND bruce_dag_scheduler_v3_cli_tests)
    add_executable(bruce_dag_scheduler_v5_cli_tests tests/unit/dag_scheduler_v5_cli_tests.cpp)
    bruce_enable_sanitizers(bruce_dag_scheduler_v5_cli_tests)
    target_link_libraries(bruce_dag_scheduler_v5_cli_tests PRIVATE bruce_core)
    target_compile_options(bruce_dag_scheduler_v5_cli_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME dag_scheduler_v5_cli_contracts COMMAND bruce_dag_scheduler_v5_cli_tests $<TARGET_FILE:bruce>)
    add_executable(bruce_dag_scheduler_v5_publish_cli_tests tests/unit/dag_scheduler_v5_publish_cli_tests.cpp)
    bruce_enable_sanitizers(bruce_dag_scheduler_v5_publish_cli_tests)
    target_link_libraries(bruce_dag_scheduler_v5_publish_cli_tests PRIVATE bruce_core)
    target_compile_options(bruce_dag_scheduler_v5_publish_cli_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME dag_scheduler_v5_publish_cli_contracts COMMAND bruce_dag_scheduler_v5_publish_cli_tests)
    add_executable(bruce_dag_scheduler_v5_payload_cli_tests tests/unit/dag_scheduler_v5_payload_cli_tests.cpp)
    bruce_enable_sanitizers(bruce_dag_scheduler_v5_payload_cli_tests)
    target_link_libraries(bruce_dag_scheduler_v5_payload_cli_tests PRIVATE bruce_core)
    target_compile_options(bruce_dag_scheduler_v5_payload_cli_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME dag_scheduler_v5_payload_cli_contracts COMMAND bruce_dag_scheduler_v5_payload_cli_tests)
    add_executable(bruce_dag_scheduler_v5_payload_inspector_cli_tests tests/unit/dag_scheduler_v5_payload_inspector_cli_tests.cpp)
    bruce_enable_sanitizers(bruce_dag_scheduler_v5_payload_inspector_cli_tests)
    target_link_libraries(bruce_dag_scheduler_v5_payload_inspector_cli_tests PRIVATE bruce_core)
    target_compile_options(bruce_dag_scheduler_v5_payload_inspector_cli_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME dag_scheduler_v5_payload_inspector_cli_contracts COMMAND bruce_dag_scheduler_v5_payload_inspector_cli_tests)
    add_test(NAME dag_scheduler_v5_payload_inspector_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_dag_scheduler_v5_payload_inspector.py $<TARGET_FILE:bruce>)
    add_test(NAME dag_scheduler_v5_cli_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_dag_scheduler_v5_cli.py $<TARGET_FILE:bruce>)
    add_test(NAME common_runtime_host_v2_stage3_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_common_runtime_host_v2_stage3.py
        $<TARGET_FILE:bruce> $<TARGET_FILE:bruce_common_runtime_host_v2_acceptance_fixture>)
    add_executable(bruce_common_runtime_host_v2_acceptance_fixture
        tests/integration/common_runtime_host_v2_acceptance_fixture.cpp)
    bruce_enable_sanitizers(bruce_common_runtime_host_v2_acceptance_fixture)
    target_link_libraries(bruce_common_runtime_host_v2_acceptance_fixture PRIVATE bruce_core)
    target_compile_options(bruce_common_runtime_host_v2_acceptance_fixture PRIVATE
        -Wall -Wextra -Werror)
    add_test(NAME common_runtime_host_v2_stage3b_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_common_runtime_host_v2_stage3b.py $<TARGET_FILE:bruce>)
    add_executable(bruce_common_runtime_host_v2_stage3c_fixture
        tests/integration/common_runtime_host_v2_stage3c_fixture.cpp)
    bruce_enable_sanitizers(bruce_common_runtime_host_v2_stage3c_fixture)
    target_link_libraries(bruce_common_runtime_host_v2_stage3c_fixture PRIVATE bruce_core)
    target_compile_options(bruce_common_runtime_host_v2_stage3c_fixture PRIVATE -Wall -Wextra -Werror)
    add_executable(bruce_common_runtime_host_v2_stage3d_fixture
        tests/integration/common_runtime_host_v2_stage3d_fixture.cpp)
    bruce_enable_sanitizers(bruce_common_runtime_host_v2_stage3d_fixture)
    target_link_libraries(bruce_common_runtime_host_v2_stage3d_fixture PRIVATE bruce_core)
    target_compile_options(bruce_common_runtime_host_v2_stage3d_fixture PRIVATE -Wall -Wextra -Werror)
    add_test(NAME common_runtime_host_v2_stage3d_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_common_runtime_host_v2_stage3d.py
        $<TARGET_FILE:bruce_common_runtime_host_v2_stage3d_fixture> $<TARGET_FILE:bruce>)
    add_test(NAME common_runtime_host_v2_stage3c_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_common_runtime_host_v2_stage3c.py
        $<TARGET_FILE:bruce_common_runtime_host_v2_stage3c_fixture>)
    add_test(NAME dag_scheduler_v5_abort_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_dag_scheduler_v5_abort.py $<TARGET_FILE:bruce>)
    add_test(NAME dag_scheduler_v5_handoff_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_dag_scheduler_v5_handoff.py $<TARGET_FILE:bruce>)
    add_test(NAME dag_scheduler_v5_payload_flow_inspector_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_dag_scheduler_v5_payload_flow_inspector.py $<TARGET_FILE:bruce>)
    add_test(NAME dag_scheduler_v5_inspector_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_dag_scheduler_v5_inspector.py $<TARGET_FILE:bruce>)
    add_test(NAME dag_scheduler_v5_publish_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_dag_scheduler_v5_publish.py $<TARGET_FILE:bruce>)
    add_test(NAME dag_scheduler_v5_payload_workflows COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_dag_scheduler_v5_payload.py $<TARGET_FILE:bruce>)
    add_executable(bruce_phase_dag_tests tests/unit/phase_dag_tests.cpp)
    bruce_enable_sanitizers(bruce_phase_dag_tests)
    target_link_libraries(bruce_phase_dag_tests PRIVATE bruce_core)
    target_compile_options(bruce_phase_dag_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME phase_dag_contracts COMMAND bruce_phase_dag_tests)
    add_executable(bruce_phase_dag_v2_tests tests/unit/phase_dag_v2_tests.cpp)
    bruce_enable_sanitizers(bruce_phase_dag_v2_tests)
    target_link_libraries(bruce_phase_dag_v2_tests PRIVATE bruce_core)
    target_compile_options(bruce_phase_dag_v2_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME phase_dag_v2_contracts COMMAND bruce_phase_dag_v2_tests)
    add_executable(bruce_phase_dag_v3_tests tests/unit/phase_dag_v3_tests.cpp)
    bruce_enable_sanitizers(bruce_phase_dag_v3_tests)
    target_link_libraries(bruce_phase_dag_v3_tests PRIVATE bruce_core)
    target_compile_options(bruce_phase_dag_v3_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME phase_dag_v3_contracts COMMAND bruce_phase_dag_v3_tests)
    add_executable(bruce_toy_relation_v1_tests tests/unit/toy_relation_v1_tests.cpp)
    bruce_enable_sanitizers(bruce_toy_relation_v1_tests)
    target_link_libraries(bruce_toy_relation_v1_tests PRIVATE bruce_core)
    target_compile_options(bruce_toy_relation_v1_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME toy_relation_v1_contracts COMMAND bruce_toy_relation_v1_tests)
    add_executable(bruce_signed_affine_dag_v1_tests tests/unit/signed_affine_dag_v1_tests.cpp)
    bruce_enable_sanitizers(bruce_signed_affine_dag_v1_tests)
    target_link_libraries(bruce_signed_affine_dag_v1_tests PRIVATE bruce_core)
    target_compile_options(bruce_signed_affine_dag_v1_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME signed_affine_dag_v1_contracts COMMAND bruce_signed_affine_dag_v1_tests)
    add_executable(bruce_signed_affine_relation_v1_tests tests/unit/signed_affine_relation_v1_tests.cpp)
    bruce_enable_sanitizers(bruce_signed_affine_relation_v1_tests)
    target_link_libraries(bruce_signed_affine_relation_v1_tests PRIVATE bruce_core)
    target_compile_options(bruce_signed_affine_relation_v1_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME signed_affine_relation_v1_contracts COMMAND bruce_signed_affine_relation_v1_tests)
    add_test(NAME sanitizer_configuration COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_sanitizer_configuration.py)
    add_test(NAME ci_workflow_contract COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_ci_contract.py)
    list(APPEND _bruce_sanitizer_excluded ci_workflow_contract)
    set(_bruce_sanitizer_reason_ci_workflow_contract
        "Workflow source and shell syntax contract only - no instrumented C++ process")
    list(APPEND _bruce_sanitizer_compatible release_signature_v1_contracts release_tool_workflows)
    bruce_classify_sanitizer_tests()
endif()
install(TARGETS bruce bruce-mod-builder RUNTIME DESTINATION bin)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/metadata/$<CONFIG>/build-info.json"
    "${CMAKE_CURRENT_BINARY_DIR}/source-files.sha256" DEPENDENCIES.md LICENSE NOTICE THIRD_PARTY_NOTICES.md DESTINATION share/bruce)
set(CPACK_PACKAGE_NAME "bruce-core")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_GENERATOR "TGZ")
set(CPACK_PACKAGE_FILE_NAME "bruce-core-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
include(CPack)
