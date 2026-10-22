# Runtime Orchestrator V1 – Task-6-Abnahme

**Status:** **IMPLEMENTIERT UND BELEGT**, mit der ausdrücklich abgegrenzten
Netzwerk-Einschränkung unten. Dieser Bericht beschreibt die am 2026-10-17
aus einer frischen CMake-Konfiguration ausgeführte V1-Abnahme, nicht eine
allgemeine System- oder Kapazitätszusage.

## Abgenommener Umfang

`bruce runtime-orchestrator-v1 serve` ist ein einzelner lokaler foreground
JSONL-Host. Er verlangt fünf positive Hostlimits, führt vorbereitete V6-Pläne
in FIFO-Reihenfolge aus und behandelt die Summen der deklarieren V6-Werte als
Admission-Accounting. Sie sind keine harte globale CPU-, RAM-, Prozess-,
I/O-, Netzwerk- oder cgroup-Grenze.

Ein Queue-Cancel erreicht keine V6-Ausführung. Ein Active-Cancel setzt nur die
private V6-Host-Latch; V6 drainiert/reapt selbst, und erst anschließend gibt
der Controller die Reservation frei und veröffentlicht `CANCELLED`. Die
Statusprojektion enthält nur opaque IDs, Sequenzen, Zustand, Grund,
Reservation und begrenzte V6-Abschlusswerte. Sie enthält keine Pfade, URLs,
PID/PGID, Credentials, Payloads, Frames, Signaturen oder Rohtelemetrie. Die
Pfad- und optionalen Remote-URL-Felder gehören ausschließlich zur
kontrollierten `submit`-Eingabe und werden nie in Antworten gespiegelt.

EOF stoppt die Annahme und drainiert akzeptierte Runs. Ein Hostende ist kein
Recovery-Protokoll: V1 rekonstruiert weder Queue noch Status, repariert keinen
Record/kein Sidecar und rollt keine Artefakte zurück. Sidecars bleiben
diagnostisch und sind keine zweite Statusautorität. V1 bietet keine Persistenz,
Recovery, Socket-, HTTP-, RPC-, WebSocket-, Cockpit- oder Mehrbenutzer-API;
es gibt keine Prioritäten, Backfill, Aging, Preemption oder automatische
Retries.

## Neue Task-6-Nachweise

Der Workflowtest `runtime_orchestrator_v1_cli_workflows` belegt zusätzlich zu
Framing, Parser-vor-Zugriff, FIFO, Queuefull, Active/Queue-Cancel und
Redaktion:

1. Nach beendetem V6-Cancel wird das bestehende Sidecar durch einen Symlink
   auf ungültige Daten ersetzt. Zwei anschließende `status`-Anfragen liefern
   weiterhin den bereits feststehenden Controllerabschluss; der Host folgt,
   repariert oder ersetzt den Sidecar nicht und startet keinen Worker.
2. Ein foreground Host wird nach nachgewiesenem V6-`ATTEMPT_STARTED` mit
   `SIGTERM` beendet. Der Test behauptet nur den bestehenden V6-Host/Child-
   Grenzvertrag und keine übergreifende Bereinigung oder Recovery. Ein neuer
   V1-Host kennt die vorherige opaque `run_id` nicht und importiert weder
   Queue noch Sidecar/Record.

## Reproduzierbare Befehle und Ergebnis

```sh
cmake -G Ninja -S . -B /tmp/bruce-orchestrator-final \
  -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/bruce-orchestrator-final
ctest --test-dir /tmp/bruce-orchestrator-final \
  -R 'runtime_orchestrator_v1_(contracts|cli_workflows)|logic_mod_artifact_dag_v[2-6]_(contracts|cli_workflows)' \
  --output-on-failure
python3 scripts/check_local_doc_links.py
```

Die vollständige Konfiguration und der vollständige Build endeten mit Exit 0.
Die Abschlussmatrix lieferte acht erfolgreiche Gruppen:
`logic_mod_artifact_dag_v2_contracts`, `v3_contracts`, `v4_contracts`,
`v5_contracts`, `v6_contracts`, `runtime_orchestrator_v1_contracts`,
`logic_mod_artifact_dag_v6_cli_workflows` und
`runtime_orchestrator_v1_cli_workflows`. Der lokale Linkcheck nach der durch
den Verifier erzeugten CLI-Dokumentation meldete
`documents=494 links=3041 broken=0`.

`logic_mod_artifact_dag_v5_cli_workflows` konnte in dieser Sandbox keine
TLS-Listener-Socket erzeugen (`AF_INET`, `EPERM`). Das ist eine
Umgebungseinschränkung und kein erfolgreicher TLS-Nachweis; sie betrifft den
bestehenden V5-HTTPS-Workflow, nicht die lokale V1-JSONL-Steuerung. Die
V5-Vertragsgruppe selbst bestand.

Vor dem vollständigen Build schlug die zielselektive Matrix erwartbar fehl,
weil ihre Exporter-Fixture nicht gebaut war. Das war kein Produktfehler; der
nachfolgende vollständige Build stellte alle CTest-Fixtures bereit.

## Dokumentations- und Kompatibilitätsbelege

- [Befehlsvertrag](../../../cli/commands.md#runtime-orchestrator-v1--lokale-fifo-steuerung-für-v6-läufe)
- [JSONL-Format](../../../cli/formats.md#runtime-orchestrator-v1-foreground-jsonl-ohne-neues-laufformat)
- [Architekturgrenze](../../../architecture/03-blueprint-consolidation.md#runtime-orchestrator-v1)
- [V1-Spezifikation](../../specs/2026-10-17-runtime-orchestrator-v1-design.md)
- [Controller-Test](../../../../tests/unit/runtime_orchestrator_v1_tests.cpp)
- [CLI-/Hostende-Test](../../../../tests/integration/test_runtime_orchestrator_v1_cli.py)

V2–V6 bleiben in der Matrix enthalten, damit der neue private V6-Latch und
der Orchestrator keine Record-/Replay-Kompatibilität behaupten, ohne die
bestehenden Verträge erneut auszuführen. V1 serialisiert selbst nichts in
Spec, DAG-ID, Sidecar oder Record; Replay lädt den Orchestrator nicht und
startet keine Worker.
