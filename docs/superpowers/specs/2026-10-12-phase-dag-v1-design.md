# PhaseDagV1 – aktuelle Referenzspezifikation

## Status, Quellen und Ziel

**IMPLEMENTIERT UND BELEGT** für den isolierten V1-Referenzkern in
`include/bruce/phase_dag/phase_dag.hpp`, `src/phase_dag/phase_dag.cpp` und
`tests/unit/phase_dag_tests.cpp`. Diese Spezifikation extrahiert den
umsetzbaren V1-Kern aus `docs/concepts/affine-phase-dag.md`, der früheren
Phase-DAG-Spezifikation vom 27. September 2026 und dem zugehörigen Plan.

**IMPLEMENTIERT UND BELEGT** ist auch die vollständige
Projekt-CMake-/CTest-Integration: Nach dem lokalen Entpacken der dokumentierten
Debian-Abhängigkeiten in `build-host-deps/root` lief ein frischer Release-Build
aus einer vollständigen temporären Kopie unter `/tmp` durch; CTest meldete
149/149 erfolgreiche Testgruppen. Der fokussierte Test wurde mit der echten
OpenSSL-SHA-256-Implementierung gebaut und ausgeführt.

**IMPLEMENTIERT UND BELEGT:** Der aktuelle Kern besitzt acht feste
`ApplicationTask`-Varianten und eine lineare `ExecutionSession` mit
`CREATED → EXECUTING → SERIALIZING → REPLAYING → READY|FAILED`. Diese V1
erweitert weder diese Session noch die CLI. Sie bleibt ein eigenständiger,
deterministischer Referenzkern.

**Ziel:** Ein kleiner, serieller, vollständig replaybarer Ausführungs-DAG
koordiniert 1 bis 16 versionierte Adapteraufrufe mit expliziten
Datenabhängigkeiten. Er bewahrt die Herkunft jedes Adapteroutputs, ohne dessen
fachliche Aussage zu verstärken.

## Nicht-Ziele

Folgende Themen bleiben außerhalb von V1: Änderungen an `ApplicationTask`,
`ExecutionSession`, CLI, Gates, Experience, Language, NanoCell, dynamischen
Plugins, Netzwerk, Datenbank, Parallelität, GPU, semantischen Zustandsgraphen,
SCC-Verdichtung, Affinitäts-/Relationssuche und allgemeiner Formelbeweise.

Ein späterer NanoCell-Adapter oder eine CLI-Integration benötigt jeweils einen
eigenen Vertrag und eigene Tests. Ein `READY` des Schedulers und ein
`MATCHED`-NanoCell-Ergebnis sind keine Learn-, Publish- oder Beweisfreigaben.

## Architektur

```text
PhaseDagSpecV1
  ↓ vollständige Validierung
kanonischer serieller Scheduler
  ↓
ein Receipt je Node
  ↓
kanonisches BRUCE_PHASE_DAG_RUN_V1-Bundle
  ↓
Replay durch Neu-Ausführung und byteidentischen Vergleich
```

Neue Produktionsdateien liegen ausschließlich in:

- `include/bruce/phase_dag/phase_dag.hpp` – öffentlicher V1-Vertrag;
- `src/phase_dag/phase_dag.cpp` – Validator, feste Adapter, Scheduler und
  Codec;
- `tests/unit/phase_dag_tests.cpp` – Vertrags-, Replay- und Manipulationstests.

`CMakeLists.txt` ergänzt nur diese Quelle zu `bruce_core` und registriert
`phase_dag_contracts`. Es gibt keine neue Option, kein neues Executable und
keine Änderung am CLI-Hilfevertrag.

## Kanonischer Datenvertrag

Alle IDs (`dag_id`, `node_id`, `adapter_id`, `adapter_version`) bestehen aus
1 bis 64 ASCII-Zeichen in `[A-Za-z0-9_.-]`. IDs, Nodes und Dependencies sind
byteweise ASCII-sortiert und eindeutig.

| Grenze | V1-Wert |
|---|---:|
| Nodes | 1..16 |
| Dependencies pro Node | 0..15 |
| Kanten gesamt | höchstens 64 |
| Requestbytes | höchstens 65.536 |
| `max_started_nodes` | 0..16 |
| `max_receipt_bytes` | 1..262.144 |
| `max_trace_entries` | 2..512 |

`PhaseDagSpecV1` enthält Version `phase-dag-v1`, DAG-ID, sortierte Nodes und
die drei Limits. Jeder `PhaseNodeSpecV1` enthält Adapter-ID/-Version,
sortierte Vorgänger-IDs, rohe Requestbytes und deren SHA-256-Digest.

Ein Adapter erhält ausschließlich unveränderliche Requestbytes und kanonisch
sortierte `PhaseInputRefV1`-Objekte. Ein erfolgreicher Aufruf liefert ein
`PhaseOutputRefV1` mit `COMPLETE`, `INCOMPLETE` oder `REJECTED`, Output- und
Evidenzbytes samt SHA-256-Digests sowie einem adapterdefinierten Claim-Scope.
Diese drei Outputzustände bleiben fachliche Adapterzustände.

Der Scheduler erzeugt dagegen genau ein `PhaseReceiptV1` pro Node:

| Receipt-Outcome | Bedeutung |
|---|---|
| `READY` | Adapter lieferte einen Output, auch bei `INCOMPLETE` oder `REJECTED` |
| `FAILED` | Adapter-, Vertrags- oder Evidenzfehler; kein Output |
| `BLOCKED` | Nicht-`READY`-Vorgänger; Adapter wurde nicht aufgerufen |
| `CANCELLED` | Globaler Control- oder Budgetstopp vor Start; kein Output |

Zyklen, Selbstkanten, unbekannte oder doppelte Vorgänger, unkanonische
Reihenfolgen, falsche Requestdigests und Grenzverletzungen sind Fehler vor
jedem Adapteraufruf.

## Ausführung und Abbruch

1. Spec vollständig validieren; bei Fehler entstehen weder Receipt noch Bundle.
2. Den lexikographisch kleinsten entscheidungsfähigen Node wählen.
3. Vor jedem Start zuerst globalen Control und anschließend die Node-, Trace-
   und Receipt-Limits prüfen.
4. Bei globalem Stopp den aktuellen und alle noch unentschiedenen Nodes als
   `CANCELLED` markieren; laufende Adapter werden nie unterbrochen.
5. Ohne globalen Stopp wird ein Node mit nicht-`READY`-Vorgänger `BLOCKED`.
   Unabhängige Nodes laufen weiterhin in kanonischer Reihenfolge.
6. Adapterfehler werden `FAILED`; nur abhängige Nachfolger werden `BLOCKED`.
7. Nach jeder Entscheidung folgt die nächste kanonische Auswahl; nach dem
   letzten Receipt wird `COMPLETED` in den Trace geschrieben.

Der Trace enthält ausschließlich `VALIDATED`, `STARTED`, `READY`, `FAILED`,
`BLOCKED`, `CANCELLED` und `COMPLETED`. V1 bleibt strikt seriell.

## Referenzadapter und Replay

V1 registriert nur zwei private Fixture-Adapter:

- `fixture-source-v1`: produziert aus einem testdefinierten Request einen
  `COMPLETE`-, `INCOMPLETE`- oder `REJECTED`-Output oder wirft einen festen
  Fehler.
- `fixture-join-v1`: übernimmt mindestens einen Input und bildet dessen
  Outputbytes in kanonischer Vorgängerreihenfolge ab.

`BRUCE_PHASE_DAG_RUN_V1` besteht aus Header, äußerem SHA-256-Digest und
length-prefix-kodierten kanonischen Spec-, Receipt- und Trace-Bytes. Replay
prüft Framing, Längen, alle Digests und die kanonische Kodierung, führt die
Spec ohne Control erneut aus, vergleicht Receipts und Trace und verlangt
byteidentisches Repacken. Digests sichern Integrität, nicht menschliche
Autorisierung.

## Abnahmekriterien

Die Tests müssen mindestens belegen:

1. Ablehnung aller nichtkanonischen, fehlerhaften oder zyklischen Specs ohne
   Adapteraufruf.
2. Zwei Quellen und ein Join starten als `a`, `b`, `c`; der Join erhält die
   Inputs in derselben Reihenfolge.
3. `INCOMPLETE` und `REJECTED` bleiben `READY`-Outputs und sind für zulässige
   Nachfolger verfügbar.
4. Ein lokaler Fehler blockiert nur Nachfolger, nicht unabhängige Nodes.
5. Budget- und Controlstopps machen alle unentschiedenen Nodes `CANCELLED`.
6. Manipulation an Header, Längen, Digests, Spec, Receipts, Inputs, Outputs,
   Evidenz, Reihenfolge, Outcome oder Trace wird beim Replay verworfen, auch
   nach Neuberechnung des äußeren Digests.
7. Ein erfolgreicher Replay-Lauf ergibt byteidentische Bundlebytes.

Ein späterer Messnachweis berichtet Spec-, Request-, Input-Ref-, Output-Ref-,
Receipt-, Trace- und Gesamtbundlegrößen getrennt. Er darf keine pauschale
RAM-, Laufzeit- oder Skalierungsbehauptung ableiten.

## Offene Folgeentscheidungen

- Welche fachlichen Adapter nach den Fixture-Adaptern statisch zugelassen
  werden;
- wie NanoCell-Status und -Evidenz in einen späteren Adaptervertrag passen;
- ob Application-Bundles später konsumiert werden dürfen;
- wie semantische Zyklen, SCCs, Parallelität und globale Ressourcenlimits
  unabhängig versioniert werden.

## Quellen

- `docs/concepts/affine-phase-dag.md`
- `docs/superpowers/specs/2026-09-27-phase-dag-v1-design.md`
- `docs/superpowers/plans/2026-09-27-phase-dag-v1.md`
- `docs/adr/012-checked-execution-phases.md`
- `docs/architecture/03-blueprint-consolidation.md`
