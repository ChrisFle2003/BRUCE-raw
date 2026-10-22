# Instrument only explicitly selected targets. Static sandbox workers deliberately
# keep their original -static -s link: ASan cannot run inside that boundary.
option(BRUCE_ENABLE_ASAN_UBSAN "Enable target-scoped AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if(BRUCE_ENABLE_ASAN_UBSAN AND NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(FATAL_ERROR "BRUCE_ENABLE_ASAN_UBSAN requires GCC or Clang")
endif()

function(bruce_enable_sanitizers target)
    if(BRUCE_ENABLE_ASAN_UBSAN)
        target_compile_options(${target} PRIVATE
            -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
endfunction()

# An explicit exhaustive partition, including tests which do not execute C++.
# Adding a CTest without a reviewed classification is a configuration error.
set(_bruce_sanitizer_compatible
    adaptive_portfolio_contracts
    adaptive_portfolio_workflows
    adaptive_research_workflows
    admission_diagnostics_v1_contracts
    application_task_adapters
    application_task_catalog
    artifact_payload_external_v1_contracts
    artifact_payload_stream_v1_contracts
    artifact_payload_v1_contracts
    artifact_store_resolve_v1_cli_workflows
    artifact_store_v1_cli_contracts
    artifact_store_v1_cli_workflows
    artifact_store_v1_contracts
    artifact_store_v2_cli_contracts
    artifact_store_v2_contracts
    artifact_store_v3_cli_contracts
    artifact_store_v3_contracts
    artifact_store_v4_contracts
    bruce_mcp_workflows
    builtin_mod_help_workflows
    cached_expression_contracts
    cached_expression_workflows
    cached_portfolio_contracts
    cached_portfolio_workflows
    cli_documentation_determinism
    cli_version
    cli_workflows
    common_runtime_compatibility_v1_contracts
    common_runtime_compatibility_v1_stage1_gate
    common_runtime_compatibility_v1_stage1_management
    common_runtime_compatibility_v1_stage1_session
    common_runtime_compatibility_v1_stage1_transfer
    common_runtime_compatibility_v1_stage2_compare
    common_runtime_compatibility_v1_stage2_grid
    common_runtime_compatibility_v1_stage2_grid_batch
    common_runtime_compatibility_v1_stage2_grid_resume
    common_runtime_compatibility_v1_workflows
    common_runtime_host_v1_contracts
    common_runtime_matrix_binding
    comparison_replay_control
    compatibility_contracts
    compatibility_replay_control
    compatibility_transfer_workflows
    compatibility_workflows
    control_contract_matrix
    control_flow_acceptance
    controller_observability_cli_v1_contracts
    controller_observability_crosswalk_v1_contracts
    core_contracts
    dag_scheduler_v1_cli_workflows
    dag_scheduler_v1_contracts
    dag_scheduler_v2_cli_workflows
    dag_scheduler_v2_contracts
    dag_scheduler_v3_cli_contracts
    dag_scheduler_v3_cli_workflows
    dag_scheduler_v4_cli_workflows
    dag_scheduler_v4_contracts
    dag_scheduler_v5_payload_cli_contracts
    dag_scheduler_v5_payload_inspector_cli_contracts
    dag_scheduler_v5_publish_cli_contracts
    domain_cache_contracts
    domain_cache_portfolio_contracts
    domain_cache_portfolio_workflows
    domain_cache_workflows
    evidence_chain_contracts
    evidence_chain_workflows
    exact_domain_contracts
    exact_domain_workflows
    execute_handoff_inspector_v1_contracts
    execute_handoff_lineage_v1_contracts
    execution_compatibility_source_control
    execution_experience_control
    execution_grid_source_control
    execution_resume_import_control
    execution_session_states
    execution_session_workflows
    experience_contracts
    experience_header_boundary
    experience_replace_workflows
    experience_replay_control
    experience_transfer_workflows
    experience_workflows
    export_control
    expression_contracts
    expression_equivalence_contracts
    expression_equivalence_workflows
    expression_operators_contracts
    expression_operators_workflows
    expression_portfolio_contracts
    expression_portfolio_workflows
    expression_powers_contracts
    expression_powers_workflows
    expression_workflows
    five_experience_contracts
    five_experience_workflows
    fraction_comparison_contracts
    fraction_comparison_workflows
    grid_axis_workflows
    grid_budget_workflows
    grid_cache_contracts
    grid_cache_workflows
    grid_contracts
    grid_experience_budget_workflows
    grid_experience_workflows
    grid_partial_workflows
    grid_replay_control
    grid_replay_workflows
    grid_research_contracts
    grid_research_replay_control
    grid_research_workflows
    grid_resume_workflows
    grid_serialization_control
    grid_stage_sweep_contracts
    grid_stage_verify_contracts
    indexed_expression_contracts
    indexed_expression_workflows
    indexed_portfolio_contracts
    indexed_portfolio_workflows
    input_read_control
    installed_cli_workflows
    installed_mod_host_v2_contracts
    json_encoding_isolation
    language_contracts
    language_run_contracts
    language_workflows
    local_builder_cli_contracts
    local_builder_contracts
    local_data_mod_example_workflows
    local_files_contracts
    local_logic_builder_contracts
    logic_mod_payload_fifo_lock_v1_contracts
    logic_mod_payload_fifo_manifest_v1_contracts
    logic_mod_payload_v1_contracts
    logic_mod_plugin_v1_contracts
    logic_mod_registry_v1_contracts
    logic_mod_result_artifact_fifo_preflight_v1_contracts
    logic_mod_result_artifact_fifo_swap_v1_contracts
    logic_mod_result_artifact_v1_contracts
    logic_mod_result_inspector_record_fifo_v1_contracts
    logic_mod_result_inspector_record_symlink_fifo_v1_contracts
    logic_mod_result_inspector_record_symlink_regular_v1_contracts
    logic_mod_result_inspector_v1_contracts
    logic_mod_supervision_log_v6_contracts
    management_inspection_control
    mcp_inspector_contracts
    mcp_inspector_workflows
    mod_api_contracts
    mod_catalog_contracts
    mod_update_v2_contracts
    modular_comparison_workflows
    modular_expression_workflows
    modular_portfolio_workflows
    nanocell_cli_workflows
    nanocell_contracts
    nanocell_mod_contracts
    package_inspection_v2_contracts
    payload_flow_inspector_v2_contracts
    payload_flow_v2_contracts
    payload_run_inspector_v1_contracts
    payload_run_stream_inspector_v1_contracts
    phase_dag_contracts
    phase_dag_nanocell_contracts
    phase_dag_payload_v3_contracts
    phase_dag_v2_cli_workflows
    phase_dag_v2_contracts
    phase_dag_v3_cli_workflows
    phase_dag_v3_contracts
    polynomial_contracts
    polynomial_research_workflows
    portfolio_contracts
    portfolio_operators_contracts
    portfolio_operators_workflows
    portfolio_powers_contracts
    portfolio_powers_workflows
    portfolio_workflows
    power_research_workflows
    q_expression_contracts
    q_expression_workflows
    q_portfolio_contracts
    q_portfolio_workflows
    quadratic_portfolio_contracts
    quadratic_portfolio_workflows
    rational_linear_contracts
    rational_linear_workflows
    rational_portfolio_contracts
    rational_portfolio_workflows
    rational_quadratic_contracts
    rational_quadratic_workflows
    rational_witness_contracts
    rational_witness_workflows
    research_contract_catalog
    research_contract_matrix
    research_gate_isolation
    research_replay_control
    result_assessment_workflows
    resume_copy_control
    resume_copy_workflows
    rule_history_contracts
    rule_proposal_contracts
    rule_review_workflows
    rule_store_workflows
    run_inspection_workflows
    runtime_mod_cli_contracts
    runtime_orchestrator_v1_contracts
    runtime_package_contracts
    runtime_registry_contracts
    serialization_control
    serialized_byte_budget_workflows
    session_work_budget_contracts
    session_work_budget_workflows
    signed_affine_dag_v1_cli_workflows
    signed_affine_dag_v1_contracts
    signed_affine_relation_v1_cli_workflows
    signed_affine_relation_v1_contracts
    task_spec_contracts
    task_spec_workflows
    tokenized_task_run_contracts
    toy_relation_v1_cli_workflows
    toy_relation_v1_contracts
    tree_registry_contracts
    unified_mod_catalog_v2_contracts
    witness_expression_contracts
    witness_expression_workflows
    witness_portfolio_contracts
    witness_portfolio_workflows
    worker_abort_control_v1_contracts
    worker_boundary_v1_contracts
    worker_run_inspector_v1_contracts
    worker_run_log_v1_contracts
)

set(_bruce_static_fixture_dependent
    controller_observability_cli_v1_v6_contracts
    installed_logic_mod_run_workflows
    local_logic_builder_cli_contracts
    logic_mod_artifact_chain_preflight_v1_contracts
    logic_mod_artifact_chain_v1_contracts
    logic_mod_artifact_chain_v1_workflows
    logic_mod_artifact_dag_v2_contracts
    logic_mod_artifact_dag_v2_preflight_contracts
    logic_mod_artifact_dag_v2_workflows
    logic_mod_artifact_dag_v3_contracts
    logic_mod_artifact_dag_v3_workflows
    logic_mod_artifact_dag_v4_contracts
    logic_mod_artifact_dag_v4_workflows
    logic_mod_artifact_dag_v5_cli_workflows
    logic_mod_artifact_dag_v5_contracts
    logic_mod_artifact_dag_v6_cli_workflows
    logic_mod_artifact_dag_v6_contracts
    logic_mod_lifecycle_workflows
    logic_mod_payload_v1_workflows
    logic_mod_payload_worker_v1_contracts
    logic_mod_plugin_v1_workflows
    logic_mod_result_artifact_v1_workflows
    logic_mod_result_inspector_v1_workflows
    logic_mod_worker_attempt_v6_contracts
    logic_mod_worker_v1_contracts
    mod_package_inspection_workflows
    mod_update_workflows
    runtime_orchestrator_v1_cli_workflows)

set(_bruce_sanitizer_excluded
    artifact_store_v2_cli_workflows
    artifact_store_v3_cli_workflows
    artifact_store_v4_cli_workflows
    common_runtime_host_v2_batch_contracts
    common_runtime_host_v2_contracts
    common_runtime_host_v2_stage3_workflows
    common_runtime_host_v2_stage3b_workflows
    common_runtime_host_v2_stage3c_workflows
    common_runtime_host_v2_stage3d_workflows
    controller_observability_cli_v1_workflows
    dag_scheduler_v5_abort_workflows
    dag_scheduler_v5_cli_contracts
    dag_scheduler_v5_cli_workflows
    dag_scheduler_v5_handoff_workflows
    dag_scheduler_v5_inspector_workflows
    dag_scheduler_v5_payload_flow_inspector_workflows
    dag_scheduler_v5_payload_inspector_workflows
    dag_scheduler_v5_payload_workflows
    dag_scheduler_v5_publish_workflows
    runtime_mod_workflows
    worker_supervisor_v1_contracts
    benchmark_runner_contracts
    builtin_mod_build_matrix
    control_contract_matrix_unit
    current_source_verification_boundaries
    host_lifecycle_contract_matrix_v1
    mod_build_language_off
    sanitizer_configuration
    with_host_deps_contract
    with_host_deps_wrapper_boundaries)
set(_bruce_sanitizer_reason_benchmark_runner_contracts "Python benchmark runner tests only - no instrumented C++ process")
set(_bruce_sanitizer_reason_artifact_store_v2_cli_workflows "Reexecutes an instrumented worker under unchanged RLIMIT_AS - ASan shadow memory cannot initialize")
set(_bruce_sanitizer_reason_artifact_store_v3_cli_workflows "Known sanitizer-only forked HTTPS transport failure - child exits 1 after TLS request - timing diagnosis remains open")
set(_bruce_sanitizer_reason_artifact_store_v4_cli_workflows "Known sanitizer-only forked HTTPS transport failure - child exits 1 after TLS request - timing diagnosis remains open")
set(_bruce_sanitizer_reason_common_runtime_host_v2_batch_contracts "Reexecutes an instrumented worker under unchanged RLIMIT_AS - ASan shadow memory cannot initialize")
set(_bruce_sanitizer_reason_common_runtime_host_v2_contracts "Reexecutes an instrumented worker under unchanged RLIMIT_AS - ASan shadow memory cannot initialize")
set(_bruce_sanitizer_reason_common_runtime_host_v2_stage3_workflows "Reexecutes an instrumented worker under unchanged RLIMIT_AS - ASan shadow memory cannot initialize")
set(_bruce_sanitizer_reason_common_runtime_host_v2_stage3b_workflows "Reexecutes an instrumented worker under unchanged RLIMIT_AS - ASan shadow memory cannot initialize")
set(_bruce_sanitizer_reason_common_runtime_host_v2_stage3c_workflows "Reexecutes an instrumented worker under unchanged RLIMIT_AS - ASan shadow memory cannot initialize")
set(_bruce_sanitizer_reason_common_runtime_host_v2_stage3d_workflows "Reexecutes an instrumented worker under unchanged RLIMIT_AS - ASan shadow memory cannot initialize")
set(_bruce_sanitizer_reason_controller_observability_cli_v1_workflows "Reexecutes an instrumented worker under unchanged RLIMIT_AS - ASan shadow memory cannot initialize")
set(_bruce_sanitizer_reason_dag_scheduler_v5_abort_workflows "Reexecutes an instrumented worker under unchanged RLIMIT_AS - ASan shadow memory cannot initialize")
set(_bruce_sanitizer_reason_dag_scheduler_v5_cli_contracts "Reexecutes an instrumented worker under unchanged RLIMIT_AS - ASan shadow memory cannot initialize")
set(_bruce_sanitizer_reason_dag_scheduler_v5_cli_workflows "Reexecutes an instrumented worker under unchanged RLIMIT_AS - ASan shadow memory cannot initialize")
set(_bruce_sanitizer_reason_dag_scheduler_v5_handoff_workflows "Reexecutes an instrumented worker under unchanged RLIMIT_AS - ASan shadow memory cannot initialize")
set(_bruce_sanitizer_reason_dag_scheduler_v5_inspector_workflows "Reexecutes an instrumented worker under unchanged RLIMIT_AS - ASan shadow memory cannot initialize")
set(_bruce_sanitizer_reason_dag_scheduler_v5_payload_flow_inspector_workflows "Reexecutes an instrumented worker under unchanged RLIMIT_AS - ASan shadow memory cannot initialize")
set(_bruce_sanitizer_reason_dag_scheduler_v5_payload_inspector_workflows "Reexecutes an instrumented worker under unchanged RLIMIT_AS - ASan shadow memory cannot initialize")
set(_bruce_sanitizer_reason_dag_scheduler_v5_payload_workflows "Reexecutes an instrumented worker under unchanged RLIMIT_AS - ASan shadow memory cannot initialize")
set(_bruce_sanitizer_reason_dag_scheduler_v5_publish_workflows "Reexecutes an instrumented worker under unchanged RLIMIT_AS - ASan shadow memory cannot initialize")
set(_bruce_sanitizer_reason_runtime_mod_workflows "LD_PRELOAD race-test barriers precede libasan - sanitizer runtime ordering rejects startup")
set(_bruce_sanitizer_reason_worker_supervisor_v1_contracts "Reexecutes an instrumented worker under unchanged RLIMIT_AS - ASan shadow memory cannot initialize")
set(_bruce_sanitizer_reason_builtin_mod_build_matrix "Configures and runs its own unsanitized build matrix")
set(_bruce_sanitizer_reason_control_contract_matrix_unit "Python verifier unit tests only - no instrumented C++ process")
set(_bruce_sanitizer_reason_current_source_verification_boundaries "Verification script boundary tests only - no instrumented C++ process")
set(_bruce_sanitizer_reason_host_lifecycle_contract_matrix_v1 "Source contract matrix checker only - no instrumented C++ process")
set(_bruce_sanitizer_reason_mod_build_language_off "Configures its own unsanitized all-mods-disabled build")
set(_bruce_sanitizer_reason_sanitizer_configuration "Generated compiler and linker graph checks only - no runtime coverage")
set(_bruce_sanitizer_reason_with_host_deps_contract "Shell wrapper contract only - no instrumented C++ process")
set(_bruce_sanitizer_reason_with_host_deps_wrapper_boundaries "Dependency path wrapper tests only - no instrumented C++ process")

# Invoke in each test-owning directory (CTest properties are directory scoped).
function(bruce_classify_sanitizer_tests)
    get_property(tests DIRECTORY PROPERTY TESTS)
    foreach(test IN LISTS tests)
        set(classes "")
        if(test IN_LIST _bruce_sanitizer_compatible)
            list(APPEND classes sanitizer-compatible)
        endif()
        if(test IN_LIST _bruce_static_fixture_dependent)
            list(APPEND classes static-fixture-dependent)
        endif()
        if(test IN_LIST _bruce_sanitizer_excluded)
            list(APPEND classes sanitizer-excluded)
        endif()
        list(LENGTH classes class_count)
        if(NOT class_count EQUAL 1)
            message(FATAL_ERROR "CTest ${test} needs exactly one sanitizer classification: ${classes}")
        endif()
        set_property(TEST ${test} APPEND PROPERTY LABELS ${classes})
        if(classes STREQUAL "static-fixture-dependent")
            set_property(TEST ${test} APPEND PROPERTY LABELS
                "sanitizer-reason:depends on unsanitized static sandbox worker fixtures - run in separate Debug tree")
        elseif(classes STREQUAL "sanitizer-excluded")
            set_property(TEST ${test} APPEND PROPERTY LABELS
                "sanitizer-reason:${_bruce_sanitizer_reason_${test}}")
        endif()
    endforeach()
endfunction()
