# BRUCE_raw – Istbestand gegen Zielbild (Core 0.102)

**Auditdatum:** 01.10.2026  
**Scope:** Lokaler Quellbaum `/home/chris/Desktop/Bruce_raw` und vorhandener Debug-Build `build/`.  
**Ziel:** Implementierung, dokumentierte Verträge und Zielbild nicht vermischen.

## Ergebnis in einem Satz

Der vor der Exact-Evidence-Chain-Änderung vorhandene BRUCE-Build ist ein **getesteter lokaler C++20-Forschungskern mit CLI** für klar begrenzte mathematische Such-, Prüf-, Replay- und Steuerungswege. Die neue Chain ist bislang nur auf Quellebene implementiert und hier nicht frisch gebaut oder ausgeführt. BRUCE ist **nicht** die vollständige Zielarchitektur eines Research Cockpits mit allgemeiner Gate-Pipeline, freier Sprache, Plugin-/Phase-DAG- oder GPU-Runtime.

Die höchste Priorität ist daher nicht Suchoptimierung, sondern die verbleibenden **Kontroll- und Evidenzgrenzen** sauber auszubauen und nachzuweisen.

## Prüfbelege und Grenzen

| Prüfung | Ergebnis | Aussagegrenze |
|---|---|---|
| `build/bruce --version` | `0.102.0` | Belegt die vorhandene Binärdatei, nicht allein den Quellstand. |
| `ctest --test-dir build --output-on-failure` (vor der Chain-Änderung) | **128/128 bestanden**, 206,65 s | Belegt nur die damals registrierten Tests des vorhandenen Builds; neue Chain-Quellen und CLI wurden dadurch nicht geprüft. |
| `sha256sum -c build/source-files.sha256` (vor der Chain-Änderung) | Erfolgreich | Belegte den damaligen Quellstand; keine Hashbindung der später hinzugefügten Chain-Quellen. |
| Frischer Build/Reconfigure der Chain | Nicht möglich: `pkg-config` und `gmpxx.h` fehlen | Der hinzugefügte C++-/CLI-Code wurde hier nicht kompiliert oder ausgeführt. |

Der frühere CMake-Glob erkannte zusätzlich acht unregistrierte Python-Dateien direkt unter `tests/`; dies löste die Reconfigure-Prüfung aus, änderte aber nicht die damalige Hashliste. Für die neue Chain steht eine frische Konfiguration, Kompilierung und Testausführung aus.

**Ausstehende Verifikation nach Bereitstellung der bestehenden Projektabhängigkeiten:**

```bash
cmake -S . -B build
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

## Klassifikation

### Vor der Chain-Änderung implementiert und für den vorhandenen Build getestet

| Bereich | Konkreter Nachweis | Einordnung |
|---|---|---|
| Lokaler C++20-Kern | `CMakeLists.txt`, `src/`, `include/bruce/`; GMP/GMPXX und OpenSSL | `bruce_core` und die CLI `bruce` sind reale Targets. |
| Acht feste Auftragsarten | `include/bruce/application/application_task.hpp`, `src/application/application_task.cpp` | Feste Laufarten, keine freie Task-Registrierung oder allgemeiner DAG. |
| Mathematische Suchfamilien | `src/math/`, `src/application/{exact,polynomial,expression,rational_linear,portfolio}_research.cpp` | Begrenzte, explizit implementierte Suchräume; Treffer bleiben statusgebunden. |
| Z-Kompatibilität | `src/compatibility/{selection,snapshot}.cpp` | Versionierte Regel-/Auswahlwege mit Trace und Replay-Kontrollen. Ein Score ist kein Beweis. |
| Replay und native Bundles | `src/evidence/bundle.cpp`, `src/application/{replay,exact_replay,application_replay}.cpp` | Reproduziert gebundene konkrete Laufwege; ersetzt keinen universellen Beweis. |
| Experience/Transfer | `src/experience/store.cpp`, `src/application/{experience_*,compatibility_transfer}.cpp` | Kontrollierte, begrenzte Speicher- und Übergabewege; keine automatische Regelpromotion. |
| Menschliche Regelkontrolle | `apps/{rule_store,rule_review}.hpp`, `src/application/{rule_proposal,rule_history}.cpp` | Konkreter Terminaldialog und lokaler Store; keine Identitätsgrenze gegen Programme desselben Nutzerkontos. |
| Lokaler Sprachpfad | `src/language/language.cpp`, `src/application/language_run.cpp` | Deterministisch und begrenzt; keine freie Sprache und kein LLM-Pfad. |
| NanoCell | `src/nanocell/nanocell.cpp`, `apps/nanocell_cli.hpp` | Isolierter Bytekern mit eigenem CLI-Weg, nicht Teil von `ApplicationTask`. |
| Lokaler semantischer Index | `bruce_semantic_index/`, `pyproject.toml` | Separates Python-3.11+-Paket; keine CMake-Target-Bindung an die BRUCE-Ausführung. |

## Dokumentiert, aber bewusst nur begrenzt oder teilweise umgesetzt

| Vertrag/Ziel | Tatsächlicher, belegter Stand | Nicht daraus ableiten |
|---|---|---|
| Evidence → Countercheck → Replay | Konkrete Forschungs- und Replaypfade existieren. | Kein allgemeiner, unabhängiger `CounterexampleFinderV1`/`HoldOutRunnerV1`/`EvidenceGateV1`; der allgemeine Vertrag bleibt Blueprint. |
| Exact-Affine-Evidence-Chain V1 | Separater `bruce evidence create|replay`-Pfad ist auf Quellebene implementiert: feste `affine`/`enumerate`-Route nur für `bigint`/`rational` (Standard `bigint`), rohe Trainings-/Holdout-CSV-Bytes, feste Policy-/Scope-Felder, lokales begrenztes Replay. Frischer Build/Test stehen aus. | `declared-disjoint-v1` ist x-Disjunktheit plus lokale Deklaration, kein Nachweis externer Unabhängigkeit/Provenienz. `countercheck_scope short-circuit-training-then-holdout-v1` bindet den kurzschließenden Kontrollfluss, keine Vollabdeckung. Jeder erreichte Kandidat wird zuerst an Training geprüft; die erste Kontradiktion beendet seine Prüfung. Die inverse Gegenprüfung läuft nur nach einer passenden Vorwärtsauswertung. Holdout beginnt nur nach bestandener Trainingsprüfung und wenn Resultat-/Budgetfluss es erreicht. Budgets können innerhalb eines Kandidaten stoppen; `INCOMPLETE` ist kein Abdeckungsnachweis. Weder Kandidat, Score, `VERIFIED_ON_SAMPLES`, Digest noch Replay sind allgemeiner Beweis; kein Learn-/Publish-/Regelaktivierungsgate, kein allgemeiner `ApplicationTask`, externer Checker, Cloud- oder Strategy-Promotion-Pfad. |
| Ergebnisstatus | CLI/Code unterscheiden `CANDIDATE`, `VERIFIED_ON_SAMPLES`, `INCOMPLETE` und benannte Beweisfälle. | `VERIFIED_ON_SAMPLES` ist **kein** allgemeiner Formelbeweis. |
| Experience | Snapshots, Quellen-Replay, Deduplizierung und Widerruf sind als Teilpaket vorhanden. | Kein vollständiger `ExperienceRecordV1`-Vertrag, keine externe Evidenzprüfung oder Learn-/Rule-Gate-Promotion. |
| Regelhoheit | Aktive Regeländerung verlangt konkrete lokale menschliche Bestätigung; Pläne/Gates/Digests aktivieren nicht. | Terminalbestätigung authentifiziert keinen Menschen und schützt nicht gegen Programme desselben Kontos. |
| Datei-/Exportkontrolle | Gemeinsamer lokaler Dateiadapter und geprüfte Kontrollwege. | Keine globale Zeit-/RAM-/CPU-Garantie, keine automatische Veröffentlichung oder Regelaktivierung. |

## Zielbild bzw. offene Pakete

Die Architektur dokumentiert diese Teile selbst als offen bzw. nicht als laufende Produktrealität:

1. **Allgemeine Gate-, Countercheck- und Hold-out-Runtime** mit expliziter Unabhängigkeit und Suchraumbeschreibung.
2. **Token-/Relationsmodell** gemäß Zielvertrag; vorhanden ist nur der begrenzte Sprachpfad.
3. **Research Cockpit**, freie Sprache, **Plugin-Runtime**, **Phase-DAG** und **GPU-Ausführung**.
4. Eine vollständige Entkopplung der Bibliotheksschichten; dokumentiert bleibt insbesondere ein Store-zu-Anwendungsaudit.
5. Zusätzliche lokale Governance-Funktionen wie Ablehnungsjournal, Migration und Journalrotation.

## Priorisierte nächste Arbeit

| Priorität | Paket | Akzeptanzkriterium |
|---:|---|---|
| P0 | Kontroll-/Fehlervertrag harmonisieren | Explizite Entscheidung und Tests für Controller-, Fehler- und Zuständigkeitsgrenzen; keine stillschweigende Semantikänderung. |
| P0 | Exact-Affine-Evidenzkette verifizieren | Die eng begrenzte Quellimplementierung mit getrennten Trainings-/Holdout-Bytes, festem Countercheck-Scope, Bundle und Replay erst nach frischer Konfiguration, Build und Tests als überprüft einordnen. Kein abgeschlossen abgenommenes P0-Paket vor dieser Verifikation. |
| P1 | Regelverwaltung ergänzen | Nur nach menschlicher Freigabe: Ablehnung, Migration und Journalrotation; weiterhin keine autonome Aktivierung. |
| P1 | Audit reproduzierbar machen | `pkg-config` und GMP-Entwicklungsheader (`gmpxx.h`) in der Build-Umgebung bereitstellen und die oben genannten drei Befehle mit tatsächlichem Resultat dokumentieren; Testzahl nicht aus dem früheren Build übernehmen. |
| P2 | Erst danach Erweiterungen | Tokenmodell, NanoCell-Integration, Cockpit, DAG oder GPU nur als getrennte, testbare Pakete. |

## Harte fachliche Grenzen

- Ein Kandidat, Score, Digest, Gatebericht oder erfolgreiches Replay ist **keine** automatische mathematische Wahrheit.
- Ein mathematischer Wert ist keine Adresse: Adresse, Wert, Token/Bedeutung und Evidenz bleiben getrennt.
- Strategien, Erfahrung, Gateberichte und Sprachkomponenten dürfen weder Regeln autonom aktivieren noch veröffentlichen oder verändern.
- BRUCE trifft **keine** Aussage über reale Beschleunigung von Private-Key-Suche, Bitcoin-Mining, BSGS oder vergleichbaren kryptografischen Angriffen.

## Quellen im Quellbaum

- Konsolidierter Stand und offene Pakete: `docs/architecture/03-blueprint-consolidation.md`
- Zielarchitektur und Umsetzungsgrenzen: `docs/architecture/02-system-architecture.md`
- Invarianten und Statusgrenzen: `docs/architecture/01-identity-and-invariants.md`
- Evidenz-/Experience-Grenzen: `docs/contracts/03-evidence-and-experience.md`, `04-math-evidence-replay.md`
- Menschliche Regelhoheit: `docs/contracts/05-human-rule-approval.md`, `docs/cli/active-rules.md`
- Ergebnisstatus: `docs/cli/results-and-control.md`
- Build-/Testdefinition: `CMakeLists.txt`
