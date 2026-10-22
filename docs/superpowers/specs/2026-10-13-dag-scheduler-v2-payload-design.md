# DAG-Scheduler V2 – begrenzter direkter Payload-Fluss

**Status: VORSCHLAG — freigegebener Entwurf, vor Implementierungsplanung zu prüfen.**

## 1. Ziel und Grenzen

DAG-Scheduler V2 ergänzt den bestehenden, seriell-deterministischen
`dag_scheduler_v1` um begrenzte Byte-Payloads zwischen direkten erfolgreichen
Vorgängern und ihren Nachfolgern. Ein Knoten besitzt Request-Bytes samt
SHA-256-Digest; ein erfolgreiches Callback erzeugt Ergebniscode, Payload und
Payload-Digest. Der Scheduler übergibt einem Callback ausschließlich die
vollständigen, verifizierten Payloads seiner direkten `READY`-Vorgänger,
sortiert nach Produzenten-ID.

V2 ist ein eigener adapterneutraler C++20-Vertrag. `dag_scheduler_v1`,
Phase-DAG V2/V3, NanoCell, Cockpit, Bundle- und Replay-Formate bleiben
unverändert. Der Kern ist lokal, synchron und seriell: Er startet keine
Threads und führt genau einen Callback zur Zeit aus. V2 enthält keine
Dateieingabe, Persistenz, Artefaktreferenzen, Netzwerk-, GPU-, Plugin- oder
allgemeine Runtime-Ausführung.

Byte-Payloads sind keine geheimen Speicherbereiche: Das Modell begrenzt nur
Größe und Integrität. Es bietet keine Verschlüsselung, Zugriffskontrolle oder
Persistenz. Diese Eigenschaften sind außerhalb des V2-Vertrags.

Der einzige öffentliche CLI-Adapter ist eine feste Referenz:

```text
bruce dag-scheduler-v2 demo flow
```

Freie DAG-Dateien, Callback-Namen, Budgets, Payloads oder Parameter sind
keine CLI-Schnittstelle.

## 2. Öffentlicher C++20-Vertrag

Neue Dateien:

```text
include/bruce/dag_scheduler/dag_scheduler_v2.hpp
src/dag_scheduler/dag_scheduler_v2.cpp
```

Der Header definiert exakt:

```cpp
struct DagSchedulerNodeSpecV2 {
    std::string node_id;
    std::string callback_id;
    std::vector<std::string> dependencies;
    std::string request_bytes;
    std::string request_digest;
};

struct DagSchedulerSpecV2 {
    std::string scheduler_version{"dag-scheduler-v2"};
    std::string dag_id;
    std::vector<DagSchedulerNodeSpecV2> nodes;
    std::uint64_t max_started_nodes{};
    std::uint64_t max_total_receipt_bytes{};
};

struct DagSchedulerInputV2 {
    std::string producer_node_id;
    std::string result_code;
    std::string payload_bytes;
    std::string payload_digest;
};

enum class DagSchedulerCallbackStatusV2 { ready, failed };
struct DagSchedulerCallbackResultV2 {
    DagSchedulerCallbackStatusV2 status{};
    std::string result_code;
    std::string payload_bytes;
    std::string payload_digest;
};

using DagSchedulerCallbackFnV2 = DagSchedulerCallbackResultV2 (*) (
    std::string_view request_bytes, std::span<const DagSchedulerInputV2>);
struct DagSchedulerCallbackEntryV2 {
    std::string_view callback_id;
    DagSchedulerCallbackFnV2 execute{};
};
using DagSchedulerCallbackRegistryV2 = std::span<const DagSchedulerCallbackEntryV2>;

struct DagSchedulerOutputRefV2 {
    std::string output_version{"dag-scheduler-output-v2"};
    std::string result_code;
    std::string payload_bytes;
    std::string payload_digest;
};

enum class DagSchedulerReceiptOutcomeV2 { ready, failed, blocked, cancelled };
struct DagSchedulerReceiptV2 {
    std::string node_id;
    std::string callback_id;
    std::string request_digest;
    std::vector<DagSchedulerInputV2> input_refs;
    DagSchedulerReceiptOutcomeV2 outcome{};
    std::optional<DagSchedulerOutputRefV2> output;
    std::string failure_code;
};

enum class DagSchedulerTraceEventV2 {
    validated, started, ready, failed, blocked, cancelled, completed
};
struct DagSchedulerTraceEntryV2 {
    DagSchedulerTraceEventV2 event;
    std::string node_id;
};

struct DagSchedulerControlV2 {
    std::function<bool()> cancelled;
};
struct DagSchedulerRunV2 {
    DagSchedulerSpecV2 spec;
    std::vector<DagSchedulerReceiptV2> receipts;
    std::vector<DagSchedulerTraceEntryV2> trace;
};

void validate_dag_scheduler_spec_v2(
    const DagSchedulerSpecV2&, DagSchedulerCallbackRegistryV2);
DagSchedulerRunV2 execute_dag_scheduler_v2(
    const DagSchedulerSpecV2&, DagSchedulerCallbackRegistryV2,
    const DagSchedulerControlV2& = {});
```

Der V2-Header inkludiert `<cstdint>`, `<functional>`, `<optional>`, `<span>`,
`<string>`, `<string_view>` und `<vector>`. Er exponiert keine Phase-DAG- oder
V1-Typen.

Knoten-, Callback-, DAG- und Ergebniscodes verwenden die V1-Identifierregel:
nicht leer, höchstens 64 Zeichen und ausschließlich `A-Z`, `a-z`, `0-9`, `-`
und `_`. Der Callback-Katalog ist nicht leer sowie nach `callback_id` strikt
aufsteigend und eindeutig sortiert. Eine Spec enthält 1 bis 64 Knoten, jeder
Knoten höchstens 15 verschiedene Abhängigkeiten, und
`max_started_nodes` liegt zwischen 1 und 64.

`request_bytes` und jeder erfolgreiche `payload_bytes`-Wert enthalten
höchstens 65.536 Bytes. Ihr Digest ist exakt `sha256(bytes)` des vorhandenen
Kern-Hashvertrags. `max_total_receipt_bytes` liegt zwischen 1 und 4.194.304
Bytes. Dieser Wert begrenzt ausschließlich die insgesamt in Receipts
retinierten Byte-Payloads, inklusive jeder Kopie in `input_refs`; Requests,
Digests, IDs, Ergebnis- und Fehlercodes zählen nicht zu diesem Budget.

## 3. Deterministische Laufzeitsemantik

1. Vor jedem Lauf validiert der Kern Version, IDs, Katalog, Knoten,
   Request-Größe und -Digest, Abhängigkeiten, bekannte Callbacks, Budgets und
   Azyklizität. Eine ungültige Spec erzeugt keinen Run.
2. Der Kern erzeugt den einzelnen Traceeintrag `VALIDATED`. Die Ready-Menge
   ist nach `node_id` sortiert; er startet pro Schleife genau den
   lexikographisch kleinsten bereiten Knoten.
3. Ein gestarteter Knoten erhält seinen unveränderten `request_bytes`-Wert
   und ausschließlich `READY`-Receipts seiner direkten Vorgänger. Für jedes
   Input-Element kopiert der Kern Produzenten-ID, Ergebniscode, Payload und
   Digest aus dessen `output`; die Elemente sind nach `producer_node_id`
   sortiert. V2 übergibt weder indirekte Vorgänger noch Daten nicht-erfolgreicher
   Knoten.
4. Ein Callback muss `ready` oder `failed` zurückgeben und einen gültigen
   `result_code` tragen. Bei `ready` müssen die Payloadgröße sowie
   `payload_digest == sha256(payload_bytes)` gelten. Der Kern erstellt dann
   ein `READY`-Receipt mit `output_version == "dag-scheduler-output-v2"`,
   Ergebniscode, Payload und Digest und schreibt `READY` in den Trace.
5. Bei `failed` müssen `payload_bytes` und `payload_digest` leer sein. Der
   resultierende `FAILED`-Receipt hat kein `output`, keine `input_refs` und
   den Callback-Ergebniscode als `failure_code`; der Trace erhält `FAILED`.
   `BLOCKED` und `CANCELLED` haben ebenfalls kein `output` und keine
   `input_refs`.
6. Sobald ein nicht gestarteter Knoten einen terminalen Vorgänger besitzt,
   der nicht `READY` ist, wird er `BLOCKED` mit
   `DEPENDENCY_NOT_READY`. Diese Konsolidierung geschieht transitiv vor jeder
   Abbruch- oder Budgetprüfung.
7. Vor jedem weiteren Start konsultiert der Kern `control.cancelled`, wenn
   gesetzt. Bei `true` werden alle noch nicht terminalen Knoten
   `CANCELLED/CONTROLLED_CANCELLATION`. Wenn das Startbudget ausgeschöpft
   ist, erhalten sie `CANCELLED/START_BUDGET_EXHAUSTED`. Bereits gestartete
   Callbacks werden nicht erneut aufgerufen.
8. Der Scheduler berechnet für jeden erfolgreichen Receipt die Menge seiner
   gespeicherten Bytes als `output.payload_bytes.size()` plus die Größen aller
   `input_refs[*].payload_bytes`. Die Summe über alle erfolgreichen Receipts
   darf `max_total_receipt_bytes` nicht übersteigen. Würde das Publizieren
   eines Receipts die Grenze übersteigen, wirft der Kern
   `DagSchedulerV2ReceiptBudgetExceeded` und gibt keinen partiellen
   `DagSchedulerRunV2` zurück.
9. Am Ende sind alle Knoten terminal. `receipts` sind nach `node_id` sortiert;
   der Trace bleibt in logischer Zeitreihenfolge und endet mit `COMPLETED`
   ohne `node_id`.

Ein Callback, der eine Ausnahme wirft, wird nicht in ein Receipt übersetzt:
die Ausnahme propagiert und es wird kein partieller Run veröffentlicht.
Ungültige Callback-Rückgaben ebenso wie ein Budgetüberlauf führen zu keinem
partiellen Run. Der Scheduler kann aus einem einzelnen Callback-Aufruf keine
inhaltliche Nichtdeterministik erkennen, ohne den Callback erneut auszuführen
und damit dessen Seiteneffekte zu duplizieren. Daher lehnt V2 ungültige
Rückgaben ab und ist bei gleichen Spec-, Control- und Callback-Rückgaben
deterministisch; die Reinheit eines Callbacks bleibt Verantwortung seines
Einbinders.

## 4. Fehlerverhalten

| Bedingung | Fehler |
|---|---|
| ungültige Version, IDs, Ergebniscode, Request-Größe oder Request-Digest | `InvalidDagSchedulerV2Spec` |
| leerer/zu großer Graph oder ungültiges Start-/Receipt-Budget | `InvalidDagSchedulerV2Spec` |
| unsortierter, doppelter, leerer oder funktionsloser Katalog | `InvalidDagSchedulerV2CallbackRegistry` |
| doppelte Knoten-ID oder unbekannter Callback/Vorgänger | `DuplicateDagSchedulerV2Node`, `UnknownDagSchedulerV2Callback`, `UnknownDagSchedulerV2Dependency` |
| Selbstkante oder doppelte Abhängigkeit | `InvalidDagSchedulerV2Dependency` |
| Zyklus | `DagSchedulerV2Cycle` |
| ungültiger Callback-Status/-code, Payload bei `failed`, zu große Payload oder falscher Payload-Digest | `InvalidDagSchedulerV2CallbackResult` |
| gespeicherte Payloadkopien überschreiten das Receipt-Budget | `DagSchedulerV2ReceiptBudgetExceeded` |
| unterschiedliche, aber jeweils gültige Callback-Rückgaben in getrennten Läufen | Nicht automatisch aus dem Repository-Lauf ableitbar; keine erneute Ausführung zur Erkennung |

Die Fehlermeldung enthält die betroffene gültige ID, sofern vorhanden. Die
genannten Fehlersignale sind Fehler des aufrufenden Programms oder Callbacks;
`FAILED`, `BLOCKED` und `CANCELLED` dagegen sind kontrollierte terminale
Laufzeitergebnisse.

## 5. Feste CLI-Referenz

Neue Dateien:

```text
apps/dag_scheduler_v2_cli.hpp
tests/integration/test_dag_scheduler_v2_cli.py
```

Die Route ist unbedingt in `apps/bruce_cli.cpp` registriert und steht auch
ohne NanoCell und Cockpit zur Verfügung. Ein erfolgreicher Lauf schreibt
stabil formatiertes stdout-JSON mit finalem Zeilenumbruch, leerem stderr und
Vertrag `"dag-scheduler-v2-cli-v1"`. Das JSON zeigt Request- und
Payload-Digests sowie Bytegrößen, aber niemals rohe Request- oder
Payload-Bytes. Falsche Argumente schreiben ausschließlich

```json
{"error":"Usage: bruce dag-scheduler-v2 demo flow"}
```

nach stderr und beenden sich mit Exitcode 2.

`flow` verwendet genau diese lokale Vier-Knoten-Spec mit einem Startbudget
von 4 und einem Receipt-Budget von 4.096 Bytes:

```text
a-source       SOURCE_READY       Payload "alpha"
b-transform    TRANSFORM_READY    abhängig von a-source, Payload "alpha:beta"
c-independent  INDEPENDENT_READY  Payload "gamma"
d-join         JOIN_READY         abhängig von b-transform und c-independent,
                                  Payload "alpha:beta|gamma"
```

Alle vier Knoten enthalten feste nichtleere Request-Bytes mit korrektem
Digest. `b-transform` prüft den einzigen Input von `a-source`; `d-join`
prüft genau die Inputs `b-transform` und danach `c-independent`. Die
lexikographische Scheduler-Reihenfolge startet `a-source`, dann
`b-transform`, danach `c-independent` und zuletzt `d-join`. Der Bericht
liefert kanonisch nach Knotennamen sortierte Receipts und einen
chronologischen Trace. Der Adapter besitzt keine alternative Demo und keine
Abkürzung, die die Kernvalidierung umgeht.

## 6. Tests und Dokumentation

Die Unit-Tests prüfen mit realen Callback-Funktionen und Aufrufzählern:

- den vollständigen `flow` mit tatsächlichen Byteinhalten, korrekten
  SHA-256-Digests und direkter, sortierter Inputweitergabe;
- manipulierte Request- und Payload-Digests, Payloads über 65.536 Bytes und
  `failed`-Ergebnisse mit Payload;
- genaue Zählung des Receipt-Budgets über Output-Payloads und kopierte
  Input-Payloads sowie keinen veröffentlichten partiellen Run beim Überlauf;
- Fehler, Blockierung, Cancellation und Startbudget samt Nichtausführung
  nicht gestarteter Callbacks;
- Wiederholungsgleichheit einer vollständigen Ergebnisprojektion sowie
  unveränderte Ausführung der bestehenden V1-Vertragsprüfungen.

Der Python-Workflowtest startet den echten Binary-Prozess. Er prüft das
vollständige `flow`-JSON, alle angezeigten Größen und Digests ohne rohe
Payloads, leeres stderr, finalen Zeilenumbruch, falsche Argumente und
Byte-für-Byte-Stabilität mehrfacher Läufe. Er läuft im Standardbuild und in
einem Build mit NanoCell und Cockpit deaktiviert; beide JSON-Berichte müssen
identisch sein.

README, Blueprint und CLI-Handbuch nennen ausschließlich diesen lokalen,
seriellen, begrenzten Datenfluss als **IMPLEMENTIERT UND BELEGT**, sobald die
geplanten Tests grün vorliegen. Bundle/Persistenz/Replay, externe
Artefaktspeicher, Phase-DAG-V3-Adapter, Parallelität, GPU- oder
Netzwerkausführung bleiben **DOKUMENTIERT / KONZEPT**. Die geschützten
CLI-Artefakte werden ausschließlich mittels `docs/cli/verify_examples.py`
in einer vollständigen nativen temporären Kopie erzeugt oder geprüft. Führt
der Generator keine V2-Demo aus, wird diese Abdeckungslücke ausdrücklich
dokumentiert; der echte Workflowtest bleibt der Laufzeitbeleg.

## 7. Integration

`bruce_core` erhält ausschließlich die neue V2-Quelle, einen V2-Unit-Test und
keine neue Abhängigkeit. Der CLI-Adapter referenziert nur den V2-Kern und
registriert genau die feste `flow`-Demo. CMake folgt dem bestehenden
Core-/Header-only-Adapter-/Python-Workflowmuster.

Nicht im Umfang: Änderungen an V1, Phase-DAG V2/V3, Bundle- oder
Replayformaten, Cockpit-Routen, NanoCell, Signed-Affine, Toy-Relation oder
bestehenden CLI-Verträgen.
