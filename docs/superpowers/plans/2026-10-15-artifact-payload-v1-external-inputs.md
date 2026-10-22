# BRUCE Artifact Payload V1.4 — External Local Inputs Implementation Plan

> **Für Implementierungsagenten:** Die Umsetzung erfolgte taskweise nach
> `superpowers:subagent-driven-development`. Der Plan dokumentiert den
> freigegebenen und inzwischen belegten Implementierungsstand.

**Ziel:** Den freigegebenen V1.4-Sidecar-Pfad in den bestehenden
`dag-scheduler-v5 payload`-Ablauf integrieren. Ein Node kann damit ausdrücklich
ausgewählte Objekte aus einem bereits vorhandenen lokalen Artifact Store
konsumieren, während Bruce Store-Zugriff, Provenienz, Budgets, Materialisierung,
Worker-Grenze und Veröffentlichung vollständig kontrolliert.

**Spec:**
`docs/superpowers/specs/2026-10-15-artifact-payload-v1-external-inputs-design.md`

**Status:** **IMPLEMENTIERT UND BELEGT** — die geplante V1.4-Umsetzung ist im
Produktionscode integriert und durch fokussierte Verträge, Workflow- und
Dokumentationsprüfungen verifiziert.

## Leitplanken

- Ohne `--external-inputs` bleibt der V1.3-Ablauf unverändert.
- Die neue Option ist ausschließlich für
  `bruce dag-scheduler-v5 payload SPEC --output BUNDLE --store STORE` gültig.
- Der Sidecar wird als owner-only, reguläre Datei mit einem Hardlink und ohne
  Symlink gelesen; seine exakten Bytes werden vor dem Parsen gehasht.
- Es gibt nur lokale, explizit ausgewählte Stores. URLs, Netzwerkzugriffe,
  Provider-Auswahl, freie Plugins und fremde Storepfade sind ausgeschlossen.
- Bruce liest und verifiziert Manifest und Objekt. Der Worker erhält nur
  Bruce-eigene relative Attempt-Referenzen mit Digest und Größe.
- Direkte In-Run-Payloads behalten ihre kanonische Abhängigkeitsreihenfolge;
  externe Bindungen werden je Node in der Sidecar-Reihenfolge angehängt.
- Die kombinierte Zahl aus direkten und externen Payloadobjekten ist auf 15
  begrenzt, weil der unveränderte WorkerRequest einen Platz für den Request
  selbst reserviert.
- Struktur-, Vertrags-, Node- und Arityfehler werden vor der Laufinitialisierung
  mit Exit 2 abgewiesen. Fehler beim konkreten Manifest, Objekt oder Attempt
  setzen den Node auf `FAILED` und enden mit Exit 1.
- Externe Referenz-, Integritäts-, Berechtigungs- und Budgetfehler werden nie
  implizit wiederholt. Nur die bestehende explizite `--retry N`-Option darf
  temporäre Workerfehler wiederholen.
- Reports bleiben metadata-only. Sidecar-Digest und externe Count/Bytes werden
  ergänzt; IDs, Provenienz, Pfade, Payloadbytes und OS-Diagnosen bleiben außen
  vor.
- V2, V3, V4, bestehende V5-Routen, Worker-Frame und Resolve-Vertrag werden
  nicht erweitert. Freie Sprache, allgemeines Beweisen, GPU-Ausführung und
  große übergreifende Runtime-Orchestrierung bleiben **DOKUMENTIERT / KONZEPT**
  beziehungsweise zukünftiger Scope. :codex-annotation{index="1"}

## Betroffene Dateien und Grenzen

| Datei | Aufgabe |
|---|---|
| `include/bruce/artifact/artifact_payload_external_v1.hpp` | V1.4-Sidecar-, Binding- und Fehler-Typen; Parser-/Digest-Schnittstelle |
| `src/artifact/artifact_payload_external_v1.cpp` | bounded Sidecar-Lesen, exakte JSON-Struktur, Bindungsvalidierung und Digest |
| `include/bruce/artifact/artifact_payload_v1.hpp` / `src/artifact/artifact_payload_v1.cpp` | nur additive Wiederverwendung der bestehenden Auswahl und Materialisierung; V1.3-Typen unverändert |
| `apps/dag_scheduler_v5_cli.hpp` | Option, Preflight, externe Auswahl, kombinierte Materialisierung, Report und Fehlerabbildung |
| `tests/unit/artifact_payload_external_v1_tests.cpp` | Sidecar-Syntax, Dateisicherheit, Digest, Bindungen und Fehlerklassen |
| `tests/unit/dag_scheduler_v5_payload_cli_tests.cpp` | neue Option, Route-Isolation, Exit-Klassen und Reportfelder |
| `tests/integration/test_dag_scheduler_v5_payload.py` | reale lokale Stores, einzelne/mehrere Bindungen, Mischpfad und Fehlerfälle |
| `CMakeLists.txt` | neue Quelle und fokussierter Unit-Test; bestehende Ziele unverändert registrieren |
| `docs/cli/commands.md`, `docs/cli/formats.md`, `docs/implementation/README.md`, `docs/architecture/03-blueprint-consolidation.md` | implementierte V1.4-Oberfläche, Format und Grenzen dokumentieren |
| `tests/integration/test_cli_docs.py` | exakte CLI-/Format-Aussagen prüfen |
| `runs/bruce-artifact-payload-v1-2026-10-15.md` | Verifikation, Einschränkungen und Evidenzlabel ergänzen |

## Task 1: Sidecar-Typen, sicherer Dateizugriff und kanonischer Parser

**Schnittstelle:**

- `ExternalInputRefV14`: `manifest_id`, `object_index`, `sha256`,
  `size_bytes`, `source_run_id`, `source_node_id`, `source_attempt_id`.
- `ExternalInputBindingV14`: `node_id` plus geordnete Ref-Liste.
- `ExternalInputsSidecarV14`: Vertragskennung, Bindungen und exakte Digest-
  Darstellung der Originalbytes.
- Typed errors unterscheiden Struktur/Vertrag, unbekannten Node/Arity,
  Manifest, Objekt, Limit, Timeout und Berechtigung.

**Schritte:**

1. Failing Unit-Tests für reguläre owner-only Sidecar-Datei, genau einen
   Hardlink, kein Symlink, maximale Dateigröße, absolute/relative Pfade,
   ungültige IDs, Digests, Zahlen und zusätzliche oder falsch geordnete Felder
   schreiben.
2. Den Test gezielt ausführen und den fehlenden V1.4-Typ/Parser als erwarteten
   Fehler dokumentieren.
3. Einen bounded reader mit `O_NOFOLLOW`, `fstat`, Owner-/Mode-/Hardlink-
   Prüfung und vollständigem Byte-Lesen implementieren. Keine Pfadinformation
   aus JSON-Feldern darf als Store- oder Objektpfad dienen.
4. Einen exakten JSON-Parser für die festgelegte Feldreihenfolge ergänzen:
   `contract`, `bindings`, `node_id`, `inputs` und die sieben Ref-Felder. Keine
   unbekannten Felder, Duplikate, Whitespace-Normalisierung oder implizite
   Defaults akzeptieren.
5. Den SHA-256-Digest über die unveränderten Sidecarbytes berechnen und nur
   diesen Digest an die spätere Report-Schicht geben.
6. Unit-Testziel und bestehende Artifact-Store-Verträge ausführen.

## Task 2: Preflight-Bindung an den V3-DAG und kombinierte Limits

**Schnittstelle:**

- Sidecar-Nodes müssen genau einmal im geladenen V3-DAG vorkommen.
- Jede Bindung wird vor Workerstart gegen Callback-Arity, Node-Existenz,
  Duplikate und die kombinierte 15er Payloadgrenze geprüft.
- Die Sidecar-Datei wird vollständig vor Laufinitialisierung gelesen; konkrete
  Store-/Manifest-/Objektfehler bleiben Laufzeitfehler mit Exit 1.

**Schritte:**

1. Failing Tests für unbekannte Nodes, doppelte Node-Bindungen, doppelte
   Objektbindungen, Arityüberschreitung, 15 gültige kombinierte Objekte und
   das 16. Objekt schreiben.
2. Eine immutable Lookup-Struktur `node_id -> ordered external refs` erzeugen.
3. Die bestehende `parse_payload`-Optionserkennung um genau ein
   `--external-inputs PATH` ergänzen; `execute`, `inspect`, `publish` und
   andere Routen müssen die Option weiterhin ablehnen.
4. Sidecar- und DAG-Preflight vor `ArtifactStoreV1::open_existing` ausführen,
   damit Strukturfehler keinen Storezugriff auslösen.
5. Unit-Tests für Defaults, Maximalwerte, doppelte Option, fehlenden Wert,
   Sidecar-/Bundle-Kollision und Route-Isolation ergänzen.

## Task 3: Bruce-seitige Auswahl und bounded Materialisierung

**Schritte:**

1. Für jeden externen Ref den exakt angegebenen lokalen Store öffnen und mit
   der vorhandenen V1.3-Auswahllogik ein kanonisches `READY`-Manifest lesen.
2. `source_run_id`, `source_node_id`, `source_attempt_id`, Index, Digest und
   Größe exakt gegen Manifest und Objekt prüfen. Ein anderer Store oder eine
   alternative Quelle darf nie als Fallback dienen.
3. Direkte In-Run-Selektionen in ihrer bestehenden Reihenfolge und externe
   Selektionen danach in Sidecar-Reihenfolge zu einer Attempt-Auswahlliste
   kombinieren.
4. Requestbytes plus kombinierte Payloadbytes vor der Materialisierung gegen
   `max_input_bytes` prüfen; `max_wall_ms` end-to-end weiterreichen.
5. Jede Materialisierung in einer privaten, neu angelegten Attempt-Datei mit
   vorhandener V1.3-Sicherheitslogik durchführen. Bei jedem Fehler alle
   Dateien dieses Attempts entfernen und keine Partial-Referenz im Worker-
   Request belassen.
6. Die vorhandene `WorkerRequest`-Struktur unverändert verwenden. Nur relative
   private Referenzen, erwartete Größen und Digests werden eingesetzt.
7. Unit-Tests für fehlendes/nicht-READY/provenance-falsches/digest-falsches
   Manifest, Store-Rechte, Deadline, Inputbytes, late corruption und
   leak-freie Bereinigung ergänzen.

## Task 4: DAG-Ausführung, Statusadoption und V1.4-Report

**Schritte:**

1. Die bestehende V5-Orchestrierung so erweitern, dass ein externer Fehler den
   betroffenen Node auf `FAILED` setzt und direkte sowie transitive Nachfolger
   deterministisch auf `BLOCKED` bringt.
2. Nur ein akzeptierter `READY`-Output darf Nachfolger freigeben. Externe
   Eingaben werden nie als neue DAG-Kante oder indirekter Vorgänger
   interpretiert.
3. Die bestehende `--retry N`-Schleife nur für temporäre Workerfehler nutzen;
   externe Bindungs-/Integritäts-/Berechtigungs-/Budgetfehler bleiben ohne
   impliziten Retry.
4. `render_report` und `render_multi_report` auf
   `dag-scheduler-v5-payload-v14` erweitern. Feldreihenfolge:
   `budget`, `external_inputs_digest`, `status`; pro Receipt die bestehenden
   Usage-Felder, danach `external_input_count` und
   `external_input_bytes`.
5. Bei fehlendem Sidecar `external_inputs_digest` als `null` sowie externe
   Count/Bytes als `0` ausgeben. Keine Sidecar- oder Quellmanifest-IDs
   veröffentlichen.
6. CLI-Fehlerabbildung auf die stabilen V1.4-Codes und Exit 1/2-Regeln
   festlegen; stdout/stderr bleiben metadata-only.
7. Unit- und Integrationstests für Einzelnodes, mehrere geordnete externe
   Inputs, gemischte In-Run-/externe Inputs, Folge-Nodes, FAILED/BLOCKED,
   Retry-Grenzen und Report-Hygiene schreiben.

## Task 5: Dokumentation und Verifikation

**Schritte:**

1. CLI-Handbuch und Formatreferenz um Option, Sidecar-Beispiel, Feldordnung,
   Limits, Fehlercodes und Reportvertrag ergänzen.
2. Blueprint und Implementierungsübersicht mit Evidenzlabeln aktualisieren:
   V1.4 als implementiert erst nach Tests; Netzwerk, GPU, freie Plugins,
   freie Sprache, allgemeines Beweisen und große Runtime-Orchestrierung als
   Konzept/Zukunft markieren.
3. `tests/integration/test_cli_docs.py` um V1.4-Route, Version, Limits und
   Kompatibilitätsbehauptungen ergänzen.
4. Generierte CLI-Artefakte ausschließlich über
   `docs/cli/verify_examples.py` in einer temporären Kopie prüfen.
5. Prescribed CMake/CTest-Pfad und fokussierte Payload-, Store-, Worker- und
   V5-Tests ausführen. Zusätzlich lokale Markdown-Links prüfen und bekannte
   Socket-Sandbox-Grenzen exakt dokumentieren.
6. Den Abschlussbericht mit ausgeführten Befehlen, Exit-Codes, Testzahlen,
   nicht verifizierten Bereichen und Statusklassifikationen aktualisieren.

## Verifikation

```sh
cmake -G Ninja -S . -B /tmp/bruce-payload-v14-build \
  -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/bruce-payload-v14-build
ctest --test-dir /tmp/bruce-payload-v14-build -R \
  'artifact_payload_external_v1|artifact_payload_v1|artifact_store_v1|dag_scheduler_v5_payload' \
  --output-on-failure
python3 tests/integration/test_dag_scheduler_v5_payload.py \
  /tmp/bruce-payload-v14-build/bruce
python3 tests/integration/test_cli_docs.py \
  /tmp/bruce-payload-v14-build/bruce
python3 scripts/check_local_doc_links.py
```

Der vorgeschriebene `scripts/with-host-deps.sh`-Pfad wird zusätzlich versucht,
sofern die native Host-Dependency-Konfiguration verfügbar ist. Ein Fehler des
Wrappers oder die bekannte Socket-Sandbox-Einschränkung wird als
Umgebungsproblem getrennt von Produktfehlern berichtet. Da `.git` in dieser
Arbeitskopie nicht als Repository verwendbar ist, dürfen Git-Prüfungen nur als
nicht ausführbar gemeldet werden.

## Planstatus

**IMPLEMENTIERT UND BELEGT.** Die freigegebene Taskfolge ist umgesetzt. Die
V1.3-Route bleibt ohne Sidecar kompatibel; der additive V1.4-Sidecarpfad ist
durch fokussierte CTest-, Workflow- und Dokumentationsprüfungen belegt. Der
einzige Gesamtprüflauf-Fehler ist die bekannte, umgebungsbedingte
Socket-Sandbox-Sperre im Cockpit-Workflow.
