# BRUCE CLI – quellengestützter Dokumentations-Prompt und Referenz

**Stand:** 29. September 2026 · **geprüfte Programmversion:** `0.102.0` · **Sprache:** Deutsch

Diese Datei ist eine ergänzende Referenz zum bestehenden [CLI-Handbuch](README.md). Sie ist bewusst an der tatsächlich eingebauten CLI ausgerichtet: `apps/bruce_cli.cpp`, `apps/nanocell_cli.hpp`, `apps/rule_store.hpp`, `apps/rule_review.hpp` und der Ausgabe von `bruce --help`. Entwürfe oder Roadmap-Einträge sind keine Befehlszusage.

## Wiederverwendbarer Prompt

Kopiere den folgenden Prompt in ein LLM, wenn eine Erklärung der lokalen BRUCE-CLI erstellt oder aktualisiert werden soll:

```text
Du bist technischer Redakteur für die lokale C++20-CLI „BRUCE“. Erstelle eine präzise, deutschsprachige und quellengestützte Bedienungsanleitung für den tatsächlich implementierten Stand.

Arbeite in dieser Reihenfolge:
1. Lies zuerst `apps/bruce_cli.cpp`, `apps/nanocell_cli.hpp`, `apps/rule_store.hpp` und `apps/rule_review.hpp` vollständig. Führe danach das gebaute Programm mit `--version` und `--help` aus. Nutze vorhandene Dokumentation nur zum Abgleich, nie als alleinige Quelle.
2. Dokumentiere ausschließlich Befehle, Unterbefehle, Optionen, Standardwerte und Grenzwerte, die im Parser bzw. in der Ausführung vorhanden sind. Kennzeichne fehlende oder geplante Funktionen ausdrücklich als nicht implementiert; erfinde niemals Flags, Kurzoptionen, Umgebungsvariablen, Konfigurationsdateien oder APIs.
3. Für jeden Befehl erkläre: Zweck, vollständige Syntax, Positionsargumente, jede Option mit Pflichtstatus, Standardwert, zulässigen Werten/Format, Abhängigkeiten und Konflikten, Ergebnis/Export sowie typische Fehler. Trenne allgemeine Sitzungsoptionen von transfer-spezifischen Optionen.
4. Erkläre die globalen Aufrufregeln: Optionen haben genau einen getrennten Wert; keine Kurzformen, keine `--name=wert`-Schreibweise, keine Wiederholungen, Groß-/Kleinschreibung ist relevant. Erfolgsantworten sind JSON auf stdout; Fehler sind JSON auf stderr mit Exitcode 2. Exitcode 0 beweist keine vollständige Suche oder mathematische Allgemeingültigkeit.
5. Behandle die folgenden Gruppen vollständig: Hilfe/Vertragskataloge, Rechnen und Koordinaten, Grid, Forschung, Bruchvergleich, Sprache, NanoCell, Regelverwaltung, Experience, Z-Kompatibilität, Laufinspektion und Replay. Erkläre auch Datei- und Exportregeln: `--output` ist optional, Exporte werden exklusiv erstellt und vorhandene Ziele niemals überschrieben; Zielverzeichnisse entstehen nicht automatisch.
6. Liefere zwei Artefakte: Markdown für Menschen und valides JSON für Programme. Das JSON braucht mindestens `schema_version`, `generated_from`, `cli_version`, `global_rules`, `command_groups` und pro Befehl `path`, `summary`, `syntax`, `options`, `positionals`, `output`, `constraints` und `source_files`.
7. Verifiziere vor Abschluss: JSON mit einem Parser laden, jede `path`-Zeile gegen `bruce --help` und die Parser-Whitelist prüfen, alle Standardwerte aus dem Code abgleichen und ungesicherte Aussagen entfernen.

Schreibe knapp genug zum Nachschlagen, aber so konkret, dass ein Nutzer weiß, welchen Befehl und welche Option er für welchen Zweck verwendet. Nutze keine Behauptung über reale Private-Key-, Bitcoin-, Mining- oder BSGS-Beschleunigung.
```

## Globale Bedienregeln

| Regel | Bedeutung |
| --- | --- |
| Hilfe | `bruce`, `bruce help` und `bruce --help` geben die Gesamthilfe aus. Unterbefehle besitzen kein eigenes `--help`. |
| Optionen | Jede Option ist ein Paar aus Name und Wert, etwa `--max-bits 4096`. `--x=1`, Kurzoptionen, kombinierte Schalter und `--` sind nicht implementiert. |
| Wiederholung | Eine Option darf pro Aufruf nur einmal vorkommen. Unbekannte Optionen, fehlende Werte und doppelte Optionen sind Fehler. |
| Zahlen/Text | Optionen und Werte sind groß-/kleinschreibungssensitiv. Negative Werte stehen als eigenes Argument, z. B. `--min -8`. Shell-Sonderzeichen und Leerzeichen quoten. |
| Ausgabe | Erfolgreiche Fachverarbeitung: JSON auf stdout, Exitcode 0. Fehler: JSON auf stderr, Exitcode 2. |
| Bedeutung von Erfolg | `INCOMPLETE`, `NO_CANDIDATE`, `REFUTED`, `UNDECIDED`, Mehrdeutigkeit sowie NanoCell-`NO_MATCH`/`CONTEXT_REJECTED` sind reguläre fachliche Ergebnisse; Exitcode 0 ist kein Beweis. |
| Dateien | Ohne `--output` wird kein mathematisches Bundle geschrieben. Exporte sind exklusiv: vorhandene Ziele werden nicht überschrieben und Elternverzeichnisse nicht erzeugt. |

## Gemeinsame Optionsfamilien

Die folgenden Optionen gelten **nicht** pauschal für alle Befehle. Sie sind nur bei den im jeweiligen Parser zugelassenen Laufbefehlen gültig.

| Familie | Optionen | Zweck |
| --- | --- | --- |
| Sitzungsprüfung | `--verification none|replay`, `--max-session-checkpoints N`, `--verification-cancellation WERT`, `--max-replay-bytes N`, `--max-serialized-bytes N` | Prüft optional den erzeugten Lauf vollständig erneut und begrenzt Replay-, Serialisierungs- oder Prüfpunktausgaben. `--max-session-checkpoints` ist nur mit `--verification replay` sinnvoll/zulässig. |
| Ausführungsabbruch | `--execution-cancellation none|experience-checkpoints|grid-source-checkpoints|grid-and-experience-checkpoints|compatibility-source-checkpoints|resume-import-checkpoints` | Aktiviert Signalauswertung an dem zum Lauf passenden Importpfad. |
| Serialisierungsabbruch | `--serialization-cancellation none|digest-checkpoints|grid-record-checkpoints` | Beobachtet Prüfsummen- bzw. Grid-Datensatzgrenzen; `grid-record-checkpoints` ist auf Grid-Batch, Grid-Resume und Grid-Forschung beschränkt. |
| Transfersteuerung | `--max-transfer-checkpoints N`, `--transfer-cancellation none|checkpoints` | Gilt nur für Experience- und Compatibility-Plan/Apply-Übergaben, nicht für allgemeine Sitzungen. |

`SIGINT`/`SIGTERM` kann an eingebauten Such- oder Prüfgrenzen wirken. Es ist kein allgemeines Zeit-, Speicher- oder Signallatenzlimit.

## Befehlsreferenz

### Metadaten und Hilfe

| Pfad | Verwendung |
| --- | --- |
| `bruce --version` | Gibt Version und Vertragskennung aus; akzeptiert keine weiteren Argumente. |
| `bruce contracts` | Listet eingebaute Forschungsstrategien, Standardplaner und zulässige Planer. |
| `bruce task-contracts` | Gibt die acht Anwendungstypen und Architekturgrenzen aus. |
| `bruce gate-contracts` | Gibt den versionierten Forschungs-Gatevertrag aus; prüft keinen Lauf. |

### Elementare Mathematik und Adressen

| Pfad | Syntax | Wann verwenden | Wichtige Optionen |
| --- | --- | --- | --- |
| `coordinate` | `bruce coordinate --x X --y Y` oder `bruce coordinate --z Z` | Reversible Cantor-Adresse zwischen `(x,y)` und `z` berechnen. | Genau eine Form. Alle Werte UInt64; lokale Koordinaten `<8` liefern zusätzlich `local_z`. |
| `calculate` | `bruce calculate --op OP --left A --right B [--domain bigint|rational] [--max-bits N] [--modulus M]` | Eine einzelne exakte Rechenoperation ausführen. | `--op`, `--left`, `--right` Pflicht. OP: `add`, `sub`, `mul`, `div`, `divmod`, `pow`, `modpow`. Standarddomäne `bigint`, Standard `--max-bits 4096`; `--modulus` nur für `modpow`. |
| `compare` | `bruce compare --left-numerator LISTE --left-denominator LISTE --right-numerator LISTE --right-denominator LISTE [--domain-policy ...] [--max-bits N] [--max-operations N] [Sitzungsoptionen] [--output DATEI]` | Zwei Polynombrüche über Q samt Definitionsbereich vergleichen. | Vier Koeffizientenlisten Pflicht, aufsteigende Potenzreihenfolge. `--domain-policy`: `quadratic` (Standard), `modular`, `modular-witness`; Defaults: 4096 Bits, 1000000 Operationen. |

### Grid

| Pfad | Syntax | Wann verwenden | Wichtige Optionen |
| --- | --- | --- | --- |
| `grid` | `bruce grid --path '2,6/4,3' [--mode exact|modular|metric] [--modulus M] [--max-bits N] [--zero-zero one|stop] [Sitzungsoptionen] [--output DATEI]` | Letztes lokales Koordinatenpaar eines Pfads als `x^y` auswerten. | `--path` Pflicht, 1..64 Paare in `0..7`. Standardmodus `exact`, Standard 64 Bits, `0^0=1`. `--modulus` ist nur bei `modular` Pflicht. |
| `grid-batch` | `bruce grid-batch --paths 'P1;P2;...' [--cache-bytes N] [--max-query-steps N] [Grid- und Sitzungsoptionen] [--output DATEI]` | Mehrere Grid-Pfade mit lokalem FIFO-Cache auswerten; eignet sich für fortsetzbare Teilergebnisse. | `--paths` Pflicht; höchstens 1024 Pfade. Standardcache 65536 Bytes, `0` deaktiviert ihn. `--max-query-steps 0` erzeugt einen unvollständigen Lauf. |
| `grid-resume` | `bruce grid-resume --from-run PARTIAL --additional-queries N [Sitzungsoptionen] [--output DATEI]` | Ein unvollständiges Grid-Batch-/Resume-Bundle fortsetzen. | Beide Optionen Pflicht; N ist 1..1024. Quelle muss unvollständig und replaybar sein; Konfiguration wird übernommen. |

### Forschung

```text
bruce research --train TRAIN.csv [--holdout HOLD.csv] [Forschungsoptionen] [Sitzungsoptionen] [--output RUN.bruce]
bruce research --grid-train GRID.bruce [--grid-holdout GRID.bruce] [--grid-axis x|y]
               [--max-total-operations N] [Forschungsoptionen] [Sitzungsoptionen] [--output RUN.bruce]
```

`--train` und `--grid-train` schließen sich aus. Ein Holdout ist optional, muss aber zu seiner Quellenform passen. Relevante gemeinsame Forschungsoptionen: `--strategy`, `--domain`, `--planner`, `--min`, `--max`, `--max-candidates`, `--max-operations`, `--max-trace`, `--max-results`, `--max-bits`, `--max-denominator`, `--max-trace-bytes`, `--experience` und `--output`.

| Strategie | Zweck und spezielle Optionen |
| --- | --- |
| `affine` (Standard) | Sucht `a*x+b`; CSV und ohne `--domain` standardmäßig `int64`. |
| `power-affine` | Sucht `a*x^p+b`; `--max-power` (Standard 4), Planer `enumerate|adaptive`; exakte Domäne. |
| `polynomial` | Sucht Polynome; `--max-degree` (Standard 3), Planer `interpolate`; exakte Domäne. |
| `rational-linear`, `rational-quadratic` | Rationale Familien; Planer `solve`, rationale Domäne. |
| `expression` | Ausdruckssynthese mit rationaler Domäne; `--max-nodes`, `--operators`, `--equivalence`, `--domain-cache`, Planer `grammar|indexed-grammar|cached-grammar`. |
| `portfolio`, `portfolio-rational`, `portfolio-quadratic`, `portfolio-expression` | Kombinieren mehrere Familien; erlauben je nach Familie Portfolio-/Adaptive-/Experience-Planung. `--experience` verlangt den passenden rationalen Experience-Pfad. `portfolio-expression` akzeptiert zusätzlich `--expression-planner`. |

Forschungsbudgets haben die Code-Standardwerte `--max-candidates 100000`, `--max-operations 1000000`, `--max-trace 100002`, `--max-results 128`. Der exakte Pfad verwendet standardmäßig `--max-bits 4096`; die Standardwerte der Suchbereiche sind meist `-8..8`, bei Ausdrucksstrategien `0..1`. Zulässigkeit einzelner Spezialoptionen hängt von Strategie, Domäne und Planer ab; unpassende Kombinationen werden abgewiesen.

### Sprache und NanoCell

| Pfad | Syntax | Wann verwenden | Einschränkungen |
| --- | --- | --- | --- |
| `language interpret` | `bruce language interpret --text TEXT --train TRAIN.csv --holdout HOLD.csv [Forschungsbudgets] --output DRAFT` | Einen der zwei fest eingebauten deutschen Satzmuster in einen kanonischen Entwurf umsetzen. | Alle vier Hauptoptionen Pflicht. Keine freie Sprache oder Dateipfade aus Text. |
| `language select` | `bruce language select DRAFT --alternative ID --output SELECTED` | Bei mehrdeutigem Entwurf eine ausgegebene Alternative wählen. | `DRAFT` ist Positionsargument; beide Optionen Pflicht. |
| `language execute` | `bruce language execute DRAFT --output LANGUAGE_RUN` | Einen kanonischen, ausgewählten Entwurf als Core-Lauf ausführen. | Export ist Pflicht. |
| `language replay` | `bruce language replay LANGUAGE_RUN` | Sprachlauf und eingebetteten Core-Lauf prüfen. | Nimmt genau eine Datei, schreibt keine neue. |
| `nanocell run` | `bruce nanocell run --core HEX16 --active-index 0..7 --context-id ID --context-file FILE --technique ID [--max-operations N] [--max-trace-entries N] [--output DATEI]` | Isolierten Acht-Byte-Kern mit gebundenem Kontext ausführen. | Fünf Hauptoptionen Pflicht. Techniken: `masked-match-v1`, `rotate-match-v1`, `context-conflict-v1`; Defaults 16 Operationen und 18 Traceeinträge. |
| `nanocell replay` | `bruce nanocell replay RUN` | NanoCell-Bundle semantisch erneut prüfen. | Eigenes Bundleformat; nicht mit `bruce replay` verwenden. |

### Regeln, Experience und Z-Kompatibilität

| Pfad | Syntax | Wann verwenden | Wichtig |
| --- | --- | --- | --- |
| `rules-stage` | `bruce rules-stage STORE NEW PROPOSAL` | Aus zwei Regelständen einen Vorschlag erzeugen. | Positionsargumente; kein Sitzungsbundle. |
| `rules-propose` | `bruce rules-propose OLD NEW PROPOSAL` | Regeländerung als prüfbaren Vorschlag erzeugen. | Positionale Dateien. |
| `rules-approve` | `bruce rules-approve PROPOSAL CURRENT OUTPUT` | Einen Vorschlag nach zwingender Terminalbestätigung als Regeldatei exportieren. | Interaktiver Dialog; Ablehnung ist Exit 2. |
| `rules-activate` | `bruce rules-activate STORE PROPOSAL` | Einen bestätigten Vorschlag nach Terminalbestätigung atomar aktivieren. | Aktualisiert den benannten Regelspeicher als Ausnahme von der exklusiven Exportregel. |
| `rules-status` | `bruce rules-status STORE` | Aktive Regeln und Journal eines Speichers anzeigen. | Nur Positionsargument. |
| `experience add` | `bruce experience add --from-run RUN [--store STORE] --output NEW [Transfersteuerung]` | Gültigen Forschungsbefund in einen Experience-Snapshot übernehmen. | `--from-run`, `--output` Pflicht. |
| `experience revoke` | `bruce experience revoke --store STORE --id ID --reason TEXT --output NEW [Transfersteuerung]` | Experience-Eintrag nachvollziehbar widerrufen. | Alle vier Hauptoptionen Pflicht. |
| `experience replace` | `bruce experience replace --store STORE --id ID --reason TEXT --from-run RUN --output NEW [Transfersteuerung]` | Eintrag mit neuer Quelle ersetzen. | Alle Hauptoptionen Pflicht; Ausgabedatei darf nicht existieren. |
| `experience plan-add` | `bruce experience plan-add --from-run RUN [--store STORE] [--max-source-bytes N] [--max-store-bytes N] --output PLAN [Transfersteuerung]` | Einen expliziten Übergabeplan erzeugen. | Keinen Snapshot ändern. |
| `experience apply-add` | `bruce experience apply-add PLAN --from-run RUN [--store STORE] --output NEW [Transfersteuerung]` | Einen vorher erzeugten Plan anwenden. | `PLAN` ist Positionsargument. |
| `experience inspect` | `bruce experience inspect SNAPSHOT` | Snapshot prüfen und anzeigen. | Kein allgemeines `replay`. |
| `compatibility` | `bruce compatibility (--rules FILE|--rule-store STORE) (--candidates CSV|--from-run RUN --at X) [Budgets] [Sitzungsoptionen] [--output RUN]` | Kandidaten nach lokalen Regeln auswählen. | Jeweils genau eine Regel- und eine Quellenform. |
| `compatibility-plan` | `bruce compatibility-plan --from-run RUN (--rules FILE|--rule-store STORE) --at X --output PLAN [Budgets] [Transfersteuerung]` | Prüfbaren Übergabeplan von Forschung zur Z-Auswahl erzeugen. | Quelle, Regeln/Store, Stelle und Ausgabe erforderlich. |
| `compatibility-apply` | `bruce compatibility-apply PLAN --from-run RUN (--rules FILE|--rule-store STORE) [--output RUN] [Transfersteuerung]` | Einen Kompatibilitätsplan anwenden. | `PLAN` ist Positionsargument. |

### Laufprüfung

| Pfad | Syntax | Verwendung |
| --- | --- | --- |
| `inspect-run` | `bruce inspect-run RUN.bruce` | Metadaten und Ergebnis eines allgemeinen Bundles darstellen. |
| `assess-run` | `bruce assess-run RUN.bruce` | Einen Lauf nach den eingebauten Gate-Regeln bewerten. |
| `replay` | `bruce replay RUN.bruce` | Ein allgemeines BRUCE-Anwendungsbundle semantisch wiederholen. Nicht für Language-, NanoCell- oder Experience-Dateien. |
| `gate-run` | `bruce gate-run RUN.bruce [--max-session-checkpoints N] [--max-replay-bytes N] [--cancellation none|checkpoints]` | Einen konkreten Lauf mit optionaler Checkpoint-/Replaybyte-Grenze für ein Gate prüfen. |

## Formate und sichere Verwendung

- Forschungsdaten: CSV mit exaktem Header `x,y`; 1..4096 Paare, Training/Holdout ohne doppelte X-Werte.
- Ganze Zahlen sind dezimal. `rational` erlaubt zusätzlich `Zähler/Nenner`; NanoCell-`--core` ist die Ausnahme und erwartet exakt 16 Hexziffern.
- `compare` nutzt kommagetrennte rationale Koeffizienten in aufsteigender Potenzreihenfolge.
- Bundles und Snapshots sind versionierte, digestgebundene Formate, keine JSON-Dateien. Nicht umformatieren oder Zeilenenden verändern.
- Beschränke sensible NanoCell-Kontexte: Das Bundle enthält Kontextbytes.

## Prüfung dieser Referenz

Prüfschritte für Aktualisierungen:

```bash
./build/bruce --version
./build/bruce --help
python3 -m json.tool docs/cli/bruce-cli-reference.json >/dev/null
```

Danach jede Befehlszeile gegen die Whitelists in `apps/bruce_cli.cpp`, `apps/nanocell_cli.hpp`, `apps/rule_store.hpp` und `apps/rule_review.hpp` abgleichen. Für detaillierte Dateiformate, Ergebnisfelder und Diagnosemeldungen bleiben [formats.md](formats.md), [results-and-control.md](results-and-control.md) und [source-index.md](source-index.md) die ergänzenden Detailquellen.
