# Common Runtime Host V1 `grid-resume` — Implementierungsplan

> Subagent-driven, taskweise Umsetzung mit TDD und unabhängigem Review.

**Ziel:** Den bestehenden Host additiv auf `grid-resume` erweitern und die
Familienprüfung vor Parent-Bundle-Lesen ausführen, ohne bestehende Resume-
Semantik zu verändern.

## Dateien

Neu:

- `tests/integration/test_common_runtime_compatibility_v1_stage2_grid_resume.py`
- `runs/common-runtime-host-v1-grid-resume.md`

Zu ändern:

- `include/bruce/application/common_runtime_host_v1.hpp`
- `src/application/common_runtime_host_v1.cpp`
- `apps/bruce_cli.cpp`
- `tests/unit/common_runtime_host_v1_tests.cpp`
- `CMakeLists.txt`
- `docs/contracts/06-control-and-publication.md`
- `docs/implementation/184-control-contract-matrix-usage.md`

## Task 1 — Hostvertrag und Unit-Tests

1. Zuerst Tests für `grid-resume` canonical, falschen Entrypoint, falsche
   Familie, mutierten Descriptor, leeren Delegate, genau-einmalige Delegation
   und Exception-Passthrough schreiben.
2. Host-Allowlist exakt auf `compare`, `grid`, `grid-batch`, `grid-resume`
   erweitern.
3. Keine Requestfelder für Parentpfad, Queries, Budgets, Bundle oder Output
   hinzufügen.

## Task 2 — CLI-Wiring und Integration

1. `options(...)` beibehalten.
2. Für die explizite Familienoption den Host direkt danach validieren.
3. Parent-Bundle-Lesen, `additional-queries`, Signalinstallation,
   `GridResumeTask` und `execute_cli` in einen einzigen Delegate legen.
4. Legacy ohne Option direkt lassen.
5. Integrationstest mit Erfolg, vollständigem/partiellem Bundle, Fehlerpfad,
   Replay/Budget/Cancellation, Parsergrenzen und Vorabvalidierung ergänzen.
6. CTest `common_runtime_compatibility_v1_stage2_grid_resume` registrieren.

## Task 3 — Regression und Dokumentation

Fokussiert ausführen:

```sh
cmake --build /tmp/bruce-v14-core-build -j2
ctest --test-dir /tmp/bruce-v14-core-build -R '^(common_runtime_host_v1_contracts|common_runtime_compatibility_v1_stage2_compare|common_runtime_compatibility_v1_stage2_grid|common_runtime_compatibility_v1_stage2_grid_batch|common_runtime_compatibility_v1_stage2_grid_resume|common_runtime_compatibility_v1_contracts|common_runtime_matrix_binding|common_runtime_compatibility_v1_workflows|common_runtime_compatibility_v1_stage1_gate|common_runtime_compatibility_v1_stage1_management|common_runtime_compatibility_v1_stage1_transfer|common_runtime_compatibility_v1_stage1_session|execution_session_states|execution_session_workflows|grid_resume_workflows|resume_copy_control|resume_copy_workflows|control_contract_matrix|control_contract_matrix_unit)$' --output-on-failure
```

Danach JSON-Parsing, lokale Markdown-Links, Python-Syntax und den vollständigen
CLI-Verifier in einer temporären Repository-Kopie prüfen. Generierte CLI-
Artefakte ausschließlich aus dieser Kopie übernehmen.

## Task 4 — Review

Ein unabhängiger Review prüft insbesondere die Grenze vor Parent-Bundle-Lesen,
Bytegleichheit, keine Budgetaggregation, genau-einmalige Delegation und keine
Stage-3-Ausweitung. Erst danach darf die Spec als **IMPLEMENTIERT UND BELEGT**
markiert werden.
