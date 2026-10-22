# Phase-DAG V3 Cockpit Inspector — Implementation Plan

> Required execution method: Subagent-Driven Development. Each task receives an implementation pass, a task-scoped review, and any required fix/re-review before the next task starts.

**Goal:** Den lokalen, read-only Cockpit-Inspector additiv um eine explizite Phase-DAG-V3-Ansicht erweitern. V2 bleibt unverändert erreichbar und fachlich kompatibel.

**Architecture:** Der Inspector erhält eine zweite Replay- und JSON-Projektion für V3. Der HTTP-Server besitzt getrennte Versionrouten. Die eingebettete UI verlangt eine explizite Auswahl und zeigt ausschließlich text-sichere Metadaten. Alle Uploads verbleiben im Request-Speicher.

**Spec:** [2026-10-13-phase-dag-v3-cockpit-inspector-design.md](../specs/2026-10-13-phase-dag-v3-cockpit-inspector-design.md)

## Globale Grenzen

- POST /api/v1/inspect und die bisherige V2-Antwort bleiben unverändert; V2 bleibt bei 1 MiB.
- POST /api/v3/inspect akzeptiert nur BRUCE_PHASE_DAG_RUN_V3 und höchstens 5 MiB.
- Beide Prüfpfade verwenden ausschließlich die festen V2- beziehungsweise V3-NanoCell-Kataloge und deterministisches Replay.
- JSON und DOM enthalten niemals Rohpayloads, Requestbytes, Evidencebytes, Kontextdateiinhalte oder interne Fehlertexte.
- Der Server bindet ausschließlich 127.0.0.1, speichert keine Uploads, startet keine Ausführung und erhält keine neue Schreibroute.
- Generierte CLI-Artefakte werden nur durch docs/cli/verify_examples.py in einer vollständigen temporären Kopie erzeugt oder aktualisiert.
- Git-Status, Commits und Git-Diffs sind in diesem Checkout nicht verfügbar. Für Reviews werden vor Aufgaben Dateischnappschüsse und Unified-Diffs unter runs/ verwendet.

## Task 1: V3-Replay-Projektion und C++-Verträge

**Dateien:**

- Ändern: mods/cockpit/src/cockpit_inspector.hpp
- Ändern: mods/cockpit/src/cockpit_inspector.cpp
- Ändern: tests/mods/cockpit/cockpit_inspector_tests.cpp

**Schnittstellen:**

- `cockpit_max_inspect_bytes_v1` bleibt exakt 1 MiB.
- Ergänze eine getrennte V3-Grenze von 5 MiB und `inspect_phase_dag_v3_bundle(std::string_view)`.
- Die neue Funktion verwendet `replay_phase_dag_run_v3(bytes, phase_dag_nanocell_catalog_v3())` und normalisiert jeden `Error` zu der vorhandenen festen REJECTED-Antwort.
- Der erfolgreiche V3-JSON-Projektor enthält Knoten, Inputs, Receipts und Trace gemäß Spec. Jeder V3-Input und -Output enthält nur Digest und `payload_size`; keines der Bytefelder erscheint.

**TDD-Schritte:**

1. Erweitere den C++-Vertrag zuerst um eine NanoCell-V3 → Payload-Gate-V3-Fixture, den Hilfsmodus `--write-valid-v3-bundle`, eine V3-Validprojektion, V2/V3-Kreuzablehnung, Manipulation und die Grenzen 1 MiB/5 MiB.
2. Baue den bestehenden `bruce_cockpit_inspector_tests`-Target in einer vollständigen temporären Kopie: die neuen Symbole und Tests müssen zunächst rot sein.
3. Implementiere getrennte V3-Status-, Outcome- und Trace-Renderer und die V3-Input-/Output-JSON-Projektion. Teile keine V2-Typen oder Bundlemarker als V3-Vertrag.
4. Baue und führe `cockpit_inspector_contracts`, `phase_dag_v3_contracts` sowie `phase_dag_payload_v3_contracts` aus. Der V2-Cockpitvertrag muss unverändert grün bleiben.

## Task 2: Versionierte HTTP-Routen und sichere Auswahloberfläche

**Dateien:**

- Ändern: mods/cockpit/src/cockpit_inspector_server.cpp
- Ändern: mods/cockpit/src/cockpit_inspector_assets.hpp
- Ändern: mods/cockpit/src/cockpit_inspector_assets.cpp
- Ändern: tests/mods/cockpit/test_cockpit_inspector.py

**Schnittstellen:**

- Der Server setzt die Transportgrenze auf 5 MiB, registriert zusätzlich nur POST /api/v3/inspect und behält alle anderen Routen sowie 404-Regeln bei.
- Die V2-Route ruft weiterhin nur den V2-Inspector auf; die V3-Route nur den V3-Inspector. HTTP 200, 422, 413 und 500 folgen dem bestehenden festen JSON-Fehlervertrag.
- Ersetze das V2-exklusive HTML durch eine klar benannte gemeinsame Inspector-Ansicht mit Versionauswahl. Der Browser wählt Zielroute und lokale Größenprüfung explizit aus; er erkennt Formate nicht automatisch.
- Die Anzeige verwendet weiterhin ausschließlich textContent und bildet bei V3 `payload_size` und Digests ab. Sie führt weder Rohbytes noch deren Beispieltext in DOM oder Fehlermeldungen.

**TDD-Schritte:**

1. Ergänze zuerst Python-HTTP- und DOM-Tests: V3-Upload, V2/V3-Kreuzablehnung, 5-MiB-V3-413, V2-1-MiB-Fachgrenze trotz größerem Serverlimit, Route-/Methodeninventar, sichtbare Auswahl und Ausschluss von Raw-Byte-Leaks.
2. Führe ausschließlich die neuen Workflowtests gegen die noch V2-only gebaute Vollkopie aus; sie müssen rot sein.
3. Implementiere die Route und anschließend die UI. Bewahre die Loopbackbindung, die Ausnahmebehandlung, Reset- und Request-Lebensdauer sowie V2-Verhalten.
4. Führe `cockpit_inspector_workflows`, `cockpit_inspector_contracts`, V2-/V3-CLI-Workflows und einen NanoCell-off-Build aus. Dokumentiere Socket- oder belegte-Port-Umgebungsgrenzen getrennt von Produktfehlern.

## Task 3: Dokumentation, gemessene Artefakte und Integrationsabnahme

**Dateien:**

- Ändern: README.md
- Ändern: DEPENDENCIES.md
- Ändern: docs/architecture/03-blueprint-consolidation.md
- Ändern: docs/architecture/repository-structure.md
- Ändern: docs/cli/README.md
- Ändern: docs/cli/commands.md
- Ändern: docs/cli/formats.md
- Ändern: docs/implementation/README.md
- Ändern: tests/mods/cockpit/test_cockpit_inspector.py
- Nur über Generator aktualisieren: docs/cli/source-index.md, docs/cli/examples.json, docs/cli/validation.md

**TDD-Schritte:**

1. Ergänze die Dokumentationsassertions zuerst um die zwei Inspect-Routen, V2-1-MiB-/V3-5-MiB-Grenzen, den Payloadschutz, die explizite Auswahl und die unveränderten read-only Grenzen. Die vorhandene V2-Evidenz bleibt Teil der Assertions.
2. Lasse diese Assertions vor den Textänderungen rot laufen.
3. Aktualisiere ausschließlich die Human-authored Dokumentation. Jede Aussage über V3 wird als IMPLEMENTIERT UND BELEGT erst nach erfolgreicher Implementierung formuliert; kein Cockpit-V3-Execution-, Speicher- oder Plugin-Claim.
4. Kopiere das gesamte Repository in einen nativen temporären Pfad, baue dort `bruce`, führe dort `docs/cli/verify_examples.py` aus und kopiere exakt die drei Generatorartefakte zurück. Bearbeite sie nie manuell.
5. Prüfe Dokumentationstests, lokale Links, C++- und HTTP-Verträge sowie die geeignete vollständige CTest-Suite. Führe vor Abschluss die vorhandenen Prüfpfade aus und dokumentiere unvermeidbare Sandboxgrenzen präzise.

## Review- und Abnahmefokus

- V2 darf weder durch V3-Uploads akzeptieren noch seine 1-MiB-Fachgrenze verlieren.
- Eine erfolgreiche V3-Antwort muss die tatsächlichen direkten Input-Payloadgrößen und Digests enthalten, aber keine Payloadbytes.
- Ein neu gehashter manipulierter V3-Bundleinhalt muss beim V3-Replay scheitern.
- Der HTTP-Server darf keine Route außerhalb GET /, GET /api/v1/health, POST /api/v1/inspect und POST /api/v3/inspect annehmen.
- Der V3-UI-Pfad muss genauso text-sicher wie V2 sein; automatische Headererkennung ist verboten.
- Alle generierten Artefakte müssen aus einer Vollkopie stammen und bytegleich mit deren Generatorergebnis sein.
