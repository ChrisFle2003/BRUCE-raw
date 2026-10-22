# V5 Artifact-Store-Importbrücke V1.1 — Implementierungsplan

> **Für agentische Arbeiter:** diesen Plan taskweise mit
> `superpowers:subagent-driven-development` ausführen. Bestehende V2-, V3-,
> V4- und V5-Verträge dürfen nur mit expliziten Bytegleichheitsprüfungen
> berührt werden.

**Ziel:** Einen separaten, read-only geprüften Publikationsschritt für
vollständig gültige V5-Läufe implementieren. Der Befehl
`bruce dag-scheduler-v5 publish BUNDLE --store STORE` importiert ausschließlich
final akzeptierte Worker-Ergebnisse in den lokalen Artifact Store V1 und
veröffentlicht je akzeptiertem Knoten ein kanonisches Manifest.

**Spezifikation:**
`docs/superpowers/specs/2026-10-15-worker-artifact-publish-v1-design.md`

## Festgelegte Entscheidungen

- Ein vollständiger erfolgreicher Lauf benötigt `RUN_FINISHED`, `VALID`,
  `READY` und für jeden Knoten ein verifiziertes `ARTIFACT_ACCEPTED`.
- Es gibt ein Manifest pro akzeptiertem Knoten; ein run-weites Aggregatmanifest
  ist nicht Teil von V1.1.
- `origin` trägt die kontrollierte öffentliche Referenz (`artifact/<node>`);
  das Manifestmodell bleibt unverändert.
- `manifest_id` wird aus der kanonischen Bytefolge
  `BRUCE_DAG_SCHEDULER_V5_PUBLISH_V1\nrun_id=<run>\nnode_id=<node>\nattempt_id=<attempt>\n`
  als `v5-` plus SHA-256 gebildet und als Manifestname validiert.
- Der Publish-Bericht enthält in fester Reihenfolge `contract`, `action`,
  `status`, `run_id`, `manifests`; je Eintrag `node_id`, `attempt_id`,
  `reference`, `manifest_id`, `sha256`, `size_bytes`. `dag_id` wird nicht
  ergänzt, weil der aktuelle Inspector die für die Importbindung nötige
  Knoten-/Attempt-/Digest-Menge bereits liefert und der bestehende Execute-
  Bericht unverändert bleiben soll.
- Ein gemeinsamer interner Validierungstyp (`ValidatedV5RunV1`) wird ergänzt;
  die öffentliche Inspect-Ausgabe bleibt unverändert.
- Mehrere Store-Schreibvorgänge sind nicht transaktional. Bereits immutable
  publizierte Objekte oder Manifeste dürfen bei einem späteren Fehler liegen
  bleiben; V1.1 löscht nichts und besitzt keine GC-/Rollback-Funktion.
- Alle öffentlichen Publikationsfehler verwenden das vorhandene kompakte
  `{"error":"PROTOCOL_ERROR"}`-Envelope und Exit 2.

## Dateien und Grenzen

| Datei/Bereich | Verantwortung |
|---|---|
| `include/bruce/worker/worker_run_inspector_v1.hpp` und `src/worker/worker_run_inspector_v1.cpp` | interne, bereits validierte Importansicht mit Run-/Knoten-/Attempt-/Referenzbindung; öffentliche Inspect-Daten bleiben kompatibel |
| `include/bruce/artifact/artifact_store_v1.hpp` und `src/artifact/artifact_store_v1.cpp` | Wiederverwendung von `put_file` und `publish_manifest`; keine Änderung an Objekt- oder Manifestgrenzen |
| `apps/dag_scheduler_v5_cli.hpp` | Parser, Inspector-first-Publish-Ablauf, sichere Quellenauflösung, Manifestbildung und kanonischer Publish-Bericht |
| `apps/bruce_cli.cpp` | keine neue Top-Level-Architektur; nur vorhandene V5-Route dispatcht die neue Unterroute |
| `tests/unit/worker_run_inspector_v1_tests.cpp` | Regressionen für interne Importliste und bestehende Inspector-Ausgabe |
| `tests/unit/dag_scheduler_v5_cli_tests.cpp` | Syntax, Berichtsfeldreihenfolge, Fehlerhülle, Execute-/Inspect-Bytegleichheit |
| `tests/integration/test_dag_scheduler_v5_publish.py` | reale Einzel-/Mehrknoten-, Wiederholungs-, Manipulations- und Teilpublikationsfälle |
| `docs/cli/commands.md`, `docs/cli/formats.md`, `docs/implementation/README.md`, `docs/architecture/03-blueprint-consolidation.md` | Implementierungsstatus, CLI-/Manifestvertrag und Grenzen |
| `tests/integration/test_cli_docs.py` | Dokumentationsvertrag |
| `runs/bruce-worker-artifact-publish-v1-2026-10-15.md` | reproduzierbarer Abschlussbericht |

## Task 1 — Validierte Importansicht und Publish-Kern

1. Ergänze zuerst Tests für eine vollständige gültige Einzel- und
   Mehrknotenprüfung, die deterministische Importeinträge in V5-Spec-Reihenfolge
   liefert. Prüfe, dass unvollständige, fehlgeschlagene, blockierte,
   abgebrochene oder manipulierte Läufe vor jedem Store-Schreibvorgang
   abgewiesen werden.
2. Ergänze den internen Validierungstyp mit Run-ID, Knoten-ID, Attempt-ID,
   kontrollierter Referenz, Digest, Größe, Status, Resultcode und sicherem
   descriptor-relativem Quellpfad bzw. einer Run-Root-Bindung. Keine privaten
   Pfade nach außen geben.
3. Implementiere die Publish-Kernlogik: Inspector-first, vollständige
   Vorprüfung, erneute `ArtifactStoreV1::put_file`-Bindung mit erwartetem
   Digest/Größe, dann Manifestobjekt und `publish_manifest`.
4. Implementiere die deterministische Manifest-ID und kanonische
   Manifestwerte. Identische Wiederholung muss idempotent sein; abweichende
   vorhandene Objekte/Manifeste müssen als Konflikt enden.
5. Belege, dass Publish weder Worker, Adapter, Replay noch BUNDLE/BUNDLE.run
   verändert. Dokumentiere die bewusst mögliche Teilpublikation ohne Löschung.
6. Führe die fokussierten Worker-/Artifact-/V5-Unit-Tests aus und fordere eine
   scoped Review an. Jede Review-Feststellung geht durch einen Fix-Loop, bevor
   Task 2 beginnt.

## Task 2 — CLI-Route und reale Workflows

1. Ergänze Parser-Tests für exakt
   `dag-scheduler-v5 publish BUNDLE --store STORE`; zusätzliche Optionen,
   fehlende Argumente, `.run` als Bundle, Root-Escape, Symlinks und falsche
   Store-Rechte müssen kompakt mit Exit 2 abgewiesen werden.
2. Ergänze den kanonischen metadata-only Bericht
   `dag-scheduler-v5-publish-v1` mit stabiler Feldreihenfolge und ohne
   Payloadbytes, private Pfade, OS-Fehler oder Geheimnisse.
3. Ergänze reale Binär-Workflows für Einzelknoten, Mehrknoten in Spec-Reihenfolge,
   idempotente Wiederholung, Objekt-/Manifestkonflikt, unvollständigen Lauf,
   manipulierten Bericht/Run-Log, fehlende Datei, Symlink/Hardlink und Fehler
   beim späteren Manifest. Prüfe, dass keine vorhandenen Dateien überschrieben
   werden.
4. Prüfe die bestehenden Execute-/Inspect-Ausgaben bytegleich sowie V2/V3/V4-
   und Artifact-Store-Workflows unverändert.
5. Speichere Taskbericht und fordere eine scoped Review an.

## Task 3 — Dokumentation und Gesamtverifikation

1. Dokumentiere Route, JSON-Felder, Manifestbindung, Status-/Fehlermatrix,
   Idempotenz und Teilpublikationsgrenze. Markiere Download, Readback, GC,
   Netzwerk, externe Provider, automatische Execute-Integration und freie
   Runtime-Funktionen weiterhin als Konzept/Nichtziel.
2. Ergänze Dokumentationstests. Generierte CLI-Artefakte ausschließlich über
   `docs/cli/verify_examples.py` in einer temporären Kopie prüfen.
3. Führe Release-Konfiguration, Build, fokussierte Publish-/Worker-/Artifact-
   Store-/V5-Tests, Dokumentationstests, lokale Markdown-Linkprüfung und den
   vollständigen CTest-Lauf aus. Halte die bekannte Cockpit-Socket-Sandbox-
   Einschränkung separat fest.
4. Prüfe `git diff --check`, Git-Status, lokale Links und Geheimnisscan. Da das
   Repository derzeit kein nutzbares Git-Metadatenverzeichnis besitzt, darf
   kein Commit-/Diff-Erfolg behauptet werden.
5. Erstelle den Abschlussbericht und fordere eine Whole-Change-Review an.
   Erst nach bestandenem Review wird der Plan im SDD-Ledger als abgeschlossen
   markiert.

## Verifikation

```sh
cmake -G Ninja -S . -B /tmp/bruce-worker-artifact-publish-v1-build \
  -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/bruce-worker-artifact-publish-v1-build -j2
ctest --test-dir /tmp/bruce-worker-artifact-publish-v1-build -R \
  'dag_scheduler_v5|artifact_store_v1|worker_run_log_v1|worker_run_inspector_v1' \
  --output-on-failure
python3 tests/integration/test_cli_docs.py \
  /tmp/bruce-worker-artifact-publish-v1-build/bruce
python3 scripts/check_local_doc_links.py
```

Der Host-Dependency-Wrapper bleibt wegen des aktuellen Pfads der bekannten
Native-/GVFS-Einschränkung untergeordnet. Es werden keine Pakete installiert
und keine Netzwerkzugriffe benötigt.
