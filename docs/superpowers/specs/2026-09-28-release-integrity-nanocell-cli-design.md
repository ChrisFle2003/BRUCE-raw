# Release-Integrität, NanoCell-CLI und CI

**Status:** Entwurf zur Nutzerprüfung  
**Datum:** 2026-09-28  
**Entscheidung:** Ein frischer CMake-Build ist maßgeblich. NanoCell erhält eine eigene CLI. Der lokale Bestätigungsdialog ist keine endgültige Produktions-Autorisierung. Die Zielarchitektur bleibt im Scope, wird aber in diesem Vorhaben nicht vollständig implementiert.

## 1. Ziel und Erfolgskriterien

Dieses Vorhaben beseitigt die festgestellte Lücke zwischen Quellbaum, vorhandenen Build-Artefakten, CLI-Dokumentation und automatischer Prüfung. Es ergänzt die vorhandene NanoCell-V1-Bibliothek um einen lokalen CLI-Pfad und schafft einen reproduzierbaren CI-Nachweis.

Erfolg bedeutet:

1. Ein CI-Lauf konfiguriert einen **frischen** CMake-Build, baut `bruce` und führt alle über CTest registrierten Tests aus.
2. CI prüft CLI-Dokumentationsartefakte reproduzierbar und scheitert bei nicht eingecheckter Regeneration.
3. `bruce nanocell run` erzeugt eine JSON-Antwort und kann optional ein neues, digest-gebundenes `BRUCE_NANOCELL_RUN_V1`-Bundle exportieren.
4. `bruce nanocell replay FILE` validiert das Bundle und gibt die kanonische Antwort aus.
5. Der exklusive Bundleexport synchronisiert nach der erfolgreichen Veröffentlichung auch das Zielverzeichnis.
6. README, Architekturübersicht und CLI-Handbuch unterscheiden implementierte NanoCell-/Sprachteile sauber von weiterhin geplanten Zielarchitekturteilen.

## 2. Bestand und Grenzen

Der CMake-Einstieg baut `bruce_core` und die CLI und registriert bereits `nanocell_contracts`; die NanoCell-Bibliothek bietet `execute_nanocell_v1`, `pack_nanocell_run_v1` und `replay_nanocell_run_v1`. Es gibt bislang keinen CLI-Dispatcher dafür. `LocalFiles::publish_new` schreibt und synchronisiert die temporäre Datei, publiziert exklusiv über `link`, synchronisiert aber nicht das Zielverzeichnis. Regelaktivierung hat einen separaten `rename`-basierten Commitpfad; dessen Semantik wird nicht verändert.

Die vorhandenen lokalen Build-Verzeichnisse sind keine Freigabegrundlage: mindestens ein vorhandenes Binary meldet die aktuelle Versionsnummer, enthält jedoch nicht den aktuellen Sprach-CLI-Pfad. Der Quellbaum enthält im gelieferten Snapshot kein nutzbares Git-Metadatenverzeichnis. Daher kann hier keine Historie oder ein Commit erzeugt werden.

## 3. Architektur

```text
GitHub Actions
  └─ frischer Checkout
       ├─ CMake Configure + Build
       ├─ CTest
       ├─ CLI-Doku-Regeneration im temporären Kopieraum + Diff
       └─ Secret-Scan

bruce nanocell run
  └─ CLI-Validierung und Referenztechnik-Auswahl
       └─ execute_nanocell_v1
            └─ JSON-Rendern / pack_nanocell_run_v1
                 └─ LocalFiles::publish_new (optional)

bruce nanocell replay FILE
  └─ begrenztes Dateilesen
       └─ replay_nanocell_run_v1
            └─ JSON-Rendern
```

NanoCell bleibt ein eigener, nicht in `ApplicationTask` integrierter V1-Pfad: Die dort modellierten acht mathematischen ApplicationTasks haben Session-/Replay-Phasen und Budgets, die nicht ohne explizite Vertragsänderung auf NanoCell übertragen werden dürfen.

## 4. NanoCell-CLI-Vertrag

### Befehle

```text
bruce nanocell run --core HEX16 --active-index 0..7 \
  --context-id ID --context-file FILE \
  --technique masked-match-v1|rotate-match-v1|context-conflict-v1 \
  [--max-operations 0..16] [--max-trace-entries 2..18] [--output RUN]

bruce nanocell replay RUN
```

`--core` ist genau 16 hexadezimale Zeichen und beschreibt die acht Core-Bytes in Reihenfolge. `--context-file` wird begrenzt gelesen; der Digest wird ausschließlich aus seinen unveränderten Bytes berechnet. Die Technik wird ausschließlich aus der festen NanoCell-Referenztechnikfabrik abgeleitet; keine frei übergebene Bytecode- oder Digest-Schnittstelle wird ergänzt.

Defaults entsprechen dem bestehenden V1-Modell: `max_operations=16`, `max_trace_entries=18`. Unterbrochene Läufe durch Budgetgrenzen bleiben fachliche NanoCell-Resultate mit Status `INCOMPLETE`; `SIGINT` wird für diese erste CLI-Version nicht als neues Cancellation-Protokoll eingeführt.

Erfolgsantworten sind JSON mit einem eigenen `nanocell-cli-v1`-Vertrag, Status, finalem Bytewert (falls vorhanden), Digests, Verbrauch und Trace. Werte vom Typ Byte werden als dezimale JSON-Zahlen ausgegeben. Fehler folgen dem etablierten CLI-Format `{\"error\":\"…\"}` auf stderr mit Exitcode 2. Der Replay-Befehl erzeugt keine neue Datei.

`--output` nutzt den bestehenden exklusiven Export; kein vorhandenes Ziel darf überschrieben werden. Der Publishpfad synchronisiert nach erfolgreichem `link(temp, target)` und nach dem Entfernen der temporären Verknüpfung das Zielverzeichnis. Scheitert diese letzte Synchronisation, kann das Ziel bereits sichtbar sein; der Aufruf meldet deshalb einen eigenen Dauerhaftigkeitsfehler und behauptet weder Rollback noch garantierte Persistenz.

## 5. Release- und CI-Vertrag

Eine GitHub-Actions-Workflowdatei wird ausschließlich auf GitHub-Ereignissen für Push und Pull Request ausgeführt. Sie verwendet einen Ubuntu-LTS-Runner, installiert die deklarierte Buildkette und Systemabhängigkeiten, konfiguriert einen neuen Buildpfad innerhalb des Runner-Workspaces mit `BUILD_TESTING=ON`, baut ihn und führt `ctest --output-on-failure` aus.

Die Doku-Prüfung darf keine Arbeitsbaumdateien ändern: Sie arbeitet in einer temporären Kopie, führt `docs/cli/verify_examples.py` gegen das frische Binary aus und vergleicht danach die erzeugten `source-index.md`, `examples.json` und `validation.md` mit den eingecheckten Dateien. Abweichungen brechen CI ab.

Ein Secret-Scan prüft den Checkout auf typische Schlüssel- und Credential-Muster. Sein Ergebnis ist ein Schutznetz, kein Nachweis der Geheimnisfreiheit. Die endgültige Wahl des Scanners wird im Plan an den ohne zusätzliche produktive Laufzeitabhängigkeit verfügbaren GitHub-Mechanismus gebunden. Die Workflow-Versionen werden auf konkrete, überprüfbare Releases oder Commit-IDs festgelegt.

## 6. Datei- und Dokuänderungen

Voraussichtlich geändert oder ergänzt werden:

| Datei | Änderung |
|---|---|
| `apps/bruce_cli.cpp` | NanoCell-Parsing, Dispatch, JSON-Rendern |
| `src/io/local_files.cpp` | Zielverzeichnis nach erfolgreichem Publish synchronisieren |
| `tests/integration/test_nanocell_cli.py` | Black-box-CLI-Tests für run, replay, Fehler, Export |
| `CMakeLists.txt` | Integrationstest registrieren |
| `.github/workflows/ci.yml` | frischer Build, Tests, Doku-Drift, Secret-Scan |
| `README.md` | aktueller Funktionsstand und frischer Build als Referenz |
| `docs/architecture/03-blueprint-consolidation.md` | implementiert vs. geplant korrigieren |
| `docs/cli/README.md`, `commands.md`, `formats.md`, `results-and-control.md`, `local-files.md` | NanoCell-CLI und Publishfehler dokumentieren |
| `docs/cli/source-index.md`, `examples.json`, `validation.md` | regenerierte Artefakte |

## 7. Regelautorisierung: vorbereitete nächste Architekturphase

Die vorhandene Terminalbestätigung bleibt nur für lokalen Einzelbenutzer-/Entwicklungsbetrieb zulässig. Dieses Vorhaben führt **keine** unvollständige Authentifizierung oder Signaturattrappe ein.

Die Dokumentation erhält stattdessen eine explizite Produktionsgrenze: Eine zukünftige Autorisierungsadapter-Schnittstelle muss die konkrete Änderung, Ziel, Basisversion, Akteur und unabhängige Freigabe binden. Sie benötigt ein getrenntes Berechtigungsmodell und Audit-/Widerrufssemantik. Die Entscheidung über Identitätsanbieter, Schlüsselverwaltung und Deployment erfolgt in einem separaten Architekturvorhaben.

## 8. Nicht-Ziele

- Keine freie natürliche Sprachschnittstelle.
- Keine NanoCell-Integration in `ApplicationTask`, Grid, Experience oder Research-Gates.
- Keine automatische Regelaktivierung.
- Keine neue Datenbank, Netzwerk-API, Plugin-Runtime, Cockpit-, GPU- oder Phase-DAG-Implementierung.
- Keine rückwirkende Korrektur alter Build-Artefakte.

## 9. Tests und Abnahme

1. NanoCell-Unit-Tests bleiben grün.
2. Neue Integrationstests prüfen mindestens `MATCHED`, `NO_MATCH`, `CONTEXT_REJECTED`, `INCOMPLETE`, ungültiges Hex, ungültige Technik, fehlenden Kontext, exklusiven Export und Replay.
3. Ein Export wird byteidentisch durch `nanocell replay` geprüft.
4. Lokale Dateitests decken weiterhin Fehlerpfade, Temp-Cleanup und keine Überschreibung ab. Ein Plattformtest bestätigt die neue Verzeichnis-Synchronisation indirekt durch erfolgreiche Veröffentlichung, nicht durch simulierten Stromausfall; ein Fehler nach `link` darf keinen falschen Nichtvorhandenseits- oder Rollbackanspruch erzeugen.
5. CI baut in einem neuen Pfad und testet den gesamten registrierten Bestand.
6. CI schlägt fehl, wenn die generierten CLI-Dokumentationsartefakte vom Quellstand abweichen.

## 10. Risiken und Entscheidungen

| Risiko | Umgang |
|---|---|
| CLI-Format könnte später schwer zu ändern sein | eigener, versionierter `nanocell-cli-v1`-Vertrag |
| Dateisysteme erlauben kein Directory-fsync | klarer, kontrollierter Fehler; kein fälschlicher Dauerhaftigkeitsanspruch |
| Doku-Generator ist schreibend | ausschließlich in temporärer CI-Kopie ausführen |
| CI-Abhängigkeiten driften | Systemabhängigkeiten ausdrücklich installieren und frischen Build verwenden |
| Terminalbestätigung wird als Authentifizierung missverstanden | explizite Produktionsgrenze und separates Folgevorhaben |
