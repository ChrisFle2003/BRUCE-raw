# Repository-Analyse und nächste Schritte

> **Historischer Stichtags-Audit.** Die Ausgangsanalyse bezieht sich auf den
> unten genannten Commit `91530de`; spätere Nachträge sind jeweils ausdrücklich
> datiert. Für den heutigen Checkout gelten der [Dokumentationsindex](../README.md),
> der [aktuelle Blueprint](../architecture/03-blueprint-consolidation.md) und
> der Git-Verlauf. Dieses Dokument ist kein Ersatz für eine erneute Prüfung.

**Analysezeitpunkt:** 03.10.2026

**Analysierter Stand:** `master` bei `91530de` (`chore: ignore local worktrees`)

**Arbeitsgrenze:** ausschließlich dieser Repository-Checkout einschließlich seiner Git-Metadaten. Es wurden weder Produktcode noch Konfiguration noch bestehende Dokumente verändert. Dieser Bericht ist die einzige neu angelegte Datei.

## 1. Kurzüberblick

- **Projektzweck — Belegt:** BRUCE ist ein lokaler C++20-Forschungskern mit CLI. Er untersucht begrenzte mathematische Formel- und Kompatibilitätsräume, versieht konkrete Läufe mit Trace/Replay und trennt Kandidaten, Stichprobenbestätigung und begrenzte Beweisbehauptungen. Belege: `README.md:5`, `README.md:211-218`, `docs/architecture/03-blueprint-consolidation.md:5-10`.
- **Aktueller Reifegrad — Abgeleitet:** Es handelt sich um einen funktional breit ausgebauten, aber bewusst begrenzten Forschungskern, nicht um die vollständige Zielarchitektur. Reale CMake-Targets, Unit- und Integrations-Testregistrierungen sowie CI sind vorhanden; freie Sprache, Cockpit, allgemeine Plugin-/Phase-DAG- und GPU-Runtime fehlen ausdrücklich. Belege: `CMakeLists.txt:1-89`, `.github/workflows/ci.yml:10-56`, `docs/architecture/03-blueprint-consolidation.md:5-9,71-77`.
- **Frischer Verifikationsnachweis — Belegt (Nachtrag 03.10.2026):** Eine isolierte Release-Konfiguration des aktuellen `master` wurde mit CMake 3.31.6, GCC 14.2.0, GMP/GMPXX 6.3.0, OpenSSL 3.5.7 und Python 3.13.5 erfolgreich gebaut. CTest endete mit **130/130 bestanden**; darunter `evidence_chain_contracts` und `evidence_chain_workflows`. Die CI-äquivalente CLI-Dokumentationsprüfung endete ebenfalls erfolgreich (135 Beispiele, 77 dokumentierte Optionen, 470 Diagnosen, Links OK, drei erzeugte Referenzdateien bytegleich). Dies verifiziert den benannten Quellstand, keine Gesamtfreigabe der Zielarchitektur.
- **Branch-Abnahme — Belegt (Nachtrag 03.10.2026):** `feature/taskspec-token-slice` wurde aus einer temporären `git archive`-Kopie frisch gebaut und mit **133/133** CTest-Prüfungen ausgeführt. `task_spec_contracts`, `tokenized_task_run_contracts` und `task_spec_workflows` bestanden; die Branch-CLI-Dokumentationsprüfung meldete 140 Beispiele, 79 Optionen, 527 Diagnosen und gültige Links. Ein prunabler Worktree verhinderte keinen Archivbuild.
- **Wichtigster nächster Schritt — Belegt:** Den Widerspruch zwischen ADR-015/Core-0.102-Nutzungseintrag und dem aktuellen Code, Vertrag sowie CLI-Handbuch bereinigen. `experience revoke/replace` besitzen bereits einen opt-in-Transferpfad; die beiden älteren Dokumente behaupten noch das Gegenteil. Belege: `apps/bruce_cli.cpp:639-659`, `docs/contracts/06-control-and-publication.md:20-21`, `docs/cli/commands.md:261-275`, `docs/adr/015-control-contract-compatibility.md:18-22,60-63`, `docs/implementation/184-control-contract-matrix-usage.md:60-64`.
- **Größte erkennbare Risiken:**
  1. **P0 — Belegt:** Sitzungs-, Gate- und Transfersteuerung haben absichtlich unterschiedliche Fehler- und Budgetsemantik. Eine unkontrollierte Vereinheitlichung wäre eine sichtbare Vertragsänderung.
  2. **P1 — Belegt:** Lokale Regelverwaltung hat noch kein Ablehnungsjournal, keine Migration und keine Journalrotation.
  3. **P1 — Belegt:** Der tokenisierte Task-Spec-Branch ist ausführbar und testgrün, akzeptiert aber kanonische V1-Artefakte mit beliebiger Tabellenbelegung bzw. beliebiger Token-ID-zu-Rollen-Zuordnung. Das steht im Konflikt mit seiner eigenen Aussage fester, semantisch gebundener Tokens und ist vor einem Merge zu entscheiden.

## 2. Geprüfte Grundlagen und Grenzen

### Git-Ausgangslage

- **Belegt:** Repository-Root und Arbeitsverzeichnis sind identisch: `/run/user/1000/gvfs/sftp:host=192.168.15.15/home/chris/Desktop/Bruce_raw`.
- **Belegt:** Aktueller Branch ist `master` auf `91530de`; der vorherige Baseline-Commit ist `14d336b` (`chore: establish core 0.102 baseline`). Die von `master` erreichbare Historie enthält zwei Commits. Eine weitere lokale Worktree-Branch `feature/taskspec-token-slice` liegt sieben Commits vor `master` und ist nicht gemergt.
- **Belegt:** Zu Beginn lagen zahlreiche ungetrackte Artefakte vor, darunter lokale Arbeits-/Identitätsdokumente, `.bruce`-Dateien, Benchmarkdaten und `bruce_semantic_index/`. Sie wurden weder gelesen noch geändert, soweit sie nicht als bewusst sichtbarer Repository-Bestand in die Strukturaufnahme fielen. Der Bericht vermeidet Inhalts- und Secret-Ausgaben.
- **Belegt:** Die Remote-Auflistung ergab keine Ausgabe; daraus wird keine Aussage über Veröffentlichungs- oder Upstream-Status abgeleitet.

### Geprüfte Bereiche

| Bereich | Geprüfte Belege | Ergebnisgrenze |
| --- | --- | --- |
| Projektzweck, Grenzen und Bedienung | `README.md`, `docs/README.md`, CLI-/Architektur- und Vertragsdokumente | Dokumentation beschreibt den vorgesehenen und teilweise abgenommenen Umfang; sie ersetzt keinen Test des aktuellen Quellstands. |
| Build, Abhängigkeiten und CI | `CMakeLists.txt`, `DEPENDENCIES.md`, `.github/workflows/ci.yml` | Targets und Prüfketten sind im Quelltext bestätigt; kein Build wurde ausgeführt. |
| Kernarchitektur und Quellgrenzen | `include/bruce/application/application_task.hpp`, `apps/bruce_cli.cpp`, `src/evidence/chain.cpp`, Quellbaumkarte | Zentraler Laufweg und die isolierte Evidence-Chain sind direkt im Code nachvollzogen; nicht jede mathematische Strategie wurde einzeln geprüft. |
| Reifegrad, Governance und offene Pakete | Architektur, Audit, ADR-012/015, Roadmap, Teststrategie | Aktueller Audit und konsolidierte Architektur sind als maßgebliche Bestandsdokumente behandelt; ältere Roadmap-Abschnitte sind historischer Kontext. |
| Testbarkeit | CMake-Testregistrierung, CI-Konfiguration, bestehender Audit, frischer temporärer Build | Der aktuelle `master` wurde isoliert gebaut; CTest endete mit 130/130 bestanden. Die CLI-Dokumentationsprüfung endete erfolgreich. Der alte Buildordner auf GVFS bleibt dennoch nicht beschreibbar. |

### Nicht geprüft

- **Offen:** Alle mathematischen Algorithmen wurden nicht vollständig codeweise auditiert; die Aussagen beziehen sich nur auf die sichtbaren Modulgrenzen, registrierten Tests und Dokumentation.
- **Offen:** Es gibt keine neu erhobene Aussage zu Laufzeit, Speicher, Release-Artefakten, Installation oder Remote-Bereitstellung. Das wäre erst nach einem frischen Build in einer beschreibbaren, abhängigkeitserfüllten Umgebung belastbar.
- **Offen:** Ungetrackte Arbeitsartefakte, fremde Worktrees und Inhalte außerhalb dieses Repository-Root waren nicht Gegenstand der Analyse.

## 3. Repository- und Architekturüberblick

### Relevante Komponenten

| Komponente | Aufgabe und Beleg | Grenze |
| --- | --- | --- |
| C++-Kern `bruce_core` | CMake bündelt NanoCell, Anwendung, Mathematik, Evidenz, Experience, Sprache und lokale Dateiabläufe in eine C++20-Bibliothek; CLI-Ziel ist `bruce`. `CMakeLists.txt:1-89` | Keine dynamische Plugin-Registrierung belegt. |
| Gemeinsame Anwendungsaufträge | `ApplicationTask` und `ApplicationRun` sind feste `std::variant`-Mengen mit acht Varianten; Ausführung, Packen, Replay und Bewertung sind explizite Funktionen. `include/bruce/application/application_task.hpp:17-66` | Kein allgemeiner Task-DAG; in-memory-Bewertung prüft keine Herkunft (`:29-33`). |
| Mathematische Forschung | Quellen liegen unter `src/math/` und `src/application/*research.cpp`; README dokumentiert affine, Potenz-, Polynom-, rationale und Ausdrucks-/Portfolio-Wege. `CMakeLists.txt:37-56`, `README.md:236-253` | Kandidaten-/Stichprobenstatus ist kein allgemeiner Beweis. `docs/architecture/03-blueprint-consolidation.md:7-10` |
| Evidenz, Replay und Gates | Native Bundles, Replay, Bewertung und Gateprüfung sind getrennte Schichten. `src/evidence/bundle.cpp`, `src/evidence/chain.cpp`, `src/application/application_replay.cpp`, `src/application/application_gates.cpp`; Übersicht `docs/architecture/03-blueprint-consolidation.md:34-40` | Gatebericht ist kein Autorisierungstoken; keine automatische Regelpromotion. |
| Regel- und Experience-Wege | Regelvorschläge/Historie, lokaler Regelspeicher, Experience-Store und getrennte Transfers sind vorhanden. `docs/architecture/03-blueprint-consolidation.md:38-46,52-61` | Keine Identitätsgrenze gegen Prozesse desselben Benutzerkontos; Ablehnungsjournal, Migration und Rotation fehlen. |
| Begrenzte Sprache und NanoCell | Deterministischer Sprachpfad für zwei feste Muster sowie isolierter NanoCell-V1-CLI-Pfad. `README.md:77-77`, `docs/architecture/03-blueprint-consolidation.md:47-48` | Keine freie Sprache, keine Einbindung NanoCell → `ApplicationTask`. |
| Python-Semantikindex | Eigenständiges Python-3.11+-Paket ohne Laufzeitbindung an `bruce`; es validiert/indiziert lokale CLI-Dokumentation. `pyproject.toml:1-15`, `README.md:9-73` | Er führt `bruce` nicht aus und liefert keine Ausführungsvalidierung. `README.md:11-14,41-42` |

### Sichtbarer Kontroll- und Datenfluss

**Belegt:** Der maßgebliche dokumentierte Laufweg lautet: CLI → `ApplicationTask` → fachlicher Ausführer → `ApplicationRun` → natives Bundle → Replay → Ergebnisbewertung/Gates; optional führen getrennte Transferwege zu Experience bzw. Z-Auswahl. `docs/architecture/03-blueprint-consolidation.md:11-28`. Der Code bestätigt den festen Auftrags-/Laufvertrag und die getrennten Pack-/Replay-/Bewertungsfunktionen (`include/bruce/application/application_task.hpp:17-66`).

**Belegt:** Die Exact-Evidence-Chain ist davon ein spezieller, isolierter CLI-Pfad: `bruce evidence create|replay` erzwingt `affine` plus `enumerate` und `bigint`/`rational`, bindet rohe Trainings-/Holdout-CSV-Bytes an ein Exact-Research-Bundle und prüft beim Replay Digest, Policyfelder, Eingabebindung und Resultat-Rekonstruktion. `apps/bruce_cli.cpp:436-469`, `src/evidence/chain.cpp:7-16,26-45,95-155`. Ihre deklarierte Unabhängigkeit ist nur `declared-disjoint-v1`, keine externe Provenienzprüfung (`src/evidence/chain.cpp:54-59`, Audit `docs/audit/2026-10-01-core-0102-ist-gegen-zielbild.md:53-54`).

## 4. Ist-Zustand

### Umgesetzt und belegt

- **Belegt:** C++20-Projektversion 0.102.0, verpflichtende GMP/GMPXX- und OpenSSL-Crypto-Abhängigkeiten sowie ein `bruce`-CLI-Target. `CMakeLists.txt:1-9,63-89`.
- **Belegt:** Breite Testregistrierung: C++-Vertragstests und Python-Integrationstests werden unter `BUILD_TESTING` registriert. Beispielhaft NanoCell, Sprache, Steuerungsverträge, Grid, Evidence-Chain und mathematische Familien. `CMakeLists.txt:90-155,200-223`.
- **Belegt:** CI konfiguriert einen frischen Release-Build, führt CTest aus, validiert generierte CLI-Dokumentation in einer temporären Kopie und verwendet einen Secret-Scan. `.github/workflows/ci.yml:10-56`.
- **Belegt:** Lokale Regeländerung verlangt im vereinbarten Modell konkrete menschliche Bestätigung; Regeln werden nicht allein durch Digests, Pläne oder Gateberichte aktiviert. `docs/architecture/03-blueprint-consolidation.md:52-61`, `docs/contracts/06-control-and-publication.md:5-12`.
- **Belegt:** Ein getrenntes, noch nicht gemergtes Branch enthält einen tokenisierten Task-Spec-Ausbau mit zwei neuen Unit- und einem Integrationstest; der frische Branch-Build bestand 133/133 Tests einschließlich dieser drei Pfade und die zugehörige Dokumentationsprüfung. Der Worktree selbst ist prunable, der Git-Branch war aber aus dem Objektbestand vollständig reproduzierbar. Git-Diff `master...feature/taskspec-token-slice`.

### Teilweise umgesetzt oder fehleranfällig

- **Belegt (historisch):** Für den vorhandenen älteren Build dokumentiert der Audit 128/128 erfolgreiche Tests, aber ausdrücklich **vor** der Exact-Evidence-Chain-Änderung; damals fehlten `pkg-config` und `gmpxx.h` für eine frische Konfiguration. `docs/audit/2026-10-01-core-0102-ist-gegen-zielbild.md:17-22`. **Nachtrag 03.10.2026:** Der aktuelle Checkout wurde inzwischen in einem frischen temporären Build erfolgreich gebaut und mit 130/130 Tests geprüft, einschließlich beider Evidence-Chain-Testpfade.
- **Belegt:** Die drei Steuerungsfamilien sind mit verschiedenen Budgeteinheiten, Kontrollpunktreihenfolgen und Fehlerhüllen absichtlich nicht vereinheitlicht. `docs/contracts/06-control-and-publication.md:14-56`. **Dokumentationskonflikt:** ADR-015 und `184-control-contract-matrix-usage.md` behaupten veraltet, `experience revoke/replace` hätten keinen opt-in Kontrollvertrag, während Code, CLI-Referenz, Handbuch und Tests diesen Vertrag belegen. **Auswirkung:** Ein Refactoring ist nicht der nächste Schritt; zuerst muss die maßgebliche Dokumentation auf den aktuellen, getesteten Bestand zurückgeführt werden.
- **Belegt:** Der Experience-Store ist noch von Anwendungsaudit-Typen abhängig; vollständige Bibliotheksentkopplung ist nicht erreicht. `docs/architecture/03-blueprint-consolidation.md:41,50`.
- **Belegt:** Das vorhandene `build/` kann in diesem GVFS-/SFTP-Checkout keine CTest-Logdatei unter `build/Testing/Temporary` anlegen. Der ausgeführte reine List-Aufruf meldete diesen Fehler vor der Ausführung von Tests. **Auswirkung:** Dieser Buildpfad ist nicht als aktuelle, beschreibbare Verifikationsumgebung verwendbar.

### Fehlend, ungeklärt oder nicht verifizierbar

- **Belegt:** Allgemeine Gate-/Countercheck-/Holdout-Runtime, Token-/Relationsmodell, Research Cockpit, freie Sprache, Plugin-Runtime, Phase-DAG und GPU-Ausführung sind als offen oder nicht als Produktrealität dokumentiert. `docs/audit/2026-10-01-core-0102-ist-gegen-zielbild.md:60-68`.
- **Belegt:** Ablehnungsjournal, Regelmigration und Journalrotation sind für den lokalen Regelbetrieb nicht implementiert. `docs/architecture/03-blueprint-consolidation.md:61`.
- **Offen:** Ob die Arbeitsumgebung inzwischen alle Build-Abhängigkeiten bereitstellt, wurde nicht geprüft; eine Dependency-Installation ist ausdrücklich nicht Teil dieser Analyse.
- **Offen:** Ob der tokenisierte Task-Spec-Branch fachlich freigegeben, vollständig getestet oder für Merge vorgesehen ist, lässt sich aus seiner Existenz allein nicht ableiten.

## 5. Befunde und Risiken

### F-01: Aktuelle Evidence-Chain ist frisch verifiziert

- **Einstufung:** P0, geschlossen für den aktuellen `master`.
- **Beobachtung:** `bruce evidence create|replay`, Implementation und zwei Testpfade sind im aktuellen Quellbaum vorhanden. `apps/bruce_cli.cpp:436-469`, `src/evidence/chain.cpp:95-155`, `CMakeLists.txt:218-223`.
- **Beleg:** Der frühere Audit grenzte die Chain als nur quellseitig implementiert ab. Der Nachtrag vom 03.10.2026 führt nun frische Konfiguration, erfolgreichen Release-Build und **130/130** bestandene CTest-Prüfungen auf, darunter `evidence_chain_contracts` und `evidence_chain_workflows`.
- **Auswirkung:** Für den geprüften Checkout besteht ein aktueller Ausführungsnachweis. Er belegt die registrierten Verträge, nicht eine allgemeine Evidenz-/Provenienztheorie oder die vollständige Zielarchitektur.
- **Empfohlene Reaktion:** Keine funktionale Erweiterung der Chain ohne eine Entscheidung aus AP-02 über ihren möglichen Integrationsvertrag.

### F-02: Steuerungsverträge sind kompatibilitätskritisch

- **Einstufung:** P0, bestätigt.
- **Beobachtung:** Session, Gate und Transfer haben abweichende Zählreihenfolgen und Fehlerhüllen; `INCOMPLETE` kann fachlich erfolgreich sein, während ein Steuerungsfehler Exit 2 auslöst. `docs/contracts/06-control-and-publication.md:7-12,34-56`.
- **Auswirkung:** Eine „Vereinheitlichung“ ohne expliziten Versions-/Migrationsvertrag kann bestehende Automationen und gespeicherte Aufrufe semantisch brechen.
- **Empfohlene Reaktion:** Zuerst Dokumentationswiderspruch für die bereits vorhandenen `revoke/replace`-Transferoptionen auflösen. Erst bei einem **anderen** neuen Verwaltungsweg ist eine neue Architekturentscheidung nötig.

### F-02a: ADR-015 und Nutzungsdokumentation sind gegenüber Code und CLI-Handbuch veraltet

- **Einstufung:** P0, bestätigt.
- **Beobachtung:** `apps/bruce_cli.cpp:639-659` verwendet bei `experience revoke|replace` mit Transferoptionen `TransferControl`, Transferprüfpunkte, kontrolliertes Lesen, `execute_experience`, exklusives Publizieren und die Hülle `controlled-transfer-v1`. `tests/integration/test_experience_replace.py:70-106` prüft Ergebnisgleichheit, Budgetgrenze, fehlende Veröffentlichung bei Abbruch und Schutz bestehender Zieldateien; dieser Test bestand im frischen 130/130-Lauf.
- **Widersprechende Dokumente:** ADR-015 beschreibt diese Wege in `docs/adr/015-control-contract-compatibility.md:18-22,60-63` noch als ohne opt-in-Vertrag. `docs/implementation/184-control-contract-matrix-usage.md:60-64` wiederholt die Aussage. Dagegen dokumentieren Vertrag und CLI-Handbuch die tatsächlichen Optionen explizit: `docs/contracts/06-control-and-publication.md:20-21`, `docs/cli/commands.md:261-275`, `docs/cli/experience-transfer.md:102-106`.
- **Auswirkung:** Leser könnten fälschlich eine fehlende Funktion planen oder den jetzigen Kontrollumfang falsch einschätzen. Dies ist ein Dokumentations-/Entscheidungsnachweisproblem, kein durch diesen Befund bestätigter Produktcodefehler.
- **Empfohlene Reaktion:** ADR-015 und den Nutzungseintrag auf den getesteten Core-0.102-Bestand aktualisieren; die Abgrenzung „ohne Transferoption bleibt nativer direkter Weg“ ausdrücklich beibehalten.

### F-03: Bestehender Buildordner ist keine sichere aktuelle Verifikationsbasis

- **Einstufung:** P0, bestätigt.
- **Beobachtung:** Der Audit nennt einen vor der Chain-Änderung geprüften Build und fehlende Entwicklungsabhängigkeiten für einen frischen Build. Zusätzlich scheiterte in dieser Analyse selbst `ctest -N` am Erzeugen von `build/Testing/Temporary/LastTest.log`.
- **Auswirkung:** Weder historische 128/128 noch eine Testliste belegen den aktuellen Checkout.
- **Empfohlene Reaktion:** Frisches, beschreibbares Build-Verzeichnis außerhalb des schützenswerten Arbeitsbaums verwenden und Ergebnis/Quellbindung dokumentieren.

### F-04: Regel-Governance bleibt lokal und unvollständig

- **Einstufung:** P1, bestätigt.
- **Beobachtung:** Menschliche Bestätigung und atomarer Regelspeicherwechsel sind umgesetzt, aber Ablehnungen, Migration und Journalrotation fehlen. `docs/architecture/03-blueprint-consolidation.md:52-61`.
- **Auswirkung:** Langfristige Nachvollziehbarkeit und Wartung des Regeljournals bleiben begrenzt; dies ist kein Hinweis auf fehlende aktuelle lokale Bestätigung.
- **Empfohlene Reaktion:** Als getrenntes Governance-Paket nach einer expliziten fachlichen Entscheidung behandeln, ohne autonome Aktivierung hinzuzufügen.

### F-05: Tokenisierter Task-Spec-Ausbau ist technisch geprüft, aber noch nicht merge-reif

- **Einstufung:** P1, bestätigt.
- **Beobachtung:** `feature/taskspec-token-slice` enthält sieben Commits und Änderungen an CLI, CMake, Token-/Anwendungsquellen, Tests und CLI-Dokumentation. Ein temporärer Branch-Build endete mit 133/133 Tests; alle drei neuen Task-Spec-Testpfade sowie die Branch-CLI-Dokumentationsprüfung bestanden.
- **Auswirkung:** Der aktuelle `master` darf die Funktion weiterhin nicht behaupten. Der Branch ist technisch ausführbar, führt aber einen neuen öffentlichen CLI-/Daten-/Replayvertrag ein und benötigt wegen F-05a/F-05b eine fachliche Mergeentscheidung.
- **Empfohlene Reaktion:** F-05a vor einem Merge schließen und F-05b explizit entscheiden; dann den Branch erneut frisch bauen, vollständig testen und erst danach eine Mergeentscheidung treffen.

### F-05a: V1 erzwingt keine feste semantische Token-Tabelle und Rollenbindung

- **Einstufung:** P1, bestätigt; Merge-Gate.
- **Beobachtung:** Die CLI erzeugt die drei erwarteten Einträge `Train`, `Holdout`, `Rational Portfolio` und bindet die drei Rollen an ihre jeweils erzeugte Token-ID (`apps/bruce_cli.cpp` im Branch: Zeilen 470-492). Die Validierung akzeptiert jedoch jede nichtleere Tabelle mit Version `v1`, prüft nur, ob eine Rollen-ID in `ordered_token_ids` vorkommt, und erzwingt weder die drei erwarteten Tabelleninhalte noch eine eindeutige, rollenspezifische Zuordnung (`src/token/task_spec.cpp` im Branch: Zeilen 83-99,112-153,125-139). Bei der Kompilierung werden die Daten über Rollennamen/Asset-Aliase gewählt; die Token-ID dient dort nicht als semantische Auswahl (`:244-261`).
- **Widerspruch:** Das Handbuch behauptet für V1 feste Token und explizite Rollenbindung an „diese IDs“ (`docs/cli/commands.md` im Branch: Zeilen 49-55). Ein äußerlich kanonisches, neu gehashtes V1-Artefakt kann derzeit andere Tokeninhalte oder eine abweichende ID-zu-Rollen-Zuordnung tragen und dennoch `inspect`/`execute` erreichen.
- **Auswirkung:** Der Digest schützt die Bytes gegen nachträgliche Änderung, autorisiert aber nicht die erwartete V1-Semantik. Das schwächt genau die beabsichtigte Aussage über tokengebundene Rollen, obwohl der daraus kompilierte Forschungsauftrag technisch weiterhin determiniert sein kann.
- **Empfohlene Reaktion:** Vor Merge entweder (a) V1 strikt auf genau die drei kanonischen Einträge, eindeutige Reihenfolge und feste Rollen-ID/Asset-Bindung beschränken und negative Tests ergänzen, oder (b) den Vertrag ausdrücklich als erweiterbare, vom Artefakterzeuger definierte Tabelle dokumentieren. Die vorliegende CLI-Dokumentation stützt Variante (a).

### F-05b: Alternatives-Modell ist über die CLI nicht auswählbar

- **Einstufung:** P2, bestätigt; Produkt-/Vertragsentscheidung.
- **Beobachtung:** Das Binärformat und der Renderer enthalten `alternatives` und `selected_id`; `compile_tokenized_task_spec` kann einen bereits ausgewählten Alternativsatz verwenden (`src/token/task_spec.cpp` im Branch: Zeilen 143-153,244-261). Die öffentliche CLI bietet aber nur `create|inspect|execute|replay`, keinen `task-spec select`-Befehl (`apps/bruce_cli.cpp` im Branch: Zeilen 453-511). Die Integrationstests erzeugen Alternativen durch manuelle Umformung eines Bundles und inspizieren nur eine bereits gesetzte Auswahl (`tests/integration/test_task_spec.py` im Branch: Zeilen 268-328).
- **Auswirkung:** Die Handbuchaussage „Alternativen müssen vor Ausführung ausgewählt sein“ ist für CLI-Nutzer nicht vollständig ausführbar, sofern ein alternatives Spec vorliegt. Der normale CLI-Erzeuger produziert derzeit keine Alternativen.
- **Empfohlene Reaktion:** Vor Merge festlegen: Alternativen aus V1 entfernen/als externes Formatfeld dokumentieren oder einen expliziten, exklusiv exportierenden Auswahlpfad samt Tests bereitstellen. Dies ist kein Grund, die getestete direkte V1-Route falsch als defekt zu bezeichnen.

### F-06: Zielbild und implementierter Kern sind bewusst verschieden

- **Einstufung:** P2, bestätigt.
- **Beobachtung:** Dokumentation benennt Cockpit, freie Sprache, NanoCell-Integration, Phase-DAG und GPU als eigene, offene Pakete. `docs/architecture/03-blueprint-consolidation.md:5-9,71-77`; Audit `:60-68`.
- **Auswirkung:** Eine Roadmap, die diese Elemente als „fast fertig“ oder einen Testbestand als Gesamtfreigabe interpretiert, wäre sachlich falsch.
- **Empfohlene Reaktion:** Nur ein ausgewähltes Zielpaket mit eigener ADR/Spezifikation, Vertrag und Abnahme beginnen; Suchoptimierung bleibt nachrangig.

## 6. Priorisierte Roadmap

### AP-01 — Aktuellen Core reproduzierbar verifizieren (erledigt)

- **Priorität / Aufwand:** P0 / mittel; **am 03.10.2026 abgeschlossen**.
- **Ziel und Nutzen:** Einen reproduzierbaren Ausführungsnachweis für den aktuellen `master` herstellen, insbesondere für die Exact-Evidence-Chain; historische Build-Ergebnisse sauber von aktuellen trennen.
- **Tatsächliches Ergebnis:** Frischer Release-Build unter `/tmp/bruce-ap01-build-Zr6a2h` erfolgreich; `ctest --output-on-failure --parallel 4` endete mit **130/130 bestanden** in 35,49 s. Die temporäre CI-äquivalente CLI-Dokumentationsprüfung meldete 135 Beispiele, 77 dokumentierte Optionen, 470 Diagnosen und Links OK; `source-index.md`, `examples.json` und `validation.md` waren bytegleich zum Checkout.
- **Konkrete Aufgaben:**
  1. In einer beschreibbaren lokalen Linux-Umgebung die vorhandenen Abhängigkeiten CMake >= 3.20, C++20-Compiler, `pkg-config`, GMP/GMPXX, OpenSSL und Python 3 prüfen (nicht ungeprüft installieren).
  2. Ein frisches Build-Verzeichnis außerhalb des vorhandenen `build/` konfigurieren; Quellstand (`git rev-parse HEAD`) und Konfigurationsparameter protokollieren.
  3. Bauen und die vollständige CTest-Suite ausführen; Evidence-Chain-Unit- und Integrationspfad in der Ausgabe separat nachvollziehen.
  4. CLI-Dokumentationsprüfung aus dem CI-Profil gegen dieselbe Binärdatei durchführen; Ergebnisse, Fehlschläge und Versions-/Quellbindung ablegen.
- **Betroffene Bereiche:** `CMakeLists.txt`, `src/evidence/chain.cpp`, `apps/bruce_cli.cpp`, `tests/unit/evidence_chain_tests.cpp`, `tests/integration/test_evidence_chain.py`, `.github/workflows/ci.yml` — zunächst nur als Verifikationsgegenstand.
- **Abhängigkeiten/Risiken:** Entwicklungsabhängigkeiten und ein beschreibbarer lokaler Buildpfad; der aktuelle SFTP/GVFS-Buildordner ist hierfür nicht geeignet.
- **Abnahmekriterien:** Frische Konfiguration und Build enden erfolgreich; die gesamte registrierte Suite endet ohne Fehler; beide Evidence-Chain-Testpfade sind Teil dieses Laufes; die generierte CLI-Dokumentation ist konsistent; der Bericht enthält Commit, Befehle und echte Ergebnisse.
- **Geeignete Verifikation:** Lokaler CMake-/Ninja-Build, `ctest --output-on-failure`, bestehender `docs/cli/verify_examples.py` entsprechend `.github/workflows/ci.yml:24-48`.

### AP-02 — Bestehenden Transfervertrag dokumentarisch konsolidieren (erledigt)

- **Priorität / Aufwand:** P0 / klein; **am 03.10.2026 abgeschlossen**.
- **Ziel und Nutzen:** Den bereits implementierten und getesteten opt-in-Transfervertrag von `experience revoke/replace` einheitlich dokumentieren, damit keine doppelte Arbeit aus einem historischen Widerspruch entsteht.
- **Tatsächliches Ergebnis:** `docs/adr/015-control-contract-compatibility.md` und `docs/implementation/184-control-contract-matrix-usage.md` nennen nun den bestehenden opt-in-Transferweg, bewahren den direkten nativen Weg ohne Transferoptionen und benennen `experience inspect` korrekt als weiterhin unkontrollierten Verwaltungsweg. Es wurden weder C++-Code, CLI-Optionen, Datenformate noch Tests geändert.
- **Konkrete Aufgaben:**
  1. ADR-015 und `184-control-contract-matrix-usage.md` gegen den aktuellen Code, Vertrag und CLI-Referenz abgleichen.
  2. Festhalten: Nur bei mindestens einer der bestehenden Transferoptionen wird `controlled-transfer-v1` genutzt; ohne diese Optionen bleiben Bericht und Snapshotbytes im direkten nativen Weg unverändert.
  3. Betroffene Testnachweise (`experience_replace_workflows`, `experience_transfer_workflows`, `control_contract_matrix`) als Nachweis ergänzen, ohne Semantik zu ändern.
  4. Erst danach für einen wirklich neuen Verwaltungsweg entscheiden, ob ein weiterer opt-in-Adapter nötig ist.
- **Betroffene Bereiche:** `docs/adr/015-control-contract-compatibility.md`, `docs/implementation/184-control-contract-matrix-usage.md`; als Beleg `apps/bruce_cli.cpp:639-659`, `docs/contracts/06-control-and-publication.md:20-21`, `docs/cli/commands.md:261-275` und bestehende Tests.
- **Abhängigkeiten/Risiken:** AP-01 ist erfüllt. Der Dokumentationspatch darf keine CLI-Option, Budgetreihenfolge, Fehlerhülle, Ergebnisbytes oder fachliche Experience-Semantik ändern.
- **Abnahmekriterien:** Alle maßgeblichen Dokumente stimmen darin überein, wann `controlled-transfer-v1` gilt und wann nicht; sie nennen die vorhandenen Testnachweise; der frische 130/130-Nachweis bleibt gültig.
- **Geeignete Verifikation:** Dokumentdiff gegen Code/CLI-Handbuch sowie erneute vollständige CTest-Suite nach einer reinen Dokumentationsänderung.

### AP-03 — Regel-Governance als begrenztes Paket schließen

- **Priorität / Aufwand:** P1 / mittel.
- **Ziel und Nutzen:** Nachvollziehbarkeit des lokalen Regelspeichers um Ablehnungen, definierte Migration und Journalrotation ergänzen, ohne die menschliche Bestätigung oder lokale Sicherheitsgrenze umzudeuten.
- **Konkrete Aufgaben:**
  1. Fachlich entscheiden, welche Ablehnungsinformationen gespeichert werden dürfen und wann Rotation/Migration zulässig ist.
  2. Formatversion, Upgrade-/Rollback-Verhalten, Sperr- und Wiederanlaufverhalten in ADR und Vertrag festlegen.
  3. Einen kleinen, atomaren Migrations- und Rotationspfad planen; aktive Regeln dürfen ohne bestätigte Aktion nicht wechseln.
  4. Positive, abgelehnte, unterbrochene und beschädigte-Journal-Fälle als Tests spezifizieren.
- **Betroffene Bereiche:** Vorläufig `apps/rule_store.hpp`, `apps/rule_review.hpp`, `src/application/rule_history.cpp`, `src/application/rule_proposal.cpp`, `src/io/local_files.cpp`, `docs/contracts/05-human-rule-approval.md`, `docs/cli/active-rules.md` und Tests.
- **Abhängigkeiten/Risiken:** Benötigt eine Produkt-/Governance-Entscheidung; keine technische Maßnahme darf eine autonome Regelaktivierung einführen.
- **Abnahmekriterien:** Jede angenommene oder abgelehnte Aktion ist versioniert nachvollziehbar; Migration/Rotation bewahren aktive Regelbindung und Journalintegrität; Abbruch erzeugt keinen stillen Aktivierungswechsel.
- **Geeignete Verifikation:** Neue lokale Dateisystemtests plus CLI-Integration in temporären Verzeichnissen; anschließender vollständiger Suite-Lauf aus AP-01.

### AP-04 — Tokenisierten Task-Spec-Branch getrennt abnehmen (erledigt, mit Merge-Gate)

- **Priorität / Aufwand:** P1 / mittel; **Abnahme am 03.10.2026 abgeschlossen, Merge-Gate offen**.
- **Ziel und Nutzen:** Feststellen, ob `feature/taskspec-token-slice` fachlich und technisch in den Core passt, ohne seinen Funktionsumfang vorab als Teil von `master` auszugeben.
- **Tatsächliches Ergebnis:** Frischer Build aus `git archive feature/taskspec-token-slice` erfolgreich; CTest **133/133** in 32,67 s inklusive der drei Task-Spec-Pfade. Die CLI-Dokumentationsprüfung meldete 140 Beispiele, 79 Optionen, 527 Diagnosen und gültige Links. F-05a verhindert dennoch eine vorbehaltlose Merge-Empfehlung.
- **Konkrete Aufgaben:**
  1. Branch-Spezifikation und Implementierung auf die neuen öffentlichen Verträge, Grenzen, Replay und Rollenalternativen prüfen.
  2. Die 16 geänderten Dateien gegen bestehende `ApplicationTask`-/Forschungsgrenzen und CLI-Referenz abgleichen.
  3. Neues Testmaterial und die generierten CLI-Referenzen im frischen Build aus AP-01 prüfen.
  4. Erst danach explizit entscheiden: überarbeiten, in kleineren Slice zerlegen oder mergen.
- **Betroffene Bereiche:** Branch-Diff nennt `include/bruce/token/task_spec.hpp`, `include/bruce/application/tokenized_task_run.hpp`, `src/token/task_spec.cpp`, `src/application/tokenized_task_run.cpp`, `apps/bruce_cli.cpp`, CMake, CLI-Dokumentation und drei neue Tests.
- **Abhängigkeiten/Risiken:** AP-01; Branch liegt in separatem Worktree und ist nicht `master`. Ein Merge verändert CLI, Daten-/Replayvertrag und Testregistrierung.
- **Abnahmekriterien:** Jeder neue CLI-Befehl und jedes Ergebnisfeld hat eine definierte Semantik und negative Fälle; die vollständige Suite inklusive neuer Tests läuft auf einem frischen Build; Dokumentationsgeneratoren ergeben keine unbeabsichtigten Diffs; Mergeentscheidung ist ausdrücklich festgehalten.
- **Geeignete Verifikation:** Branch-spezifischer frischer CMake-/CTest-Lauf und CI-äquivalente CLI-Dokumentationsprüfung; Review der neuen Unit-/Integrationstests.

### AP-05 — Ein offenes Zielpaket nach Wert und Beweisbarkeit auswählen

- **Priorität / Aufwand:** P2 / groß, vorab nicht belastbar schätzbar.
- **Ziel und Nutzen:** Nach stabiler Verifikation und Governance entweder das Token-/Relationsmodell, NanoCell-Integration, Cockpit, Phase-DAG oder GPU als **ein** abgegrenztes Paket beginnen.
- **Konkrete Aufgaben:** Produktentscheidung treffen; ADR/Spezifikation mit nicht-funktionalen Grenzen, Daten-/Replayformat, Fehlerfällen, Abnahmematrix und Migrationsstrategie schreiben; erst nach Freigabe implementieren.
- **Betroffene Bereiche:** Abhängig von der Auswahl; die Architektur nennt die offenen Grenzen in `docs/audit/2026-10-01-core-0102-ist-gegen-zielbild.md:60-68`.
- **Abhängigkeiten/Risiken:** AP-01 und AP-02; alle Kandidaten sind größere eigenständige Projekte, keine kleine „Restarbeit“.
- **Abnahmekriterien:** Das ausgewählte Paket hat einen eigenen, begrenzten Vertrag und reproduzierbare End-to-End-Abnahme; es behauptet keine allgemeine mathematische Wahrheit und aktiviert keine Regeln autonom.
- **Geeignete Verifikation:** Neue, paketbezogene Unit-/Integration-/Replay- und gegebenenfalls manuelle Nutzungsabnahme.

### AP-06 — Suchoptimierung und breitere Runtime erst danach bewerten

- **Priorität / Aufwand:** P3 / offen.
- **Ziel und Nutzen:** Erst nach belastbaren Verträgen messen, ob Suchoptimierungen, Parallelität oder GPU einen nachweisbaren Nutzen haben.
- **Konkrete Aufgaben:** Repräsentative, versionierte Benchmarks festlegen; Ergebnisse gegen gleiche Befunde und Grenzen vergleichen; keinen Performance-Claim ohne Messung veröffentlichen.
- **Betroffene Bereiche:** Vorläufig `benchmarks/`, `scripts/benchmark.py`, mathematische Strategien; keine konkrete Dateiänderung aus diesem Audit ableitbar.
- **Abhängigkeiten/Risiken:** Stabile Semantik, aktuelle Baseline und ein beschlossenes Zielpaket. Parallel/GPU darf nicht aus vorhandenen Testzahlen abgeleitet werden.
- **Abnahmekriterien:** Messmethodik, Hardware-/Buildkontext, Workload, gleiche Ergebnissemantik und begrenzte Aussage sind dokumentiert.
- **Geeignete Verifikation:** Reproduzierbarer lokaler Benchmarklauf, unabhängig geprüfte Ergebnisgleichheit und Replay-Stichprobe.

## 7. Empfohlene Reihenfolge

1. **AP-01 abgeschlossen:** Der frische Core-0.102-Build, die 130/130 CTest-Prüfungen und die CLI-Dokumentationsprüfung liefern jetzt die belastbare Baseline für `master`.
2. **AP-02 abgeschlossen:** ADR-015 und der Core-0.102-Nutzungseintrag beschreiben nun den bestehenden opt-in-Transferweg für `revoke/replace` konsistent; ohne Transferoptionen bleibt der direkte Weg ausdrücklich erhalten.
3. **AP-04 geprüft, aber nicht freigegeben:** Der Token-Spec-Branch ist frisch gebaut, vollständig getestet und dokumentiert; vor einem Merge muss F-05a entschieden und umgesetzt werden. F-05b ist eine klar abgrenzbare Folgeentscheidung.
4. **AP-05 erst mit expliziter Zielauswahl:** Cockpit, freie Sprache, NanoCell-Integration, Phase-DAG und GPU sind getrennte Zielpakete. Sie sollten nicht parallel als unscharfer Ausbau begonnen werden.
5. **AP-06 zuletzt:** Optimierung ist nur sinnvoll, wenn die vorigen Verträge und die Vergleichsbasis feststehen.

## 8. Offene Fragen und Annahmen

1. **Buildumgebung:** Welche beschreibbare lokale Umgebung stellt die bestehenden CMake-/GMP/GMPXX-/OpenSSL-Abhängigkeiten bereit? Diese Antwort wird vor AP-01 benötigt; sie entscheidet über den Verifikationsort, nicht über Produktsemantik.
2. **Controller-Ziel:** Soll der nächste Ausbau einen vorhandenen Vertrag bewusst übernehmen oder eine neue opt-in-Controllerfamilie erhalten? Diese Architekturentscheidung wird vor AP-02-Implementierung benötigt.
3. **Token-Branch:** Ist `feature/taskspec-token-slice` ein zur Integration vorgesehener Funktionsumfang oder ein experimenteller Arbeitszweig? Die Antwort ist vor AP-04-Mergeentscheidung nötig.
4. **Produktpriorität nach Stabilisierung:** Welches einzelne Zielpaket hat Vorrang: Token-/Relationsmodell, NanoCell-Integration, Cockpit, Phase-DAG oder GPU? Ohne diese Antwort ist AP-05 absichtlich nicht weiter konkretisiert.
5. **Governance-Umfang:** Welche Informationen und Aufbewahrungsfristen soll ein Ablehnungsjournal haben? Diese Antwort wird vor AP-03-Datenformat und Migration benötigt.

## 9. Verifikationsprotokoll

### Tatsächlich ausgeführt (lesend bzw. nicht produktiv)

| Prüfung | Ergebnis |
| --- | --- |
| `pwd -P`; `git rev-parse --show-toplevel`; `git status --short --branch`; `git log -5 --date=short --pretty=format:'%h %ad %s'`; `git remote -v` | Root/Branch/ungestrackte Ausgangslage ermittelt; keine Remote-Ausgabe. |
| `rg --files ...` | Nicht verfügbar (`rg: command not found`); keine Dateien verändert. |
| `find`-Inventar der üblichen Projekt-/Build-/Dokumentationsdateien und Verzeichnisse | CMake-, Python-, Dokumentations-, Test- und Buildstruktur ermittelt. |
| `git ls-files`; `nl -ba` auf README, CMake, Python-Konfiguration, CI, Architektur-, Audit-, Vertrags-, ADR- und ausgewählten C++-Dateien | Die im Bericht genannten Pfade/Zeilen und Modulgrenzen geprüft. In einer großen Sammelausgabe traten Terminal-Kürzungen auf; alle tragenden Belege wurden danach gezielt einzeln gelesen. |
| `grep`/`nl -ba` auf Experience-CLI, `ExperienceTask`, Transfervertrag, `test_experience_replace.py`, ADR-015, Control-Vertrag und CLI-Handbuch | Opt-in-Transfersteuerung für `experience revoke/replace` im Code und in der aktuellen CLI-Dokumentation bestätigt; veralteter Gegenstand in ADR-015 und `184-control-contract-matrix-usage.md` identifiziert. |
| `git grep -n` nach Evidence-Chain-Einstiegen und nach TODO/FIXME/XXX/HACK | Evidence-Chain-CLI/Code/Testregistrierung bestätigt. In Produktcode wurden keine inhaltlichen TODO/FIXME-Marker gefunden; Treffer betreffen temporäre Dateinamen bzw. historische Planungsdokumente. |
| `git branch -vv`; `git log master..feature/taskspec-token-slice`; `git diff --stat/--name-only master...feature/taskspec-token-slice` | Separaten, ungemergten Token-Task-Spec-Branch mit sieben Commits und 16 geänderten Dateien bestätigt. |
| `ctest -N --test-dir build` | **Keine Tests ausgeführt.** CTest scheiterte vor der Liste an `Cannot create directory .../build/Testing/Temporary` und `Cannot create log file: LastTest.log`; daher keine Testaussage daraus abgeleitet. |
| Frische Konfiguration: `cmake -G Ninja -S . -B /tmp/bruce-ap01-build-Zr6a2h -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release` | Erfolgreich; CMake erkannte GCC 14.2.0, OpenSSL 3.5.7, `gmpxx` 6.3.0 und Python 3.13.5. |
| Frischer Build: `cmake --build /tmp/bruce-ap01-build-Zr6a2h --parallel 4` | Erfolgreich; der Build kompiliert unter anderem `src/evidence/chain.cpp` und die Evidence-Chain-Unit-Test-Binärdatei. |
| Vollständige Suite: `ctest --test-dir /tmp/bruce-ap01-build-Zr6a2h --output-on-failure --parallel 4` | **130/130 bestanden**, 0 fehlgeschlagen, 35,49 s; `evidence_chain_contracts` und `evidence_chain_workflows` bestanden. |
| CI-äquivalente CLI-Dokumentationsprüfung in einer temporären `git archive HEAD`-Kopie: `python3 .../docs/cli/verify_examples.py /tmp/bruce-ap01-build-Zr6a2h/bruce`, danach drei `cmp`-Prüfungen | Erfolgreich: 135 Beispiele, 77 dokumentierte Optionen, 470 Diagnosen, Links OK; erzeugte Referenzdateien bytegleich zum Checkout. |
| Dokumentationskonsolidierung AP-02: gezielter Diff-/Textabgleich von ADR-015 und `184-control-contract-matrix-usage.md` gegen `apps/bruce_cli.cpp:639-659`, Control-Vertrag und CLI-Handbuch | Bestehender opt-in-Transferweg für `experience revoke/replace` belegt; zwei veraltete Gegenbehauptungen korrigiert. `git diff --check` und Leerzeichenprüfung endeten ohne Befund; seit der frischen 130/130-Verifikation wurden keine Produktdateien geändert. |
| Branch-Abnahme AP-04: `git archive feature/taskspec-token-slice` in temporäre Quelle, frisches CMake/Ninja-Release-Build und `ctest --output-on-failure --parallel 4` | **133/133 bestanden**, 0 fehlgeschlagen, 32,67 s; darunter `task_spec_contracts`, `tokenized_task_run_contracts` und `task_spec_workflows`. |
| Branch-CLI-Dokumentationsprüfung: `python3 /tmp/bruce-ap04-source-CdWbnw/docs/cli/verify_examples.py /tmp/bruce-ap04-build-YMqS3p/bruce` | Erfolgreich: 140 Beispiele, 79 dokumentierte Optionen, 527 Diagnosen, Links OK; die erzeugten Referenzdateien waren bytegleich zum Branch-Archivstand. |
| Quellreview AP-04: `task_spec.cpp`, `tokenized_task_run.cpp`, neue Unit-/Integrationstests sowie CLI-Diff | F-05a und F-05b direkt aus Validierungs-/Kompilierungsgrenzen und sichtbaren CLI-Aktionen abgeleitet; sie widerlegen weder den bestandenen Build noch die getesteten direkten V1-Abläufe. |

### Bewusst nicht ausgeführt

- Kein Installer, Benchmark, Produkt-CLI-Manuallauf, Dependency-Installation, Release oder Deployment. CMake-Configure/Build, vollständiger CTest-Lauf und die CLI-Dokumentationsprüfung wurden ausschließlich isoliert unter `/tmp` ausgeführt; der Repository-Checkout blieb unverändert.
- Keine Netzwerk-, Remote-, Release-, Deploy-, Installations-, Git-Schreib- oder Datenmigrationsaktion.
- Keine Prüfung oder Änderung ungetrackter Nutzerartefakte.

### Gegenprüfung dieses Berichts

- Wesentliche Empfehlungen referenzieren einen konkreten Befund oder sind als offen/abgeleitet markiert.
- Reihenfolge und Abhängigkeiten stehen in Abschnitt 7; jedes Arbeitspaket enthält überprüfbare Abnahme- und Verifikationskriterien.
- Die Aussagegrenzen historischer Testdokumentation, des aktuellen Quellstandes und der ungemergten Branch wurden getrennt gehalten.
