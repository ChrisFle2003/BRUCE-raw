# DAG-Scheduler V1 – Design

**Status: VORSCHLAG — zur Prüfung vor Implementierungsplanung.**

## 1. Ziel und Grenzen

DAG-Scheduler V1 ist ein kleiner, adapterneutraler C++20-Kern für die
seriell-deterministische Ausführung einer begrenzten gerichteten azyklischen
Graph-Spezifikation. Er validiert einen Graphen und einen festen Callback-
Katalog, wählt immer den lexikographisch kleinsten bereiten Knoten, führt
seinen in-memory Callback aus und veröffentlicht kanonische Receipts sowie
einen logischen Trace.

Der Kern ist von Phase-DAG V2/V3 unabhängig. Er verwendet keine NanoCell-,
Payload-, Bundle-, Replay-, Cockpit-, ApplicationTask- oder Signed-Affine-
Typen und ändert keine bestehenden Verträge. Er ist nicht parallel und
startet keine Threads. Er enthält keine freie Callback-Registrierung im CLI,
keine Dateien, Persistenz, Netzwerk-, GPU- oder Plugin-Ausführung und keine
universelle Runtime-Orchestrierung.

Ein fester CLI-Adapter bietet ausschließlich drei in-memory Referenzen:

```text
bruce dag-scheduler-v1 demo mixed
bruce dag-scheduler-v1 demo budget
bruce dag-scheduler-v1 demo cancelled
```

Freie DAG-Dateien, Callback-Namen, Budgets oder andere Parameter sind keine
CLI-Schnittstelle.

## 2. Öffentlicher C++20-Vertrag

Neue Dateien:

```text
include/bruce/dag_scheduler/dag_scheduler_v1.hpp
src/dag_scheduler/dag_scheduler_v1.cpp
```

Der Header definiert exakt:

```cpp
struct DagSchedulerNodeSpecV1 {
    std::string node_id;
    std::string callback_id;
    std::vector<std::string> dependencies;
};

struct DagSchedulerSpecV1 {
    std::string scheduler_version{"dag-scheduler-v1"};
    std::string dag_id;
    std::vector<DagSchedulerNodeSpecV1> nodes;
    std::uint64_t max_started_nodes{};
};

struct DagSchedulerInputV1 {
    std::string producer_node_id;
    std::string result_code;
};

enum class DagSchedulerCallbackStatusV1 { ready, failed };
struct DagSchedulerCallbackResultV1 {
    DagSchedulerCallbackStatusV1 status{};
    std::string result_code;
};

using DagSchedulerCallbackFnV1 = DagSchedulerCallbackResultV1 (*) (
    std::span<const DagSchedulerInputV1>);
struct DagSchedulerCallbackEntryV1 {
    std::string_view callback_id;
    DagSchedulerCallbackFnV1 execute{};
};
using DagSchedulerCallbackRegistryV1 = std::span<const DagSchedulerCallbackEntryV1>;

enum class DagSchedulerReceiptOutcomeV1 { ready, failed, blocked, cancelled };
struct DagSchedulerReceiptV1 {
    std::string node_id;
    std::string callback_id;
    std::vector<DagSchedulerInputV1> input_refs;
    DagSchedulerReceiptOutcomeV1 outcome{};
    std::string terminal_code;
};

enum class DagSchedulerTraceEventV1 {
    validated, started, ready, failed, blocked, cancelled, completed
};
struct DagSchedulerTraceEntryV1 {
    DagSchedulerTraceEventV1 event;
    std::string node_id;
};

struct DagSchedulerControlV1 {
    std::function<bool()> cancelled;
};
struct DagSchedulerRunV1 {
    DagSchedulerSpecV1 spec;
    std::vector<DagSchedulerReceiptV1> receipts;
    std::vector<DagSchedulerTraceEntryV1> trace;
};

void validate_dag_scheduler_spec_v1(
    const DagSchedulerSpecV1&, DagSchedulerCallbackRegistryV1);
DagSchedulerRunV1 execute_dag_scheduler_v1(
    const DagSchedulerSpecV1&, DagSchedulerCallbackRegistryV1,
    const DagSchedulerControlV1& = {});
```

Knoten-, Callback-, DAG- und Ergebniscode-IDs folgen derselben begrenzten
ASCII-Identifierregel: nicht leer, höchstens 64 Zeichen und nur
`A-Z`, `a-z`, `0-9`, `-` und `_`; Ergebniscodes dürfen zusätzlich keine
Leerzeichen enthalten. Der Katalog ist nach `callback_id` eindeutig und
lexikographisch sortiert. Eine Spec enthält 1 bis 64 Knoten, höchstens 15
verschiedene Abhängigkeiten pro Knoten und ein Startbudget von 1 bis 64.

## 3. Deterministische Laufzeitsemantik

1. Vor jedem Lauf validiert der Kern Version, IDs, Katalog, Knoten,
   Abhängigkeiten, Selbstkanten, doppelte Abhängigkeiten, bekannte Callbacks,
   Budget und Azyklizität. Bei einer ungültigen Spec wird kein Run erzeugt.
2. Der Kern schreibt einen einzelnen `VALIDATED`-Traceeintrag und führt danach
   Kahn-Auswahl aus. Die Ready-Menge ist nach `node_id` sortiert; pro Schleife
   startet genau der kleinste Knoten.
3. Ein gestarteter Knoten erhält ausschließlich die `READY`-Receipts seiner
   direkten Vorgänger, nach `producer_node_id` sortiert. Der Callback wird
   synchron aufgerufen. Sein Ergebniscode ist obligatorisch und wird gegen
   die Identifierregel geprüft.
4. Ein Callback-Ergebnis `ready` erzeugt ein `READY`-Receipt und -Traceevent;
   `failed` ein `FAILED`-Receipt und -Traceevent. Bereits gestartete
   Callbacks werden nie erneut aufgerufen.
5. Sobald ein noch nicht gestarteter Knoten einen terminalen Vorgänger hat,
   der nicht `READY` ist, wird er `BLOCKED` mit
   `DEPENDENCY_NOT_READY`. Diese Konsolidierung geschieht vor Budget- oder
   Abbruchbehandlung und kann sich transitiv wiederholen.
6. Vor jedem weiteren Start fragt der Kern `control.cancelled`, falls es
   gesetzt ist. Bei `true` werden alle noch nicht terminalen Knoten
   `CANCELLED` mit `CONTROLLED_CANCELLATION`. Bereits erzeugte Receipts und
   Traceeinträge bleiben erhalten.
7. Wenn die Zahl gestarteter Knoten das Budget erreicht, werden alle noch
   nicht terminalen Knoten `CANCELLED` mit `START_BUDGET_EXHAUSTED`.
8. Am Ende sind alle Knoten terminal. `receipts` sind nach `node_id` sortiert;
   `trace` behält die zeitliche logische Reihenfolge und endet mit
   `COMPLETED` ohne `node_id`.

Der Scheduler erhält keine Ausnahme aus einem Callback als normales Ergebnis:
wirft ein Callback, propagiert die Ausnahme und veröffentlicht keinen
partiellen `DagSchedulerRunV1`.

## 4. Fehlerverhalten

| Bedingung | Fehler |
|---|---|
| ungültige Version, DAG-ID, Knoten-ID, Callback-ID oder Ergebniscode | `InvalidDagSchedulerIdentifier` bzw. `InvalidDagSchedulerCallbackResult` |
| leerer/zu großer Graph oder ungültiges Budget | `InvalidDagSchedulerSpec` bzw. `InvalidDagSchedulerStartBudget` |
| unsortierter, doppelter, leerer oder funktionsloser Katalog | `InvalidDagSchedulerCallbackRegistry` |
| doppelte Knoten-ID oder unbekannter Callback/Vorgänger | `DuplicateDagSchedulerNode`, `UnknownDagSchedulerCallback`, `UnknownDagSchedulerDependency` |
| Selbstkante oder doppelte Abhängigkeit | `InvalidDagSchedulerDependency` |
| Zyklus | `DagSchedulerCycle` |

Die genauen Fehlermeldungen enthalten die betroffene ID, sofern eine gültige
ID vorhanden ist. Falsche Spezifikationen und ungültige Callback-Ergebnisse
kehren ohne teilweisen Run zurück. Terminale Laufzeitfehler hingegen sind
kontrollierte Receipts und kein Exceptionsignal.

## 5. Feste CLI-Szenarien

Neue Dateien:

```text
apps/dag_scheduler_v1_cli.hpp
tests/integration/test_dag_scheduler_v1_cli.py
```

Die Route ist unconditional in `apps/bruce_cli.cpp` registriert und auch ohne
NanoCell/Cockpit verfügbar. Alle Erfolgsausgaben sind stabil formatiertes
stdout-JSON mit finalem Zeilenumbruch, leerem stderr und Vertrag
`"dag-scheduler-v1-cli-v1"`; falsche Argumente schreiben ausschließlich
`{"error": "Usage: bruce dag-scheduler-v1 demo mixed|budget|cancelled"}`
nach stderr und enden mit Exitcode `2`.

### `mixed`

Die feste Spec besitzt fünf Knoten:

```text
a-root          root, Callback `root-ready-v1`       → READY ROOT_READY
b-fail          abhängig von a-root, `fail-v1`      → FAILED EXPECTED_FAILURE
c-independent   root, `independent-ready-v1`        → READY INDEPENDENT_READY
d-blocked       abhängig von b-fail, niemals gestartet → BLOCKED DEPENDENCY_NOT_READY
e-finish        abhängig von c-independent, `finish-v1` → READY FINISHED
```

Das Budget ist 5. Der Trace belegt `a-root`, dann `b-fail` vor dem bereits
bereiten `c-independent`, die Blockierung von `d-blocked`, anschließend
`c-independent` und `e-finish`. Der Bericht enthält für `e-finish` das
Vorgängerresultat `c-independent/INDEPENDENT_READY`.

### `budget`

Ein fester Drei-Knoten-DAG hat Budget 1. Nach `a-root` werden die zwei nicht
terminalen Knoten als `CANCELLED/START_BUDGET_EXHAUSTED` veröffentlicht; ihre
Callbacks werden nicht aufgerufen.

### `cancelled`

Ein fester Drei-Knoten-DAG setzt nach seinem ersten erfolgreichen Callback
eine lokale Cancellation-Flagge. Vor dem nächsten Start liest der Control-
Callback diese Flagge; die zwei übrigen Knoten werden
`CANCELLED/CONTROLLED_CANCELLATION`. Kein zweiter Callback wird aufgerufen.

## 6. Tests und Dokumentation

Neue Unit-Tests prüfen den vollständigen Vertrag: positive Reihenfolge und
Inputs, `mixed`, Budget, Abbruch, Wiederholung, Callback-Ausnahme,
Katalog- und Spec-Validierung, Callback-Fehlercodes und Nichtausführung von
BLOCKED/CANCELLED-Knoten. Die Tests prüfen tatsächliche Callback-Aufrufzähler,
keine Mock-Interaktion allein. Insbesondere belegt ein tiefer, aber begrenzter
64-Knoten-DAG die iterative/loop-basierte Ausführung ohne implizite
Parallelität.

Ein echter CLI-Workflow prüft die vollständigen drei JSON-Berichte,
Fehlerpfade, Leerheit von stderr/stdout, Byte-Stabilität und Hilfezeilen. Er
läuft sowohl im Standardbuild als auch mit NanoCell und Cockpit deaktiviert.

README, Blueprint und CLI-Handbuch markieren ausschließlich die obige
serielle, lokale Referenz als **IMPLEMENTIERT UND BELEGT**. Allgemeine
Parallelität, Phase-DAG-V2/V3-Integration, Bundle/Replay, externe Adapter,
Payloadspeicher, Netzwerk/GPU und allgemeine Runtime-Orchestrierung bleiben
**DOKUMENTIERT / KONZEPT**. Die geschützten CLI-Artefakte werden nur über
`docs/cli/verify_examples.py` in einer vollständigen nativen temporären Kopie
behandelt. Wenn der Generator die Demos nicht ausführt, wird diese Grenze
explizit dokumentiert und der echte Workflow-Test bleibt der Laufzeitbeleg.

## 7. Integration

`bruce_core` erhält die neue Quelle, den Unit-Test und keine zusätzlichen
Abhängigkeiten. Die CLI registriert nur den festen Adapter. Neue CMake-Ziele
folgen dem bestehenden Header-only-Adapter-, Core- und Python-Workflowmuster.
Keine bestehende V2/V3-Quelle, kein Bundleformat, keine Cockpit-Routen und
keine bestehenden Signed-Affine-/Toy-Relation-Dateien werden geändert.
