# Logic-Mod Artifact DAG V6 – Task 5: Dokumentation und Abnahme

**Status:** **IMPLEMENTIERT UND BELEGT** für den dokumentierten V6-Umfang.

## Aktualisierte Verträge

- `docs/cli/commands.md` dokumentiert den öffentlichen V6-`run`- und
  `replay`-Vertrag, das verpflichtende `--report`, weitergereichte V4-Budgets
  und den V5-Remote-Restore.
- `docs/cli/formats.md` trennt die unveränderten kanonischen V2-Bytes vom
  privaten V6-Sidecar.
- `docs/architecture/03-blueprint-consolidation.md` ordnet V6 als
  Linux-lokale, hostseitige Supervision ein.
- Die V6-Spezifikation ist vom Planungsstatus auf den belegten Iststand
  aktualisiert.

## Belegte Grenzen

- V6-`run` besitzt pro gestartetem Node den direkten Linux-Attempt. Die feste
  Host-Cancel-Eskalation ist 1000 ms Grace, `SIGTERM`, 50 ms Grace,
  `SIGKILL`; sie ist keine Worker-, Spec- oder CLI-Steuerung.
- `REPORT.run/control/supervision-v6.jsonl` ist owner-only (`0700`/`0600`),
  descriptor-relativ und fail-closed. Ein vollständig kanonisches, noch nicht
  terminales Lifecycle-Präfix ist ein Diagnosezwischenstand, aber kein Beweis;
  syntaktisch abgeschnittene oder feindliche Historien werden abgewiesen.
- Peak-Telemetrie ist optional und markiert `OBSERVED`, `UNAVAILABLE` oder
  `NOT_APPLICABLE`; fehlende Daten werden nicht als Nullwert dargestellt.
- V2–V5-Record- und Replay-Bytes bleiben erhalten. V6-Replay bleibt offline,
  workerfrei und sidecarfrei.

V6 beansprucht keine cgroup- oder hostweite Ressourcenaggregation, keine
GPU-/Netzwerk-/Remote-Worker, keine freien Codeplugins, keine Retries und
keine verifizierte öffentliche `MEMORY_LIMIT`-Klassifikation. Unklare
Prozessenden sind `PROCESS_CRASHED`.

## Verifikation

Die zugehörigen ausführbaren Belege sind
`tests/unit/logic_mod_worker_attempt_v6_tests.cpp`,
`tests/unit/logic_mod_supervision_log_v6_tests.cpp`,
`tests/unit/logic_mod_artifact_dag_v5_tests.cpp` (als V6-CMake-Target) und
`tests/integration/test_logic_mod_artifact_dag_v6_cli.py`.

Am 2026-10-17 wurden im vorhandenen `/tmp/bruce-build` ausgeführt:

```text
cmake --build /tmp/bruce-build --target logic_mod_artifact_dag_v6_tests \
  bruce_logic_mod_worker_attempt_v6_tests bruce_logic_mod_supervision_log_v6_tests bruce
ctest --test-dir /tmp/bruce-build -R 'logic_mod_(worker_attempt_v6|supervision_log_v6|artifact_dag_v6)_contracts|logic_mod_artifact_dag_v6_cli_workflows' --output-on-failure
```

Ergebnis: Exit 0; 4/4 Tests grün (`logic_mod_artifact_dag_v6_contracts`,
`logic_mod_worker_attempt_v6_contracts`,
`logic_mod_supervision_log_v6_contracts`,
`logic_mod_artifact_dag_v6_cli_workflows`). Der CLI-Beispielgenerator lief
nur in einer vollständigen temporären Kopie mit `/tmp/bruce-build/bruce`:
Exit 0, 167 Beispiele, 93 dokumentierte Optionen, 1029 Diagnosen, Links OK.
Ein zusätzlicher lokaler Markdown-Link-Scan ergab `[]`.

Die vollständige CTest-Abnahme ist getrennt zu protokollieren, weil die
Sandbox lokale AF_INET-Tests verweigern kann; ein solcher Umgebungsfehler ist
kein V6-Erfolgsnachweis.
