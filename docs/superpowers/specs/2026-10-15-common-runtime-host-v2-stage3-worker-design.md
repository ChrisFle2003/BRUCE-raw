# Common Runtime Host V2 / Stage 3 — Bruce als Host-Orchestrator

**Status: IMPLEMENTIERT UND BELEGT — Architektur- und Implementierungsvertrag
für die abgeschlossene Stage-3A-Umsetzung.**

## Ziel

Stage 3 bündelt die bereits vorhandene lokale Worker-Grenze in einem
gemeinsamen Host-V2-Pfad. Bruce bleibt der alleinige Orchestrator: Er legt den
privaten Laufbereich an, validiert den Request, startet genau einen lokalen
Worker-Prozess (mit dem internen Watchdog als Hilfsprozess), überwacht seine
Budgets, nimmt nur ein Digest-/Referenz-Ergebnis entgegen und beendet den
Worker kontrolliert, wenn der Lauf abgebrochen oder ein Budget überschritten
wird.

Die erste Ausbaustufe ist absichtlich ein vertikaler Ein-Worker-Schnitt. Sie
führt weder eine neue DAG-Engine noch ein neues Payload-Format ein. Der
bestehende V5-CLI-Pfad bleibt der erste öffentliche Benutzerpfad; V2, V3 und
V4 werden nicht verändert.

## Abgrenzung und Belegklassen

**IMPLEMENTIERT UND BELEGT:** Die technische Grundlage ist im Repository
vorhanden:

- `include/bruce/worker/worker_boundary_v1.hpp` definiert Frames, Request,
  Result, Artefaktreferenzen, fünf Budgets, Usage und Fehlercodes.
- `src/worker/worker_supervisor_v1.cpp` startet einen neuen Worker-Child pro
  Versuch und zusätzlich einen internen Watchdog-Hilfsprozess, nutzt Pipes und
  eine eigene Prozessgruppe, setzt im Worker `RLIMIT_AS`, überwacht eine
  monotone Wall-Time-Frist und reaped den Worker mit `wait4`.
- `src/worker/worker_supervisor_v1.cpp` misst den Speicher-Peak als Maximum aus
  `/proc/<pid>/status` (`VmHWM`/`VmRSS`) und `ru_maxrss` aus `wait4`, prüft
  Input-/Output-Größen sowie SHA-256-Digests und akzeptiert bei `READY` nur die
  interne Referenz `result`.
- `include/bruce/worker/worker_run_log_v1.hpp` und
  `src/worker/worker_run_log_v1.cpp` bilden den append-only JSONL-Vertrag
  `run/control/run-log.jsonl`.
- `include/bruce/worker/worker_abort_control_v1.hpp` und die V5-Integration
  bilden den owner-only Abort-Marker für einen laufenden Run.
- `apps/dag_scheduler_v5_cli.hpp` verwendet diese Verträge bereits für V5
  `execute` und `payload`, inklusive `--retry`, `--workers`, Budgets, JSONL
  Ereignissen und `cancel`.

**IMPLEMENTIERT UND BELEGT:** `Common Runtime Host V2` ist als additive
Anwendungsabstraktion über diese V1-Bausteine eingeführt. Die bestehenden
Worker-V1-Verträge bleiben der wire- und dateibasierte Vertrag; Host V2 ist die
Orchestrierung darüber. Die Umsetzung steht in
`include/bruce/application/common_runtime_host_v2.hpp`,
`src/application/common_runtime_host_v2.cpp` und
`apps/dag_scheduler_v5_cli.hpp`; die Verträge sind durch
`tests/unit/common_runtime_host_v2_tests.cpp` und
`tests/integration/test_common_runtime_host_v2_stage3.py` belegt.

**DOKUMENTIERT / KONZEPT:** Netzwerk-Worker, GPU-Ausführung, frei ladbare
Code-Plugins, freie Sprache, ein allgemeiner Beweiser, externe Payload-Provider
und eine mehrstufige globale Runtime gehören nicht zu Stage 3. Sie benötigen
spätere, eigene Verträge und Sicherheitsgrenzen.

## Nutzerentscheidungen für Stage 3A

Diese Punkte gelten als festgelegt, sofern die Spezifikation nicht bei der
Prüfung geändert wird:

1. **Ein Worker pro Lauf:** Stage 3A startet genau einen Attempt und genau
   einen lokalen Worker-Child; der Supervisor darf dafür zusätzlich einen
   internen Watchdog-Hilfsprozess starten. `workers > 1` und mehrere unabhängige Nodes
   werden erst in Stage 3B verdrahtet.
2. **Bruce bleibt Chef:** Kein Request darf einen alternativen Executable-Pfad,
   ein Signal, eine PID oder einen fremden Prozessstarter liefern. Der Host
   wählt den internen Child-Modus und kontrolliert dessen Lebensdauer.
3. **Resultat ohne Roh-Payload:** Der Worker liefert nur Status, Result-Code,
   Usage, Digest und relative Artefakt-Referenz. Rohbytes bleiben im privaten
   Attempt-Verzeichnis.
4. **Retry nur ausdrücklich:** Stage 3A führt keinen automatischen Retry ein.
   Ein späterer Retry wird ausschließlich durch eine vorhandene CLI-Option
   aktiviert; ein Host-API-Aufruf wiederholt nicht implizit.
5. **JSONL als Übergabeschicht:** Bruce schreibt und validiert die vorhandene
   `worker-run-log-v1`-Datei. Der Worker schreibt sie nicht.
6. **Grace-Periode:** Die V1-Konstante von `1000 ms` bleibt fest. Sie wird
   weder als Request-Feld noch als CLI-Option exponiert.
7. **Budgets:** Wall-Time, Inputbytes, Outputbytes, Operationen und der
   Prozessadressraum des Workers bleiben getrennte harte Grenzen und werden im
   Resultat diagnostisch ausgewiesen. Eine aggregierte Speichergrenze für die
   gesamte Prozessgruppe ist in Stage 3A nicht behauptet.

## Host-V2-API

Die neue API ist bewusst klein und nimmt den bestehenden Worker-Request auf,
statt ein zweites Payload- oder Frame-Modell zu definieren.

```cpp
struct CommonRuntimeHostV2Request {
    worker_v1::WorkerRequest worker_request;
};

struct CommonRuntimeHostV2Context {
    std::filesystem::path run_root;
    std::filesystem::path worker_executable;
    worker_v1::Budget budget;
    std::uint64_t grace_ms{1000};
};

struct CommonRuntimeHostV2Outcome {
    worker_v1::WorkerResult result;
    int host_exit_code{};
};

struct CommonRuntimeHostV2Run;

CommonRuntimeHostV2Run start_single_worker_v2(
    const CommonRuntimeHostV2Request&, const CommonRuntimeHostV2Context&);

std::optional<CommonRuntimeHostV2Outcome> poll_single_worker_v2(
    CommonRuntimeHostV2Run&);

CommonRuntimeHostV2Outcome finish_single_worker_v2(CommonRuntimeHostV2Run&);

void request_cancel_v2(CommonRuntimeHostV2Run&);
```

Die konkreten Namen können bei der Implementierungsplanung angepasst werden;
die Semantik ist bindend:

- `worker_request.budget` muss bytegenau mit `context.budget` übereinstimmen.
- `worker_executable` ist im hostkontrollierten Kontext auf den kanonischen
  Bruce-Prozess (`/proc/self/exe`) aufzulösen; ein fremder Pfad aus dem
  Nutzerrequest ist unzulässig.
- `grace_ms` akzeptiert ausschließlich `1000`.
- `run_root` muss owner-only sein oder vom Host neu mit Modus `0700` angelegt
  werden. `run_root` und `worker_executable` gehören zum hostkontrollierten
  Kontext und sind kein frei setzbarer Worker-Requestinhalt.
- Die API gibt niemals den Child-PID, freie stderr-Ausgabe oder lokale
  Vollpfade als Nutzdaten zurück.
- Ein laufender Handle bleibt intern beim Host. Externe Abbruchsteuerung läuft
  über den bestehenden owner-only Marker und wird im Host in
  `request_cancel` übersetzt.

**IMPLEMENTIERT UND BELEGT:** Die API ist für Stage 3A implementiert. Die
bereits vorhandene Funktion `worker_v1::start_attempt` bleibt die niedrigere
Prozessgrenze. Die Handle-Form ist absichtlich asynchron, damit der Host
zwischen `poll_single_worker_v2`-Aufrufen einen Abort-Marker beobachten kann.
Die Belege sind der Host-V2-Header und die zugehörigen Unit- und
V5-Integrationstests oben.

## Host-Ablauf

```text
Host-V2-Request
    |
    v
Request-/Budget-/Pfadprüfung
    |
    v
private Run-Struktur + RUN_CREATED
    |
    v
ATTEMPT_STARTED + STATUS=RUNNING
    |
    v
worker_v1::start_attempt()
    |
    +--> periodisch poll_attempt()
    |       |
    |       +--> STATUS_QUERY / STATUS
    |       +--> Wall-Time-Watchdog
    |       +--> Output-/Digest-Prüfung
    |
    +--> Abort-Marker entdeckt: request_cancel()
    |
    v
PROCESS_EXITED + RESULT_RECEIVED
    |
    +--> READY: die bereits durch den Supervisor validierte
    |          Artefaktreferenz übernehmen, ARTIFACT_ACCEPTED
    +--> sonst: terminaler Fehlerstatus ohne Artefakt
    |
    v
RUN_FINISHED + metadata-only Host-Resultat
```

Der Host darf keine weitere Arbeit starten, sobald der Abort-Marker beobachtet
wurde. Für Stage 3A gibt es ohnehin nur einen Attempt. Ein Ergebnis, das vor
der Host-Cancellation bereits übernommen wurde, wird nach der bestehenden
linearen Reihenfolge bewertet; ein noch nicht übernommenes `READY` darf nach
Cancellation nicht mehr akzeptiert werden.

## Prozess-, Speicher- und Abbruchvertrag

Der Host verwendet die bestehende Supervisor-Mechanik:

- `fork`/`execv` startet den Child im internen Modus `worker-v1 child`.
- Der Child erhält stdin/stdout als lokale Pipes; stderr wird nach `/dev/null`
  gelenkt und nicht in öffentliche Berichte übernommen.
- Der Child bildet eine eigene Prozessgruppe und erhält
  `PR_SET_PDEATHSIG=SIGKILL`.
- `RLIMIT_AS` wird auf `max_memory_bytes` gesetzt. Das ist die harte
  Adressraumgrenze für den Worker und vererbte Nachfahren jeweils pro Prozess;
  es ist keine aggregierte Prozessgruppen-Grenze.
- Ein separater Watchdog überwacht `max_wall_ms`; bei Ablauf folgen SIGTERM,
  nach einer kurzen internen Eskalationsfrist SIGKILL und Prozessgruppen-
  bereinigung.
- Bei Host-Cancellation sendet Bruce zuerst den bestehenden `CANCEL`-Frame,
  wartet maximal die feste Grace-Periode von `1000 ms` und eskaliert danach.
- Input-Dateien, Output-Datei und Referenzen werden descriptor-relativ,
  symlink-sicher und innerhalb des privaten Run-Roots geprüft.

Die Peak-Messung ist eine Diagnosemetrik, nicht die Durchsetzung der Grenze:
`RLIMIT_AS` begrenzt den Adressraum des Worker-Prozesses; die Messung setzt für
den beobachteten Worker-PID den größten Wert aus `VmHWM`/`VmRSS` und
`ru_maxrss` zusammen. Sie ist keine aggregierte Messung aller möglichen
Nachfahren. Ein RLIMIT-bedingtes Beenden kann deshalb als `PROCESS_CRASHED`
ankommen; `MEMORY_LIMIT_EXCEEDED` ist im vorhandenen Child-Vertrag für
explizit erkannte `std::bad_alloc`-Fehler belegt. Falls Linux keine verwertbare
Peak-Messung liefert, darf der Host keinen falschen exakten Peak behaupten.

## JSONL- und Ergebnisvertrag

Stage 3A verwendet ausschließlich die bestehende Datei:

```text
<run-root>/control/run-log.jsonl
```

Die minimale Ereignisfolge eines erfolgreichen Single-Worker-Laufs ist:

```text
RUN_CREATED
ATTEMPT_STARTED
STATUS(RUNNING)
PROCESS_EXITED(READY)
RESULT_RECEIVED(READY)
ARTIFACT_ACCEPTED(READY)
RUN_FINISHED(READY)
```

Bei einem kontrollierten Abbruch kommt `CANCEL_REQUESTED(CANCELLED)` vor dem
terminalen Attempt-Ereignis. Bei Timeout, Protokollfehler, Digestfehler,
Outputlimit oder Prozessabsturz wird kein Artefakt akzeptiert. Jede Zeile muss
den vorhandenen `worker-run-log-v1`-Validator passieren; Sequenzen steigen
monoton und jede Artefaktzeile enthält nur Referenz, Größe und SHA-256.

Das Host-Ergebnis bleibt metadata-only:

```text
status
failure_code
result_code
payload_digest | null
payload_size
reference | null
usage.wall_ms
usage.input_bytes
usage.output_bytes
usage.operations
usage.peak_memory_bytes
host_exit_code
```

`reference` ist die öffentliche logische Referenz, nicht der private lokale
Dateiname. Für einen nicht erfolgreichen Status sind Referenz, Digest und
Payloadgröße leer beziehungsweise null.

Die Abbildung ist kanonisch: Im internen `WorkerResult` müssen bei `READY`
`output_artifact_ref.reference == "result"`,
`output_artifact_ref.size_bytes == payload_size` und
`output_artifact_ref.sha256 == payload_digest` gelten. Erst nach dieser
Supervisor-Prüfung darf der Host die öffentliche `reference` setzen. Der
öffentliche Digest und die öffentliche Größe werden aus genau diesen validierten
Feldern übernommen; eine zweite, abweichende Digest-/Größenquelle ist ungültig.

Die oben gezeigte Ereignisfolge ist der Host-V2-Lebenszyklusvertrag. Der
allgemeine JSONL-Validator prüft bereits Schema, Kanonizität, monotone Sequenz,
Referenz, Größe, Digest und `fsync`; er erzwingt jedoch nicht selbst jede
vollständige Ereignisfolge. Diese Reihenfolge wird durch den Host und den
Inspector-Test abgesichert.

## Zustands- und Fehlervertrag

| Host-Zustand | Auslöser | Aktion | terminales Ergebnis |
|---|---|---|---|
| `CREATED` | gültiger Request | Run-Log anlegen | `RUN_CREATED` |
| `STARTING` | Attempt wird vorbereitet | Verzeichnisse/Input prüfen | `PROCESS_START_FAILED` bei Fehler |
| `RUNNING` | Child antwortet oder läuft | pollen und Budgets überwachen | `READY`, `FAILED` oder `BLOCKED` |
| `CANCEL_REQUESTED` | Marker oder interner Abort | `CANCEL`, Grace, Eskalation | `CANCELLED` |
| `TIMED_OUT` | Watchdog-/Host-Frist | Prozessgruppe beenden | `TIMEOUT` |
| `RESULT_VALIDATION` | Resultat empfangen | Frame, Usage, Datei und Digest prüfen | `PROTOCOL_ERROR`/`DIGEST_MISMATCH` |
| `FINISHED` | Abschluss geschrieben | `RUN_FINISHED` anhängen | Host-Exit `0` nur bei `READY` |

`PROCESS_CRASHED`, `PROCESS_START_FAILED`, `TIMEOUT`, `CANCELLED`,
`INPUT_LIMIT_EXCEEDED`, `OUTPUT_LIMIT_EXCEEDED`,
`OPERATION_BUDGET_EXCEEDED`, `MEMORY_LIMIT_EXCEEDED`, `DIGEST_MISMATCH` und
`PROTOCOL_ERROR` werden als terminale, kontrollierte Resultate abgebildet. Ein
Fehler darf keinen erfolgreichen Artefaktbezug zurücklassen.

## CLI-Integration

Der erste öffentliche Endpunkt bleibt der vorhandene V5-Befehl:

```text
bruce dag-scheduler-v5 execute SPEC --output BUNDLE
```

Die aktuelle V5-Implementierung besitzt bereits die passenden Defaults und
Maximalwerte:

| Option | Default | Maximum |
|---|---:|---:|
| `--retry` | `0` | `3` |
| `--max-wall-ms` | `30000` | `600000` |
| `--max-input-bytes` | `65536` | `1048576` |
| `--max-output-bytes` | `65536` | `5242880` |
| `--max-operations` | `1000000` | `10000000` |
| `--max-memory-bytes` | `268435456` | `2147483648` |
| `--workers` | `1` | `4` |

Belege: `apps/dag_scheduler_v5_cli.hpp`, `Options`, `parse_options` und
`execute`. Stage 3A ändert diese V5-Oberfläche nicht. `--workers N` und
mehrere DAG-Nodes bleiben für die nächste Ausbaustufe der gemeinsame
Orchestrator; ein Host-V2-Single-Run darf intern nur einen aktiven Attempt
besitzen.

## Implementierungsgrenzen

Die erste Implementierungsplanung darf nur folgende Änderungen enthalten:

1. neue additive Host-V2-Header-/Source-Datei;
2. Wiederverwendung von `worker_v1::start_attempt`, `poll_attempt`,
   `request_cancel` und `WorkerRunLog`;
3. eine schmale Adapter-Integration aus dem bestehenden V5-Pfad für einen
   einzelnen Attempt;
4. Unit-Tests für Request-/Budget-/Lebenszyklusentscheidungen;
5. einen fokussierten Integrationstest für JSONL, Digest/Referenz, Timeout und
   Cancellation;
6. CMake-Einträge und Dokumentation der neuen Host-V2-Verträge.

Nicht Teil dieser Planung sind die Extraktion des gesamten V5-DAG-Schedulers,
eine Änderung des Worker-Frameformats, ein dauerhafter Artifact Store, neue
Netzwerkrechte, neue Plugin-Lader oder mehrere Prozesse pro Lauf.

## Abnahmekriterien

Für Stage 3A wurden alle folgenden Fälle als **IMPLEMENTIERT UND BELEGT**
reproduzierbar geprüft:

- ein gültiger Single-Worker-Request erzeugt ein `READY`-Resultat mit
  passendem SHA-256 und nur Digest/Referenz im öffentlichen Bericht;
- ein ungültiger Request startet keinen Child-Prozess und erzeugt einen
  kontrollierten Fehler;
- ein Worker, der die Wall-Time überschreitet, endet mit `TIMEOUT`;
- ein Worker, der `CANCEL` ignoriert, wird nach `1000 ms` kontrolliert beendet;
- Output- und Operationsgrenzen werden als getrennte Fehlercodes ausgewiesen;
  ein erzwungener Adressraumabbruch wird als `PROCESS_CRASHED` oder, bei
  expliziter Child-Erkennung, als `MEMORY_LIMIT_EXCEEDED` ausgewiesen;
- ein Digest-/Größenfehler akzeptiert kein Artefakt;
- das JSONL-Log ist kanonisch, monoton sequenziert, fsync-gesichert und durch
  den vorhandenen Inspector lesbar;
- ein Prozessabsturz lässt keinen laufenden Worker oder verwaiste
  Prozessgruppenmitglieder zurück; der interne Watchdog-Hilfsprozess wird
  ebenfalls bereinigt;
- bestehende V2-/V3-/V4-CLI-Verträge bleiben bytegleich;
- relevante CTest-Ziele und die bestehenden CLI-Dokumentationstests bestehen.

## Offene Entscheidungen nach der Spezifikationsprüfung

Folgende Punkte werden bewusst erst nach Bestätigung dieses Designs in der
Implementierungsplanung entschieden:

- der endgültige Name und die Sichtbarkeit der Host-V2-C++-Typen;
- ob der V5-Adapter zunächst nur Single-Node oder auch bereits einen
  Multi-Node-Delegationspfad nutzt;
- ob der Host-Exitcode als separates JSON-Feld oder nur als Prozess-Exit
  dokumentiert wird;
- ob der vorhandene `worker-run-log-v1`-Inspector um einen expliziten
  Host-V2-Vertrag ergänzt wird oder zunächst unverändert bleibt;
- welche zusätzliche Diagnose für nicht lesbare `/proc`-Peakwerte öffentlich
  ausgewiesen wird.

Nicht eindeutig aus dem Repository ableitbar ist derzeit nur, ob der spätere
Multi-Node-Orchestrator die vorhandene V5-Scheduler-Schleife direkt übernimmt
oder die neue Host-V2-API als eigenständige Laufzeitkomponente zwischen V5 und
Worker-Supervisor schaltet. Diese Entscheidung gehört nicht in Stage 3A.

## Belegstellen

- `include/bruce/application/common_runtime_host_v1.hpp` — bisherige
  Kompatibilitäts-/Delegationsgrenze.
- `include/bruce/worker/worker_boundary_v1.hpp` — Worker V1 Daten- und
  Framevertrag.
- `include/bruce/worker/worker_supervisor_v1.hpp` — Start-/Poll-/Cancel-API.
- `src/worker/worker_supervisor_v1.cpp` — Linux-Prozessgrenze,
  Watchdog, Budgets, Reaping, Digest- und Output-Prüfung.
- `include/bruce/worker/worker_run_log_v1.hpp` und
  `src/worker/worker_run_log_v1.cpp` — kanonisches JSONL-Run-Log.
- `include/bruce/worker/worker_abort_control_v1.hpp` und
  `src/worker/worker_abort_control_v1.cpp` — owner-only Abort-Marker.
- `apps/dag_scheduler_v5_cli.hpp` — V5-Defaults, CLI-Optionen,
  Single-/Multi-Node-Schleife, Retry, Cancel und bestehende Log-Ereignisse.
- `tests/unit/worker_supervisor_v1_tests.cpp` — Supervisor-, Timeout-,
  Cancellation-, Reaping- und Nachfahrenprüfungen.
- `tests/unit/worker_run_log_v1_tests.cpp` und
  `tests/unit/worker_run_inspector_v1_tests.cpp` — JSONL-Vertrag und
  Recovery-/Inspector-Prüfungen.
