#!/usr/bin/env python3
"""Read-only source binding check for Host Lifecycle Contract Matrix V1."""

from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]


def require(text: str, fragment: str, source: Path) -> None:
    if fragment not in text:
        raise RuntimeError(f"missing source binding {fragment!r} in {source.relative_to(ROOT)}")


def source(path: str, fragments: tuple[str, ...]) -> None:
    file = ROOT / path
    text = file.read_text(encoding="utf-8")
    for fragment in fragments:
        require(text, fragment, file)


def main() -> int:
    matrix = ROOT / "docs/contracts/07-host-lifecycle-contract-matrix-v1.md"
    text = matrix.read_text(encoding="utf-8")
    for fragment in (
        "`v5-batch-host-v2`",
        "`logic-mod-artifact-dag-v6`",
        "`runtime-orchestrator-v1`",
        "Keiner dieser Einträge ist eine Familie von",
        "**keine** gemeinsame Runtime",
        "Cancellation-API, **keine** gemeinsame Publication-API",
        "**kein** gemeinsames\nRecovery-/Resume-Protokoll",
        "V6\nbleibt dabei Owner",
        "Keine Queue-Datei, kein Journal",
        "bereits akzeptierter Erfolg kann die spätere Latch-Race gewinnen",
        "Planadapter kann den bestehenden akzeptierten V6-Record",
    ):
        require(text, fragment, matrix)

    source("include/bruce/application/common_runtime_host_v2_batch.hpp", (
        "request_cancel_batch_v2",
        "finish_batch_v2",
        "CommonRuntimeHostV2BatchContext",
    ))
    source("src/application/common_runtime_host_v2_batch.cpp", (
        '"CANCEL_REQUESTED"',
        '"RUN_FINISHED"',
    ))
    source("include/bruce/mod/logic_mod_artifact_dag_v6.hpp", (
        "host_cancel_latch",
        "run_logic_mod_artifact_dag_v6",
    ))
    source("src/mod/logic_mod_artifact_dag_v6.cpp", (
        "DAG_HOST_CANCELLED",
        "host_cancelled(control)",
    ))
    source("src/mod/logic_mod_artifact_dag_v6_plan.cpp", (
        "result.accepted && request_.publish_record",
        "logic_mod_artifact_dag_record_v6(result)",
    ))
    source("include/bruce/runtime/runtime_orchestrator_v1.hpp", (
        "RuntimeOrchestratorReservationV1",
        "RuntimeOrchestratorReasonV1",
        "RuntimeOrchestratorStateV1",
    ))
    source("src/runtime/runtime_orchestrator_v1.cpp", (
        "CANCELLED_QUEUED",
        "CANCELLED_ACTIVE",
        "V6_CANCELLED_EXTERNALLY",
        "reservation_held",
    ))
    print("host_lifecycle_contract_matrix_v1: PASS")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError) as error:
        print(f"host_lifecycle_contract_matrix_v1: FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
