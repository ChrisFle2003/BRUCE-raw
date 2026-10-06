# Repository-Analyse und nächste Schritte

**Stichtag:** 06.10.2026

**Analysierter Repository-Root:** `Bruce_raw` bei `fa158de` auf `master`
**Arbeitsart:** ausschließlich lesende Analyse; keine Produkt-, Konfigurations-, Build-, Test-, Commit-, Installations- oder Deployment-Änderung.

## 1. Kurzüberblick

- **Projektzweck — Belegt:** BRUCE ist ein lokaler C++20-Forschungskern mit CLI für begrenzte mathematische Forschungs-, Vergleichs-, Replay- und Steuerungsabläufe. Kandidat, Stichprobenbestätigung, Replay und mathematischer Beweis sind ausdrücklich unterschiedliche Aussagen. Belege: `README.md:3-8`, `docs/architecture/03-blueprint-consolidation.md:5-10`.
- **Aktueller Reifegrad — Abgeleitet:** Der Kern ist funktional breit und besitzt Build-, Test-, Paket- und CI-Verdrahtung, bleibt aber bewusst eine lokale, sequenzielle CLI-Anwendung mit festen Laufarten. Er ist weder die vollständige Zielarchitektur noch ein allgemeines Forschungs-Cockpit. Grundlage: `CMakeLists.txt:1-130`, `.github/workflows/ci.yml:10-65`, `docs/architecture/03-blueprint-consolidation.md:7,28,51,72-78`.
- **Wichtigster nächster Schritt — Belegt:** Vor einer neuen Entwicklung muss die lokale Ausführberechtigung von `scripts/with-host-deps.sh` geklärt und wiederhergestellt werden. Im Arbeitsbaum ist der Modus von `100755` auf `100644` geändert; genau dieses Skript ist dokumentierter Build-Einstieg und wird als CTest-Programm aufgerufen. Belege: Git-Status zu Beginn; `README.md:22-24`, `AGENTS.md:47-49`, `CMakeLists.txt:174-177`, `tests/integration/test_with_host_deps.py:25-35`.
- **Größte Risiken:**
  1. **P0, belegt:** Der aktuelle Working Tree ist nach den jüngsten Mod-Commits nicht frisch gebaut oder getestet. Die sichtbaren historischen Nachweise gelten für frühere Stände mit 121 bzw. 130 CTests, während der aktuelle CMake-Stand 138 `add_test`-Registrierungen enthält.
  2. **P0, belegt:** Der nicht ausführbare Wrapper kann den vorgesehenen lokalen Prüfpfad und mindestens `with_host_deps_contract` blockieren.
  3. **P1, belegt:** Die neu gestagten Cockpit-Unterlagen enthalten eine noch nicht konsolidierte Abhängigkeitsentscheidung: das Design verlangt vor Umsetzung die Wahl einer HTTP-Komponente, der Plan nennt bereits `cpp-httplib` v0.59.0. Ein Cockpit ist im Quellbaum weiterhin nicht implementiert.
  4. **P1, belegt:** Sitzung, Gate und Transfer besitzen absichtlich unterschiedliche Budget-, Abbruch- und Fehlersemantik. Eine neue Oberfläche darf diese Verträge nicht unbemerkt vereinheitlichen.

## 2. Geprüfte Grundlagen und Grenzen

### Scope und Git-Ausgangslage

- **Belegt:** Der zuerst geöffnete Ordner `/home/chrisi/Desktop/Burce` war selbst kein gültiger Git-Root: sein sichtbares `.git` enthielt weder `HEAD` noch `config`. Er enthält zwei untergeordnete Git-Repositories. Um den tatsächlichen Arbeitsroot zu bestimmen, wurde nur deren Git-Metadatenstatus geprüft; anschließend wurde ausschließlich `Bruce_raw` untersucht. `Bruce_raw` ist ein vollständiger Checkout und hat den Root `/home/chrisi/Desktop/Burce/Bruce_raw`. Das zweite Repository wurde nicht inhaltlich analysiert.
- **Belegt:** Aktiver Branch ist `master`, `HEAD` ist `fa158de` (`docs: document nanocell mod boundary`). Die Dekoration weist zusätzlich `codex/cockpit-v1` auf denselben Commit aus. Die letzten in der Historie sichtbaren Änderungen betreffen die statische Mod-Grenze und NanoCell-Provenienz (`5665c42` bis `fa158de`).
- **Belegt:** Zu Beginn lagen drei lokale Änderungen vor: zwei gestagte, neue Cockpit-Dokumente und ein nicht gestagter Moduswechsel von `scripts/with-host-deps.sh` von ausführbar zu nicht ausführbar. Keine dieser Änderungen wurde verändert. `git diff --check` und `git diff --cached --check` meldeten keine Whitespace-Fehler.
- **Belegt:** Sensible Konfigurationsinhalte, Secrets und lokale Laufdaten wurden nicht ausgegeben oder inhaltlich untersucht.

### Geprüfte Bereiche

| Bereich | Belege | Aussagegrenze |
| --- | --- | --- |
| Projektzweck, Grenzen und Arbeitsregeln | `AGENTS.md`, `README.md`, `docs/README.md`, `DEPENDENCIES.md` | Dokumentation beschreibt Soll- und Bestandsgrenzen; sie ersetzt keinen aktuellen Laufnachweis. |
| Architektur und öffentliche Abläufe | `docs/architecture/03-blueprint-consolidation.md`, `docs/contracts/06-control-and-publication.md`, `include/bruce/application/application_task.hpp`, `apps/bruce_cli.cpp` | Zentrale Grenzen und Einstiegspunkte wurden nachvollzogen; mathematische Algorithmen wurden nicht einzeln fachlich auditiert. |
| Build, Abhängigkeiten, Tests und CI | `CMakeLists.txt`, `mods/*/CMakeLists.txt`, `.github/workflows/ci.yml`, `tests/integration/test_mod_build.py`, `tests/integration/test_with_host_deps.py` | Registrierung und Prüfabsicht sind belegt; kein Build oder Test wurde in dieser Analyse gestartet. |
| Mod-System und jüngste Änderungen | `include/bruce/mod/*`, `src/mod/*`, `mods/language/`, `mods/nanocell/`, Git-Historie | Statische, optionale Mods sind im Code belegt; ihre aktuelle Ausführung bleibt offen. |
| Vorhaben Cockpit | `docs/superpowers/specs/2026-10-04-cockpit-v1-design.md`, `docs/superpowers/plans/2026-10-04-cockpit-v1.md` | Beide Dateien sind lokale, gestagte Planung; sie belegen keine Implementierung oder Freigabe. |
| Historische Prüfbelege | `docs/implementation/182-control-consolidation-usage.md`, `docs/implementation/183-local-file-adapter-usage.md`, `docs/audit/2026-10-03-repository-analysis-and-next-steps.md` | Ergebnisse gelten nur für die dort genannten älteren Stände. |

### Nicht geprüft

- **Offen:** Die aktuelle Buildumgebung, die tatsächliche Ausführbarkeit des Wrappers nach einer Moduskorrektur, komplette CTest-Ergebnisse, Installation, Paket und generierte CLI-Dokumentation. Diese Prüfungen können lokale temporäre Builds und Testartefakte erzeugen und wurden deshalb nicht im Analyseauftrag ausgeführt.
- **Offen:** Performance, Speicherverbrauch, Installierbarkeit auf anderen Linux-Systemen und Remote-/Produktionsverhalten. Es gab keinen Netzwerk-, Remote-, Benchmark-, Installations- oder Deployment-Aufruf.
- **Offen:** Die fachliche Korrektheit jeder einzelnen mathematischen Strategie und der künftige Produktvorrang des Cockpits gegenüber anderen Konzeptpaketen.

## 3. Repository- und Architekturüberblick

| Komponente | Aufgabe und Beleg | Aktuelle Grenze |
| --- | --- | --- |
| `bruce_core` | C++20-Bibliothek für Anwendung, Mathematik, Evidenz, Experience, Token und lokale Dateiarbeit; GMP/GMPXX und OpenSSL Crypto sind Pflichtabhängigkeiten. `CMakeLists.txt:1-67` | Keine dynamische Plugin-Laufzeit. |
| CLI `bruce` | Parst Befehle, erzeugt feste Task-Varianten, führt sie aus und gibt JSON bzw. Fehler-JSON aus. `apps/bruce_cli.cpp:196-251,406-430,856-872` | Fachliche Fehlerhüllen unterscheiden sich je Steuerungsfamilie. |
| Gemeinsamer Laufvertrag | Acht feste `ApplicationTask`-/`ApplicationRun`-Varianten; Ausführung, Rendering und Packen sind variant-basiert. `include/bruce/application/application_task.hpp:11-20`, `src/application/application_task.cpp:26-113` | Keine dynamische Task-Registrierung oder allgemeine Task-DAG. |
| Sessions, Replay und Gates | Sessionphasen `CREATED → EXECUTING → SERIALIZING → REPLAYING → READY/FAILED`; Bundle, Replay, Bewertung und Gates sind getrennte Schritte. `include/bruce/application/execution_session.hpp:6-71`, `docs/architecture/03-blueprint-consolidation.md:11-28` | In-Memory-Bewertung prüft keine Herkunft; Gateberichte sind keine Berechtigungstoken. |
| Transfer und lokale Regel-/Dateiarbeit | Der lokale Adapter begrenzt Lesen, veröffentlicht exklusiv und führt bestätigte Regelwechsel unter Sperre aus. `include/bruce/io/local_files.hpp:5-19`, `src/io/local_files.cpp:39-123` | Lokales Linux-Modell; Bestätigung authentisiert keinen Menschen; Fehler nach einem erfolgreichen Dateiersatz können sichtbaren Zustand hinterlassen. |
| Feste statische Mods | `bruce_mod_runtime`, Katalog sowie optional gebaute Language- und NanoCell-Mods. `CMakeLists.txt:69-96,119-128`, `src/mod/catalog.cpp:33-49`, `mods/language/CMakeLists.txt`, `mods/nanocell/CMakeLists.txt` | Kein dynamisches Laden; kein Cockpit-Target; NanoCell ist nicht `ApplicationTask`. |
| Prüfinfrastruktur und Auslieferung | CTest registriert C++-Vertragstests und Python-Integrationstests; CPack erzeugt ein TGZ; CI baut Release, führt CTest, Dokumentprüfung, Linkcheck und Secret-Scan aus. `CMakeLists.txt:130-524`, `.github/workflows/ci.yml:10-65` | Es existiert kein in dieser Analyse erhobener aktueller Laufnachweis. |

**Belegt – Kontroll- und Datenfluss:** Der maßgebliche Ablauf lautet CLI → `ApplicationTask` → fachlicher Ausführer → `ApplicationRun` → versioniertes natives Bundle → Replay → Bewertung/Gates. Ein unabhängiger Befund kann getrennt für Erfahrung und Z-Auswahl weiterverwendet werden; das ist kein automatischer Scheduler. `docs/architecture/03-blueprint-consolidation.md:11-28,64-70`.

## 4. Ist-Zustand

### Umgesetzt und belegt

- **Belegt:** Projektversion `0.102.0`, C++20, strenge Warnungen als Fehler, CMake/CTest und CPack. `CMakeLists.txt:1-10,64-67,130-131,517-525`.
- **Belegt:** Lokale CLI und acht feste gemeinsame Laufvarianten einschließlich Forschung, Grid-Varianten, Bruchvergleich und Kompatibilität. `docs/architecture/03-blueprint-consolidation.md:32-37`, `include/bruce/application/application_task.hpp:17-20`.
- **Belegt:** Kernfunktionen für Bundle-/Replay-/Gate- und getrennte Transferwege sind sowohl im Architekturvertrag als auch in CMake-Testregistrierungen sichtbar. `docs/contracts/06-control-and-publication.md:14-24`, `CMakeLists.txt:186-203,448-515`.
- **Belegt:** Optional gebaute, statische Language- und NanoCell-Mods haben Manifest-/Kataloggrenzen; ein Deaktivierungs-Integrationstest baut beide Mods aus. `src/mod/catalog.cpp:3-49`, `tests/integration/test_mod_build.py:20-47`.
- **Belegt (historisch):** Core 0.102 dokumentiert 121/121 erfolgreiche CTests am 26.09.2026. Das frühere Stichtagsaudit dokumentiert für einen anderen Stand am 03.10.2026 130/130 CTests und eine bestandene CLI-Dokumentprüfung. `docs/implementation/183-local-file-adapter-usage.md:19-27`, `docs/audit/2026-10-03-repository-analysis-and-next-steps.md:15-20`.

### Teilweise umgesetzt oder fehleranfällig

- **Belegt:** Der aktuelle CMake-Quellbaum registriert 138 CTests, während die neuesten sichtbaren historischen Nachweise 121 bzw. 130 Tests zählen. Zwischen dem im älteren Audit genannten Commit und `HEAD` liegen unter anderem die statische Mod-Runtime sowie Language- und NanoCell-Änderungen. Daraus folgt kein Fehler, aber fehlende frische Verifikation für den heutigen Stand. `CMakeLists.txt:130-516`, Git-Historie `91530de..fa158de`.
- **Belegt:** Die offizielle Wrapper-Datei ist im aktuellen Working Tree nicht ausführbar, obwohl Dokumentation und Testregistrierung sie direkt ausführen. `git status --porcelain=v2` (Modus `100755 → 100644`), `README.md:22-24`, `CMakeLists.txt:174-177`, `tests/integration/test_with_host_deps.py:25-35`.
- **Belegt:** Sitzung, Gate und Transfer haben unterschiedliche Kontrollpunktreihenfolgen und öffentliche Fehlerformen. Das ist dokumentierter Bestand, keine bestätigte Inkonsistenz. `docs/contracts/06-control-and-publication.md:26-56`.
- **Belegt:** Der Experience-Store hängt weiterhin vom Anwendungsaudit ab; vollständige Bibliotheksentkopplung ist nicht erreicht. `docs/architecture/03-blueprint-consolidation.md:41,50-51`.

### Fehlend, ungeklärt oder nicht verifizierbar

- **Belegt:** Cockpit, HTTP-Server und Browser-Oberfläche fehlen im gegenwärtigen Quellbaum; `BRUCE_BUILD_MOD_COCKPIT`, `mods/cockpit/` und ein `cockpit`-CLI-Zweig sind nicht Teil der aktuellen CMake-/CLI-Quellen. `CMakeLists.txt:9-10,74-96`, `apps/bruce_cli.cpp:406-415,856-865`, Cockpit-Design `:3-7`.
- **Belegt:** Freie Sprachschnittstelle, NanoCell in `ApplicationTask`, Phase-DAG-, GPU- und allgemeine parallele Ausführung sind nicht implementiert. `docs/architecture/03-blueprint-consolidation.md:7,47-51,78`.
- **Belegt:** Ablehnungsjournal, Regelmigration und Journalrotation fehlen im vereinbarten lokalen Regelmodell. `docs/architecture/03-blueprint-consolidation.md:53-62`.
- **Offen:** Ob Cockpit tatsächlich das nächste Produktpaket ist. Dafür sprechen der Branch-Bezeichner und zwei gestagte Dokumente, die maßgebliche Architektur führt es aber weiterhin als separates Konzeptpaket. `docs/architecture/03-blueprint-consolidation.md:78`, Cockpit-Design `:3-18`.

## 5. Befunde und Risiken

### F-01: Lokaler Prüf-Wrappers ist nicht ausführbar

- **Einstufung:** P0, bestätigt.
- **Beobachtung:** Nur das Ausführungsbit von `scripts/with-host-deps.sh` weicht von `HEAD` ab (`100755 → 100644`); der Dateiinhalt ist unverändert.
- **Beleg:** Git-Diff-Statistik und `git diff --summary`; direkte Aufrufe stehen in `README.md:22-24` und `AGENTS.md:47-49`. Der CTest `with_host_deps_contract` startet das Skript als Programm (`tests/integration/test_with_host_deps.py:25-35`).
- **Auswirkung:** Die dokumentierten Aufrufe und der genannte Test können mit „Permission denied“ abbrechen. Ein frischer Baseline-Lauf wäre damit nicht aussagekräftig.
- **Empfohlene Reaktion:** Vor jedem Build mit dem Eigentümer klären, ob der Moduswechsel beabsichtigt ist. Falls nicht: ausschließlich den Modus auf den Indexstand zurücksetzen; erst danach den Wrapper-Vertrag und die vollständige Suite prüfen.

### F-02: Kein aktueller Ausführungsnachweis nach statischer Mod-Umstellung

- **Einstufung:** P0, bestätigt.
- **Beobachtung:** Die historischen Nachweise sind wertvoll, beziehen sich aber auf frühere Stände. Der aktuelle Checkout enthält danach neue Mod-Runtime-, Language- und NanoCell-Commits und 138 registrierte Tests.
- **Beleg:** `docs/implementation/183-local-file-adapter-usage.md:19-27`, historisches Audit `docs/audit/2026-10-03-repository-analysis-and-next-steps.md:15-20`; aktuelle Git-Historie und `CMakeLists.txt:130-516`.
- **Auswirkung:** Weder die aktuelle Mod-Kombination noch die vollständige CI-Kette dürfen als bestanden ausgegeben werden.
- **Empfohlene Reaktion:** Nach F-01 einen frischen, lokalen Build in einem temporären Verzeichnis, den gesamten CTest-Lauf und die CLI-Dokumentationsprüfung in einer temporären nativen Kopie ausführen. Kein Netzwerk, keine Dependency-Installation und keine Änderungen am Checkout.

### F-03: Cockpit-Vorhaben ist konkret geplant, aber noch nicht konsolidiert freigegeben

- **Einstufung:** P1, bestätigt.
- **Beobachtung:** Die gestagten Cockpit-Dateien definieren klare lokale Sicherheits- und Prozessgrenzen, sind aber selbst als „DOKUMENTIERT / KONZEPT“ markiert. Das Design fordert vor Umsetzung die Auswahl genau einer gepinnten HTTP-Abhängigkeit (`:228-234`); der Plan nennt bereits `cpp-httplib` v0.59.0 (`:16-18,114-120`).
- **Beleg:** `docs/superpowers/specs/2026-10-04-cockpit-v1-design.md:3-18,99-105,228-240`; `docs/superpowers/plans/2026-10-04-cockpit-v1.md:16-18,114-129`; CMake-/CLI-Bestand aus Abschnitt 4.
- **Auswirkung:** Eine Implementierung könnte sonst eine externe Quelle, Lizenz- und Offline-Entscheidung vorwegnehmen oder die Dokumente gegensätzlich lassen.
- **Empfohlene Reaktion:** Zuerst die Auswahl verbindlich entscheiden und die beiden Dokumente konsistent machen: konkrete Quelle/Commit/SHA-256/Lizenz, kein Buildzeit-Netzwerk, keine dynamischen Plugins, CMake-Deaktivierungsfall. Erst dann den statischen Cockpit-Slice beginnen.

### F-04: Bestehende Steuerungsverträge sind für jede neue UI kompatibilitätskritisch

- **Einstufung:** P1, bestätigt.
- **Beobachtung:** Session, Gate und Transfer unterscheiden sich absichtlich bei Einheit, Reihenfolge von Signal/Budget und Fehlerhülle. Dokumentierte Abläufe schließen nicht alle Verwaltungswege ein.
- **Beleg:** `docs/contracts/06-control-and-publication.md:14-56`; `apps/bruce_cli.cpp:196-251,866-871`.
- **Auswirkung:** Ein Cockpit darf Status, Abbruch, Replay oder Gate nicht in einen neuen fachlichen Zustand umdeuten oder verdeckte Anschlussläufe auslösen.
- **Empfohlene Reaktion:** Cockpit ausschließlich als separaten, explizit bestätigten Kindprozess-Client der bestehenden CLI implementieren. Roh-stdout/stderr, argv und originäre Exit-/JSON-Ergebnisse müssen maßgeblich bleiben.

### F-05: Lokales Sicherheitsmodell ist keine Mehrbenutzer- oder Identitätsgrenze

- **Einstufung:** P2, bestätigt innerhalb des definierten lokalen Scopes.
- **Beobachtung:** Die Regelbestätigung ist eine Inhaltsbindung, keine Authentisierung; die Dateilogik ist als lokaler Linux-Adapter formuliert. Das Cockpit-Design begrenzt einen künftigen Dienst deshalb auf `127.0.0.1` und verbietet fremde Programme, Shell-Syntax sowie externe Schreibziele.
- **Beleg:** `include/bruce/io/local_files.hpp:5-19`, `docs/architecture/03-blueprint-consolidation.md:53-62`, Cockpit-Design `:20-48,134-172`.
- **Auswirkung:** Loopback reduziert Netzexposition, ersetzt aber keine Isolation gegenüber anderen Prozessen desselben Benutzerkontos. Das ist keine Schwäche gegenüber dem aktuellen lokalen Ziel, muss aber bei jeder Scope-Erweiterung neu entschieden werden.
- **Empfohlene Reaktion:** Für Cockpit V1 an Loopback, fest gebundenem Binary, tokenisiertem argv, Jobordner-Containment und sichtbarer Bestätigung festhalten; Authentisierung, LAN/TLS oder externe Dienste nicht stillschweigend hinzufügen.

### F-06: Regel-Governance bleibt ein getrenntes, noch offenes Paket

- **Einstufung:** P3, bestätigt.
- **Beobachtung:** Ablehnungen, Migration und Journalrotation fehlen; sie sind nicht Teil der Cockpit-Spezifikation.
- **Beleg:** `docs/architecture/03-blueprint-consolidation.md:53-62`, Cockpit-Design `:215-226`.
- **Auswirkung:** Eine Benutzeroberfläche darf diese Lücken nicht durch neue automatische Aktivierung verdecken.
- **Empfohlene Reaktion:** Erst nach dem gewählten nächsten Produktpaket einen eigenen ADR-/Vertrags- und Test-Slice planen.

## 6. Priorisierte Roadmap

### AP-01 — Lokalen Wrapper-Modus bereinigen und Baseline-Prüfpfad herstellen

- **Priorität / Aufwand:** P0 / klein.
- **Ziel und Nutzen:** Die dokumentierte, vorhandene Verifikationskette wieder ausführbar machen, ohne Produktsemantik zu ändern.
- **Konkrete Aufgaben:** Modusabweichung mit der verantwortlichen Person abgleichen; bei unbeabsichtigter Änderung ausschließlich das Ausführungsbit an `HEAD` angleichen; Arbeitsbaum danach auf weitere unbeabsichtigte Änderungen prüfen.
- **Betroffene Bereiche:** `scripts/with-host-deps.sh`; als Nachweise `README.md`, `AGENTS.md`, `CMakeLists.txt`, `tests/integration/test_with_host_deps.py`.
- **Abhängigkeiten und Risiken:** Die Korrektur darf weder Skriptinhalt noch Abhängigkeitsumgebung verändern. Bei absichtlichem Entzug des Ausführungsbits ist statt einer stillen Rücknahme die Dokumentation/CTest-Verdrahtung zu entscheiden.
- **Abnahmekriterien:** Der Dateimodus entspricht der bewusst getroffenen Entscheidung; der Wrapper lässt sich auf einem nativen Pfad als dokumentiertes Programm starten; keine fremden lokalen Änderungen werden berührt.
- **Geeignete Verifikation:** `with_host_deps_contract` im frischen Build sowie ein erneuter Git-Modusvergleich.

### AP-02 — Frische, isolierte Baseline des aktuellen `master` erzeugen

- **Priorität / Aufwand:** P0 / mittel.
- **Ziel und Nutzen:** Einen aktuellen Prüfbeleg für die 138 registrierten Tests, die beiden statischen Mods und die unveränderten Dokumentationsartefakte schaffen.
- **Konkrete Aufgaben:** In einem neuen temporären Build-Verzeichnis von einem nativen Projektpfad konfigurieren und bauen; vollständiges CTest ausführen; anschließend die CLI-Dokumentationsprüfung ausschließlich in einer temporären Kopie laufen lassen und deren drei generierten Artefakte gegen den Checkout vergleichen. Toolchain- und Testversionen im Nachweis festhalten.
- **Betroffene Bereiche:** CMake-/CTest-Konfiguration in `CMakeLists.txt`, Wrapper, `docs/cli/verify_examples.py`, `.github/workflows/ci.yml`; keine Produktdatei ist vorgesehen.
- **Abhängigkeiten und Risiken:** AP-01. Die vorhandenen Systemabhängigkeiten müssen verfügbar sein; bei fehlenden Abhängigkeiten oder Buildfehlern keine Installation durchführen, sondern den Befund separat dokumentieren.
- **Abnahmekriterien:** Konfiguration und Build beenden erfolgreich; jeder registrierte CTest ist bestanden; Dokumentationsprüfung erzeugt in der temporären Kopie keine inhaltliche Abweichung; `git diff --check` bleibt sauber.
- **Geeignete Verifikation:** Die in `AGENTS.md:47-49` beschriebene CMake-/CTest-Kette sowie der CI-äquivalente temporäre Dokumentationsschritt aus `.github/workflows/ci.yml:33-48`.

### AP-03 — Cockpit-Entscheidung vor Implementierung schließen

- **Priorität / Aufwand:** P1 / klein bis mittel.
- **Ziel und Nutzen:** Einen widerspruchsfreien, offline-reproduzierbaren technischen Rahmen schaffen und den Cockpit-Scope verbindlich eingrenzen.
- **Konkrete Aufgaben:** Produktvorrang Cockpit bestätigen; HTTP-Komponente oder vorhandene Alternative einmalig wählen; Herkunft, Version/Commit, SHA-256 und Lizenz nachvollziehbar festlegen; CMake-Einbindung ohne `FetchContent`, Package-Manager oder Buildzeit-Netzwerk spezifizieren; Cockpit-Design und -Plan auf denselben Beschluss bringen.
- **Betroffene Bereiche:** Derzeitige Cockpit-Spezifikation und -Plan; bei positiver Entscheidung später vorläufig `DEPENDENCIES.md`, `THIRD_PARTY_NOTICES.md`, `CMakeLists.txt` und `third_party/`.
- **Abhängigkeiten und Risiken:** AP-02 liefert die aktuelle Basis. Die Auswahl darf keine dynamische Plugin-Laufzeit, Core-Rückabhängigkeit, TLS/LAN-Dienst oder unpinned Quelle einführen.
- **Abnahmekriterien:** Es existiert genau eine überprüfbare Transportentscheidung; Deaktivierungs-, Lizenz-, Offline- und Lieferkettenanforderungen sind testbar; nicht enthaltene Ziele bleiben ausdrücklich ausgeschlossen.
- **Geeignete Verifikation:** Dokument-/ADR-Review und ein späterer ON/OFF-CMake-Test mit deaktiviertem Cockpit-Target.

### AP-04 — Cockpit als kleinen statischen Mod-Slice beginnen

- **Priorität / Aufwand:** P1 / mittel.
- **Ziel und Nutzen:** Die optionale Cockpit-Grenze nach dem Muster von Language und NanoCell schaffen, ohne Core-Semantik oder bestehende CLI-Verträge zu verändern.
- **Konkrete Aufgaben:** `BRUCE_BUILD_MOD_COCKPIT` und ein statisches `bruce_mod_cockpit` ergänzen; Manifest/Katalog/CLI nur für `bruce cockpit [--port N]` verdrahten; alle aktivierten/deaktivierten Mod-Kombinationen testen. Danach den Jobvertrag implementieren: erlaubter Befehlskatalog, fester ausführbarer `bruce`-Pfad, tokenisiertes argv ohne Shell, eindeutige Jobordner unter `runs/cockpit/` und ausschließlich darunter liegende Schreibziele.
- **Betroffene Bereiche:** Vorläufig die in `docs/superpowers/plans/2026-10-04-cockpit-v1.md:64-81,85-204` genannten neuen Cockpit-Dateien sowie `CMakeLists.txt`, `src/mod/catalog.cpp`, `apps/bruce_cli.cpp` und Mod-Tests.
- **Abhängigkeiten und Risiken:** AP-03; der aktuelle Katalog verwendet eine Präprozessor-Matrix für zwei Mods (`src/mod/catalog.cpp:33-48`). Eine Erweiterung muss alle OFF-Kombinationen erhalten. Cockpit darf weder Regeln/Experience außerhalb seines Jobordners ändern noch eigene wissenschaftliche Ergebnisse erzeugen.
- **Abnahmekriterien:** Ausgeschaltet existieren weder Target, Manifest noch Befehl; eingeschaltet ist nur das Manifest und die begrenzte CLI-Schnittstelle sichtbar; ungültige, Shell-artige, fremde oder ausbrechende Aufträge erzeugen weder Kindprozess noch Datei.
- **Geeignete Verifikation:** Neue C++-Vertragstests plus Python-Integration für Mod ON/OFF und negative Pfad-/argv-Fälle.

### AP-05 — Lokalen Prozess-, Service- und UI-Vertikalschnitt absichern

- **Priorität / Aufwand:** P2 / groß.
- **Ziel und Nutzen:** Vorhandene CLI-Läufe lokal sichtbar und kontrolliert starten, ohne eine zweite Fachlaufzeit zu schaffen.
- **Konkrete Aufgaben:** POSIX-Kindprozessadapter mit getrennten Logs, Rückkehrcode und explizitem SIGINT; persistenter Jobstatus mit ehrlichem `UNRESOLVED_AFTER_SERVER_EXIT`; ausschließlich an `127.0.0.1` gebundener HTTP-Server; statische Dashboard-Assets; sichtbare argv-Vorschau und -Bestätigung; Warteschlange mit Standardgrenze 1; explizite, separate Inspect-/Assess-/Replay-/Gate-Jobs; externe `.bruce`-Eingaben nur lesend behandeln.
- **Betroffene Bereiche:** Vorläufig `mods/cockpit/src/cockpit_process.*`, `cockpit_jobs.*`, `cockpit_server.cpp`, `cockpit_assets.cpp` und `tests/mods/cockpit/` gemäß Cockpit-Plan `:206-361`.
- **Abhängigkeiten und Risiken:** AP-04. Portkollisionen, Pfadtraversal, Shell-Injektion, falsche Jobzustände und automatische Folgeaufrufe sind zentrale Fehlerfälle. Keine Browser-Autostarts, CORS-Freigaben, LAN-Bindung oder Prozesswiederanbindung nach Dienstabbruch.
- **Abnahmekriterien:** Der Dienst bindet ausschließlich an Loopback und gibt die tatsächliche URL aus; ein Job startet exakt den bestätigten argv-Vektor des gebundenen Binaries; Abbruch, Fehlerrückgabe, Queue und Browser-Refresh bilden ausschließlich beobachtete technische Zustände ab; externe Fixtures bleiben bytegleich.
- **Geeignete Verifikation:** Deterministische Prozessfixture, feste Portkollision, HTTP-Integrationstests und Hash-Vergleiche vor/nach Inspect, Assess, Replay und Gate.

### AP-06 — Fakten dokumentieren und erst danach weitere Architekturpakete wählen

- **Priorität / Aufwand:** P2 für Cockpit-Dokumentation; P3 für Governance, Phase-DAG, GPU und weitere Konzepte.
- **Ziel und Nutzen:** Nur nach nachgewiesener Umsetzung den Bestand dokumentieren und spätere, fachlich unabhängige Vorhaben nicht vermischen.
- **Konkrete Aufgaben:** Nach einem bestandenen Cockpit-Vertikalschnitt CLI-/Architektur-/Abhängigkeitsdokumente aktualisieren und die drei generierten CLI-Dateien nur über den vorhandenen Verifizierer in einer temporären Kopie erneuern. Anschließend Regel-Governance, NanoCell-Integration, freie Sprache, Phase-DAG und GPU jeweils als eigenes Vertrags-/ADR-Paket priorisieren.
- **Betroffene Bereiche:** Vorläufig `README.md`, `AGENTS.md`, `docs/architecture/03-blueprint-consolidation.md`, `docs/cli/`, `DEPENDENCIES.md`, `THIRD_PARTY_NOTICES.md`; für spätere Pakete erst nach Auswahl.
- **Abhängigkeiten und Risiken:** AP-05. Dokumentation darf Cockpit-Jobmetadaten nicht als mathematische Evidenz darstellen. Regel-Governance darf keine automatische Aktivierung einführen.
- **Abnahmekriterien:** Jede neue Aussage ist durch aktuellen Test- und Buildnachweis gedeckt; generierte Dateien stammen ausschließlich aus dem Verifizierer; weiterführende Konzepte besitzen vor Codearbeit einen eigenen Scope, Vertrag und negative Tests.
- **Geeignete Verifikation:** Vollständiger CTest, temporäre CLI-Dokumentationsprüfung, lokaler Linkcheck und anschließender Git-Status ohne versehentliche Lauf-/Build-/Log-Artefakte.

## 7. Empfohlene Reihenfolge

1. **AP-01:** Ausführungsbit des Wrappers klären. Ohne diesen Schritt kann selbst der vorhandene Wrapper-Vertrag scheitern.
2. **AP-02:** Den aktuellen `master` isoliert verifizieren. Erst dieser Nachweis trennt eine lokale Modusabweichung von echten Build-, Mod- oder Regressionsproblemen.
3. **AP-03:** Den Cockpit-Prioritäts- und HTTP-Abhängigkeitsbeschluss konsolidieren. Er ist Voraussetzung für eine offline-reproduzierbare Implementierung.
4. **AP-04:** Zuerst statische Mod-Grenze und sicheren Jobvertrag liefern; das schafft eine unabhängig prüfbare Sicherheitsgrenze ohne Server- oder UI-Komplexität.
5. **AP-05 und AP-06:** Erst anschließend Kindprozesse, Loopback-Service, UI, explizite Folgeläufe und die abschließende Dokumentations-/Releaseprüfung umsetzen.

Die Reihenfolge folgt einer einfachen Abhängigkeit: reproduzierbare Ausgangsbasis → verbindlicher Scope/Dependency-Entscheid → minimaler optionaler Mod → lokaler Prozess-/UI-Slice → belegte Dokumentation. Governance, Phase-DAG und GPU bleiben davon getrennt.

## 8. Offene Fragen und Annahmen

1. **Ist Cockpit das bewusst priorisierte nächste Produktpaket?** Die gestagten Dateien und die Branch-Dekoration legen dies nahe, die maßgebliche Architektur erklärt Cockpit jedoch weiterhin zum separaten Konzept. Die Antwort wird vor AP-03 benötigt.
2. **Ist `cpp-httplib` v0.59.0 die freigegebene, gepinnte HTTP-Komponente?** Der Plan nimmt dies vorweg, das Design hält die Auswahl noch offen. Die Antwort wird vor jeder Drittanbieterübernahme benötigt.
3. **War der Entzug des Ausführungsbits bei `scripts/with-host-deps.sh` beabsichtigt?** Die Antwort ist vor AP-01 erforderlich; sie entscheidet, ob der Modus wiederhergestellt oder die vorgesehene Prüfkette angepasst wird.
4. **Steht die aktuelle native Buildumgebung mit CMake, Ninja, Compiler, GMP/GMPXX, OpenSSL und Python noch bereit?** Das beeinflusst ausschließlich Ort und Ergebnis von AP-02, nicht die Produktarchitektur.
5. **Welcher Governance-Umfang ist für Ablehnungsjournal, Migration und Rotation zulässig?** Die Antwort wird erst vor dem eigenständigen Governance-Paket benötigt; sie blockiert Cockpit V1 nicht.

## 9. Verifikationsprotokoll

### Tatsächlich ausgeführt

Alle folgenden Befehle waren lesend; sie führten weder Build noch Tests noch Netzwerk-/Installations-/Deploy-Aktionen aus. `rg` wurde zuerst versucht, ist in dieser Umgebung jedoch nicht installiert.

| Befehl / Prüfung | Ergebnis |
| --- | --- |
| `pwd`; Lektüre der verpflichtenden Agenten- und Planungsleitlinien | Arbeitsordner festgestellt; nur Verfahrensanweisungen gelesen, keine Produktdaten außerhalb des Arbeitsbereichs. |
| `git status --short --branch`; `git log --oneline -5`; `git rev-parse --show-toplevel`; `git branch --show-current` im zunächst geöffneten Ordner | Kein gültiger Git-Root im Oberordner; löste ausschließlich die Root-Bestimmung aus. |
| `find . -maxdepth 3 -type d -print`; Inventarversuch mit `rg --files` im geöffneten Ordner | Zwei untergeordnete Git-Repositories gefunden; `rg` nicht verfügbar. |
| `ls -lad . .git .git/HEAD .git/config`; `git rev-parse --show-toplevel`; `git -C Bruce_raw ...`; `git -C Bruce_raw-offline-debian ...` | Gültigen Root `Bruce_raw` sowie seine Ausgangslage bestimmt; beim zweiten Kandidaten nur Git-Status/-Log zur Scope-Bestimmung, keine Datei-/Codeanalyse. |
| `find`, `git ls-files`, `git status`, `git diff --stat` und `git diff --cached --stat` im Root | Projektstruktur, Dokumentationsordner und die drei lokalen Änderungen ermittelt. |
| `sed`/`nl -ba` auf `AGENTS.md`, `README.md`, `DEPENDENCIES.md`, `CMakeLists.txt`, `docs/README.md`, Blueprint, Steuerungsvertrag, Nutzungseinträge, CI, CMake, CLI-, Header- und Mod-Dateien | Die im Bericht zitierten Modulgrenzen, Build-/Testverdrahtung und Betriebsgrenzen direkt geprüft. Große Sammelausgaben wurden gekürzt; tragende Dokumente wurden gezielt erneut gelesen. |
| `git log`, `git show --stat`, `git grep`, `grep`, `find`, `wc -l` | Jüngste Mod-Commits, Cockpit-Abgrenzung, TODO/FIXME-Status, Wrapper-Verweise, historische Nachweise und 138 aktuelle CTest-Registrierungen geprüft. |
| `git diff --check`; `git diff --cached --check`; `git status --porcelain=v2 --branch` | Keine Whitespace-Diagnosen; zwei gestagte Cockpit-Dokumente und der reine Moduswechsel des Wrappers bestätigt. |
| `git diff --no-index --check /dev/null docs/audit/2026-10-06-repository-analysis-and-next-steps.md`; `git diff --check`; `git diff --cached --check`; `git status --short --branch` | Abschlussprüfung des neu angelegten Berichts und Vergleich mit der Ausgangslage; ausgeführt nach dem letzten Speichern. |

### Bewusst nicht ausgeführt

- Kein CMake-Configure, Build, CTest, CLI-Manuallauf, Installer, CPack, Benchmark oder Dokumentationsgenerator. Die später empfohlenen Prüfungen könnten lokale temporäre Dateien anlegen; eine aktuelle Ausführung wäre darüber hinaus vor AP-01 nicht belastbar.
- Keine Dependency-Installation, kein Netz-/Remotezugriff, kein Push/Commit/Deploy und keine Datenmigration.
- Keine Produktcode-, Konfigurations-, Dokumentations- oder Berechtigungsänderung außer dieser Analyse-Datei.

### Gegenprüfung des Berichts

- Wesentliche Empfehlungen sind auf konkrete aktuelle Befunde zurückgeführt; historische Testergebnisse sind als historisch gekennzeichnet.
- Implementierung, Konzept, abgeleitete Priorisierung und offene Entscheidungen sind getrennt ausgewiesen.
- Die Roadmap enthält pro Paket Ziel, Aufgaben, Bereiche, Abhängigkeiten, Risiken, Abnahmekriterien und Verifikation.
- Der Bericht benennt die Scope-Bestimmung transparent und analysiert danach ausschließlich den validen Repository-Root `Bruce_raw`.
