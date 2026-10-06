# BRUCE_raw

> **Historischer Bestand.** Dies ist der frühere Root-Einstieg, unverändert
> erhalten, damit frühere Verweise und Entwicklungsschritte nachvollziehbar
> bleiben. Für den aktuellen Einstieg gelten [README.md](README.md) und
> [docs/README.md](docs/README.md). Der darin beschriebene lokale semantische
> Python-Index ist nicht mehr Teil des aktiven Repository-Bestands; sein
> vollständiger, lokaler Archivstand liegt unter `runs/retired-semantic-index/`.

**[Aktueller Blueprint und nächste Architekturabnahmen](docs/architecture/03-blueprint-consolidation.md)** – Bestand, Modulabweichungen und geplante Schritte nach Core 0.81.

BRUCE_raw ist die Basis fuer eine integrierte mathematische Forschungs- und Loesungsmaschine. Der C++-Kern verarbeitet neue Zahlenpaare dynamisch, untersucht begrenzte affine Formeln, Potenzformeln, Polynome und rationale Bruchfunktionen und liefert exakte Pruefung, Erklaerung und Replay. Eine fest gespeicherte Aufgaben-/Antwortdatenbank ist nicht erforderlich.

## Start

### Lokaler semantischer CLI-Index (Python 3.11+)

Der optionale, rein lokale Index liest die [strukturierte CLI-Referenz](docs/cli/bruce-cli-reference.json)
und das [Markdown-Handbuch](docs/cli/bruce-cli-reference.md). Er startet **niemals** `bruce` und führt
auch vorgeschlagene Befehle nicht aus. Für Embeddings muss Ollama bereits lokal laufen und das
gewählte Modell installiert sein; der Indexer lädt weder Modelle noch Daten aus einer Cloud.

```bash
python3 -m bruce_semantic_index build-index \
  --md docs/cli/bruce-cli-reference.md \
  --ref docs/cli/bruce-cli-reference.json \
  --out /tmp/bruce-index.json
python3 -m bruce_semantic_index search --index /tmp/bruce-index.json --query 'Regeln prüfen' --top-k 5
python3 -m bruce_semantic_index validate-command \
  --ref docs/cli/bruce-cli-reference.json --command 'bruce help'
```

`build-index` verwendet standardmäßig `--format json`. Optional sind `--format jsonl` und
`--format sqlite`: JSONL enthält zuerst eine Metadatenzeile mit `record_type: "metadata"`, danach
eine Zeile pro Item; SQLite enthält die Tabellen `metadata`, `items` und `embeddings`. Jeder Export
wird nach dem Schreiben wieder eingelesen und geprüft. **`search` liest derzeit nur JSON-Indizes**;
JSONL und SQLite sind Export-/Austauschformate, keine Such-Eingaben. Ein neuer SQLite-Export braucht
einen noch nicht vorhandenen Dateipfad.

Der JSON-Index enthält `schema_version`, `created_at`, `source_files` (Pfade und SHA-256),
`ollama`, `bruce_reference`, `dimensions`, `validation` und `items`. Jedes Item enthält stabile
`id`, `kind` (`document`, `paragraph`, `sentence`, `token`, `command`, `option`), Text, Sprachhinweis,
Quellenbezug, optionalen Befehlspfad/Optionsnamen, Metadaten und einen normierten Vektor.
Überschreitet ein Item das Kontextfenster des lokalen Ollama-Modells, wird nur dieser Text
deterministisch in kleinere Teile zerlegt; der normierte Mittelwert der Teilvektoren bleibt
ein einzelner Item-Vektor (auch für das Dokument-Item). Heben sich die Teilvektoren im
Mittel exakt auf, wird deterministisch der erste normierte Teilvektor verwendet.
Suchergebnisse enthalten `recommendation_only`, Score, Befehlspfad, dokumentierte Syntax,
Kurzbegründung und explizit dokumentierte Optionen. Sie **beweisen keine gültige Syntax**.
Markdown-Zeilen und -Abschnitte werden bekannten Befehlspfaden zugeordnet; mehrere Bezüge
stehen in `metadata.command_paths`. Dokument- und allgemeiner Kontext bleiben als solcher
markiert (`metadata.scope`). `evidence` zeigt den besten Treffer jeder Item-Art mit Quelle,
Textauszug und Kosinus-Score. Der Rangscore ist das Maximum der Belege; breiter Dokument-/
Allgemeinkontext wird mit `-1 + 0.25 * (score + 1)` abgeschwächt. Markdown-Belege und die
zusätzlichen Hilfeformen `bruce`, `bruce help`, `bruce --help` sind mit
`source_authority: "markdown_supplemental"` gekennzeichnet. Sie ergänzen keine Validatorregeln
und keine Optionslisten. Bestehende Indizes neu bauen, um diese Markdown-Zuordnungen zu erhalten.

Das Standardmodell ist `all-minilm:22m`, die Standardadresse `http://localhost:11434`.
`--model`/`--base-url` haben Vorrang vor einer lokalen JSON-Datei mit `--config`, danach folgen
`BRUCE_SEMANTIC_INDEX_MODEL`/`BRUCE_SEMANTIC_INDEX_BASE_URL`, dann die Standards. Die Adresse muss
ein lokaler HTTP-Endpunkt sein. Umgebungs-Proxys werden ignoriert und HTTP-Weiterleitungen
abgewiesen. Ein fehlendes Modell muss manuell installiert werden; eine
entsprechende Fehlermeldung nennt den erforderlichen Ollama-Befehl, führt ihn aber nicht aus.
Für sinnvolle Suchscores dieselbe Embedding-Modellkonfiguration beim Indexieren und Suchen verwenden.

`validate-command` liefert **ausschließlich ein JSON-Objekt auf stdout**: `status` ist `valid`,
`invalid`, `partial` oder bei Eingabefehlern `error`; dazu kommen `command_path`, `errors`,
`warnings` und `unchecked_constraints`. `--command` ist die benannte Eingabe für den zu prüfenden
Befehl; die bisherige positionale Form bleibt kompatibel. Beide Formen gleichzeitig sind ein
Eingabefehler. Die CLI-Hilfe-Flags `-h`/`--help` für diese Operation
sind Eingabefehler in JSON und keine Textausgabe. `partial` bedeutet, dass nicht alle Regeln aus der
strukturierten Referenz beweisbar sind, **nicht** dass BRUCE den Befehl akzeptiert. Die bekannte
Abweichung zwischen Markdown und JSON sowie freie, dateisystembezogene oder aktionsspezifische
Regeln werden nicht stillschweigend ergänzt. `build-index` und `search` schreiben je ein
JSON-Ergebnis auf stdout; ihre Fehler gehen auf stderr. Exit-Code `0` bedeutet nur, dass dieses
Werkzeug erfolgreich verarbeitet hat (einschließlich `partial`); `2` steht für ungültige Eingabe,
Validierungsfehler oder Werkzeugfehler. Kein Exit-Code ist ein BRUCE-Ausführungsergebnis.

Offline-Tests: `python3 -m unittest discover -v`; dafür werden weder Ollama noch BRUCE benötigt.

**[Ausführliches CLI-Handbuch für Core 0.102.0](docs/cli/README.md)** – alle Befehle und Optionen, Dateiformate, Ergebnisstatus, Budgets, Abbruch und ausführbare Beispiele.

Aktueller Stand: Der NanoCell-V1-Kern und die isolierten CLI-Befehle [`nanocell run`/`replay`](docs/cli/commands.md#nanocell--isolierter-bytekern) mit drei festen Techniken sind implementiert. Ebenso gibt es einen deterministischen [Sprachpfad](docs/cli/commands.md#language--expliziter-sprachpfad) für zwei begrenzte Satzmuster. Freie Sprache, NanoCell-Einbindung in den allgemeinen Aufgabenlauf, Cockpit, Phase-DAG und GPU sind nicht implementiert.

Neu in Core 0.88: [Gemeinsamer Einstieg auch für Experience-Inspektion](docs/implementation/168-experience-inspection-entry-usage.md).

Seit Core 0.87: [JSON-Helfer im Core](docs/implementation/167-json-core-boundary-usage.md).

Seit Core 0.86: [Eigener Erfahrungsquellenaudit](docs/implementation/166-experience-source-boundary-usage.md).

Seit Core 0.85: [Gateentscheidung unabhängig von JSON](docs/implementation/165-gate-render-boundary-usage.md).

Seit Core 0.84: [Erfahrungsrouting in der Anwendungsschicht](docs/implementation/164-experience-routing-boundary-usage.md).

Seit Core 0.83: [Experience-Headergrenze](docs/implementation/163-experience-header-boundary-usage.md).

Seit Core 0.82: [Gemeinsamer Anwendungseinstieg für Erfahrungsaktionen](docs/implementation/162-experience-application-usage.md).

Seit Core 0.81: [Replaysteuerung einzelner Grid-Läufe](docs/implementation/161-single-grid-replay-control-usage.md).

Seit Core 0.80: [Signalabbruch für gate-run](docs/implementation/160-gate-signal-control-usage.md).

Seit Core 0.79: [Eingabegrenze für gate-run](docs/implementation/159-gate-input-budget-usage.md).

Seit Core 0.78: [CLI-Prüfpunktbudget für gate-run](docs/implementation/158-gate-budget-usage.md).

Seit Core 0.77: [Steuerbare Zulassungsprüfung](docs/implementation/157-gate-checkpoints-usage.md) verbindet Gateprüfung und Erfahrungsquellenaudit mit optionalen Prüfpunkten.

Seit Core 0.76: [Strukturierter Zulassungsbericht](docs/implementation/156-typed-gate-report-usage.md) trennt Prüfung und Darstellung für C++-Verbraucher.

Seit Core 0.75: [Replaygeprüfte Zulassungsinspektion mit `gate-run`](docs/implementation/155-run-gates-usage.md).

Seit Core 0.74: [Begründete Forschungszulassung und `gate-contracts`](docs/implementation/154-research-gate-reasons-usage.md) ergänzen die gemeinsamen Auswahl- und Erfahrungsregeln.

Seit Core 0.73: [Gemeinsamer Ergebnisvertrag und `assess-run`](docs/implementation/153-result-assessment-usage.md) trennen Vollständigkeit, Samplebefund und begrenzte Beweisaussage für alle acht Auftragstypen.

**[Benchmark-Suite V1 und erste Release-Ergebnisse](docs/implementation/152-benchmark-v1-results.md)** – reproduzierbare CLI-Messungen mit unabhängigen Ergebnisprüfungen und dokumentierten Grenzen.

```bash
cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
python3 scripts/demo.py
```

Voraussetzungen: Linux, C++20 mit GCC/Clang, CMake, Ninja, OpenSSL- und GMP/GMPXX-Entwicklungsbibliotheken, pkg-config; Python 3 fuer Tests und Demo.

[Benutzung, eigene Eingaben und genaue Funktionsgrenzen](docs/implementation/06-core-v1-usage.md)

Neu in Core 0.2: [eigene Regeln, 64-Bit-Filtergruppen und Forschungsimport](docs/implementation/08-z-compatibility-usage.md).

Neu in Core 0.3: [große Ganzzahlen, exakte Brüche und rationale Forschung](docs/implementation/10-exact-domains-usage.md).

Neu in Core 0.4: [nichtlineare Formeln mit selbst untersuchtem Exponenten](docs/implementation/12-power-research-usage.md).

Neu in Core 0.5: [datenabhängige Koeffizientensuche mit gemessenem Arbeitsvergleich](docs/implementation/14-adaptive-search-usage.md).

Neu in Core 0.6: [mehrtermige Polynome bis Grad 8](docs/implementation/16-polynomial-research-usage.md).

Neu in Core 0.7: [gemeinsame Strategieplanung mit geteilten Budgets und Formelherkunft](docs/implementation/18-strategy-portfolio-usage.md).

```bash
./build/bruce compatibility --rules examples/compatibility.rules \
  --candidates examples/compatibility-candidates.csv
```

Neu in Core 0.8: [Bruchfunktionen mit exakter Ableitung und expliziten Definitionslücken](docs/implementation/20-rational-linear-usage.md).

Neu in Core 0.9: [gemeinsame Forschung mit Potenzen, Polynomen und Bruchfunktionen](docs/implementation/22-rational-portfolio-usage.md).

Neu in Core 0.10: [datenabhängige Strategiereihenfolge mit nachvollziehbaren Entscheidungen](docs/implementation/24-adaptive-portfolio-usage.md).

Neu in Core 0.11: [optionale Erfahrung für spätere Suchreihenfolgen, mit Widerruf und Replay](docs/implementation/26-experience-routing-usage.md).

Neu in Core 0.12: [quadratische Nenner, exakte Definitionsbedingungen und Gegenprüfung](docs/implementation/28-rational-quadratic-usage.md).

Neu in Core 0.13: [vier Formelfamilien gemeinsam, mit datenabhängiger Reihenfolge und Erfahrung](docs/implementation/30-quadratic-portfolio-usage.md).

Neu in Core 0.14: [kleine Ausdrücke aus Rechenoperationen zusammensetzen und exakt prüfen](docs/implementation/32-expression-synthesis-usage.md).

Neu in Core 0.15: [gleiche rationale Ausdrücke zusammenführen und Definitionslücken erhalten](docs/implementation/34-expression-equivalence-usage.md).

Neu in Core 0.16: [fünf mathematische Suchwege gemeinsam ausführen und gleiche Funde zusammenführen](docs/implementation/36-expression-portfolio-usage.md).

Neu in Core 0.17: [fünf Suchwege mit Formhinweisen und geprüfter Erfahrung ordnen](docs/implementation/38-five-experience-usage.md).

Neu in Core 0.18: [Ausdrucksbäume über einen kleinen Grammatikindex erzeugen](docs/implementation/40-indexed-expression-usage.md).

Neu in Core 0.19: [den Grammatikindex im Fünferportfolio und mit Erfahrung nutzen](docs/implementation/42-indexed-portfolio-usage.md).

Neu in Core 0.20: [Rechenarten auswählen und den Ausdruckssuchraum gezielt begrenzen](docs/implementation/44-expression-operators-usage.md).

Neu in Core 0.21: [Operatorauswahl im Fünferportfolio und in der Erfahrungsplanung](docs/implementation/46-portfolio-operators-usage.md).

Neu in Core 0.22: [begrenzte Potenzen direkt in Ausdrucksbäumen verwenden](docs/implementation/48-expression-powers-usage.md).

Neu in Core 0.23: [Potenzausdrücke im Portfolio und mit geprüfter Erfahrung nutzen](docs/implementation/50-portfolio-powers-usage.md).

Neu in Core 0.24: [wiederkehrende kleine Teilbäume begrenzt zwischenspeichern](docs/implementation/52-cached-expression-usage.md).

Neu in Core 0.25: [Teilbaumcache im Portfolio und mit Erfahrung nutzen](docs/implementation/54-cached-portfolio-usage.md).

Neu in Core 0.26: [Polynombrüche mit ihren rationalen Definitionsbereichen exakt vergleichen](docs/implementation/56-fraction-comparison-usage.md).

Neu in Core 0.27: [rationale Definitionsbereiche direkt in der Ausdruckssuche vergleichen](docs/implementation/58-q-expression-usage.md).

Neu in Core 0.28: [rationale Definitionsbereiche im Portfolio und mit passender Erfahrung nutzen](docs/implementation/60-q-portfolio-usage.md).

Neu in Core 0.29: [höhere Nennergrade mit exaktem Primzahlzertifikat vergleichen](docs/implementation/62-modular-comparison-usage.md).

Neu in Core 0.30: [Primzahlbeweise in Ausdruckssuche, Portfolio und Erfahrungsplanung nutzen](docs/implementation/64-modular-research-usage.md).

Neu in Core 0.31: [bewiesene Bereichsgleichheiten in der Ausdruckssuche wiederverwenden](docs/implementation/66-domain-cache-usage.md).

Neu in Core 0.32: [Bereichsbeweise im Portfolio und in der Erfahrungsplanung wiederverwenden](docs/implementation/68-domain-cache-portfolio-usage.md).

Neu in Core 0.33: [rationale Definitionslücken höhergradiger Brüche mit exakten Zeugen finden](docs/implementation/70-rational-witness-usage.md).

Neu in Core 0.34: [rationale Gegenbeispiele während der Ausdruckssuche prüfen](docs/implementation/72-witness-expression-usage.md).

Neu in Core 0.35: [Gegenbeispiele im Portfolio und Erfahrungsplanung](docs/implementation/74-witness-portfolio-usage.md).

[Aktueller Architekturstand und offene Meilensteinabnahmen](docs/implementation/75-current-architecture-and-milestones.md).

Neu in Core 0.36: [Grid-Aufträge mit ihren Parametern exportieren und replayen](docs/implementation/77-grid-replay-usage.md).

Neu in Core 0.37: [GridSpec und begrenzter Cache für wiederholte Grid-Abfragen](docs/implementation/79-grid-cache-usage.md).

Neu in Core 0.38: [Grid-Werte mit vollständiger Herkunft in der Formelforschung verwenden](docs/implementation/81-grid-research-usage.md).

Neu in Core 0.39: [Basis oder Exponent als Forschungseingabe auswählen](docs/implementation/83-grid-axis-usage.md).

Neu in Core 0.40: [Grid-Import und Forschung gemeinsam logisch begrenzen](docs/implementation/85-grid-budget-usage.md).

Neu in Core 0.41: [Grid-Stapel begrenzen, abbrechen und Teilbefunde replayen](docs/implementation/87-grid-partial-usage.md).

## Leitidee

BRUCE verbindet austauschbare mathematische Strategien mit strengen Grenzen: Ein Kandidat ist keine Loesung. Erst exakte Ausfuehrung, unabhaengige Gegenpruefung, vollstaendige Versionen und Replay duerfen ein Ergebnis als verifiziert markieren.

```text
Eingabe -> Koordinaten und z-Adresse -> Kompatibilitaet -> Strategie
        -> Gegenpruefung -> Evidenz und Replay -> Erklaerung
```

## Dokumentationslandkarte

- [Konsolidierte Entwicklungsroadmap](docs/implementation/04-roadmap.md) – Prioritäten, Designkonflikte, Meilensteine und überprüfbare Abnahmen.
- [Dokumentationsindex](docs/README.md)
- [Produktidentitaet und Invarianten](docs/architecture/01-identity-and-invariants.md)
- [Gesamtarchitektur](docs/architecture/02-system-architecture.md)
- [Z-Kompatibilitaetsraum](docs/contracts/01-z-compatibility.md)
- [Token, Sprache und Relationen](docs/contracts/02-token-language-relations.md)
- [Evidenz und Experience Store](docs/contracts/03-evidence-and-experience.md)
- [Mathematische Forschung und Replay](docs/contracts/04-math-evidence-replay.md)
- [Teststrategie](docs/implementation/01-test-strategy.md)
- [Umsetzungsphasen](docs/implementation/02-implementation-phases.md)
- [Offene Entscheidungen](docs/implementation/03-open-decisions.md)

## Status

**Implementiert:** CLI, gepruefte 64-Bit-Koordinaten/Arithmetik, lokale Lazy-Grid-Feldanfrage, dynamische ganzzahlige affine Suche, getrennte Lern-/Hold-out-Pruefung, Budgetgrenzen, logischer Trace und optionaler Replay-Export.

**Zusätzlich implementiert in Core 0.2:** numerische Z-Kompatibilität mit dynamischen Regelsnapshots, getrennten 64-Bit-Maskengruppen, regelgebundener Erklärung und Replay; bestehende Forschungsexporte werden als Kandidatenquelle tatsächlich verarbeitet.

**Zusätzlich implementiert in Core 0.3:** begrenzte BigInt-/Brucharithmetik mit GMP, affine Forschung mit großen und rationalen Koeffizienten, Gegenprüfung und eigenem Replayformat. Historische Exporte bleiben unterstützt.

**Zusätzlich implementiert in Core 0.4:** begrenzte Suche nach `a*x^p+b`, Exponenten 1–8, getrennte exakte Gegenrechnung, Hold-out, Replay und tatsächlich potenzierende Z-Projektion. Gleiche Beispiele können weiterhin mehrere Formeln zulassen.

**Zusätzlich implementiert in Core 0.5:** optionaler adaptiver Planer für diese Familie. Er leitet Koeffizienten aus Lernpaaren ab oder fällt auf vollständige Suche zurück; Rechenarbeit, Entscheidungen und Abbruchposition bleiben budgetiert und replaybar.

**Zusätzlich implementiert in Core 0.6:** begrenzte mehrtermige Polynomforschung mit exakter Newton-Ableitung, Horner-/Potenzsummenprüfung, Hold-out, Replay und Z-Import. Fehlende Lernpaare für den Maximalgrad ergeben einen unvollständigen Befund. Interpolation ist kein allgemeiner Beweis.

**Zusätzlich implementiert in Core 0.7:** gemeinsamer Lauf aus adaptiver Potenzsuche und Polynomforschung, mit Restbudgets, kanonischer Formelzusammenführung und sichtbarer Herkunft. Teilabbrüche bleiben im Gesamtstatus unvollständig; gleiche Daten erzeugen keine zusätzlichen unabhängigen Belege.

**Zusätzlich implementiert in Core 0.8:** die rationale Familie `(a*x+b)/(x+c)` mit exakter Gleichungsableitung, vollständiger begrenzter Suche bei Rangmangel, separater Gegenrechnung, expliziten Nennernullstellen, Hold-out, Replay und Z-Projektion.

**Zusätzlich implementiert in Core 0.9:** das Dreistrategien-Portfolio mit gemeinsamen Budgets, erhaltenen Definitionslücken, Herkunft und versioniertem Replay. Gleichwertige Polynome werden zusammengeführt; Bruchfunktionen mit anderen Definitionsbereichen bleiben getrennt.

**Zusätzlich implementiert in Core 0.10:** ein optionaler Planer für die Reihenfolge der drei Familien anhand eines kurzen Lernpräfixes. Kosten, Entscheidungen und globale Abbruchpositionen sind replaybar; frühe Treffer bleiben bei offenen Familien Teilbefunde.

**Zusätzlich implementiert in Core 0.11:** begrenzter Routing-Experience-Store mit unveränderlichen Snapshots, replay-geprüften Quellen, Stimmenranking und Widerrufen. Er beeinflusst nur die Suchreihenfolge; neue Aufgaben werden erneut geprüft.

**Zusätzlich implementiert in Core 0.12:** die eigenständige Familie `(a*x+b)/(x²+c*x+d)` mit exakter Vierkoeffizientenableitung, begrenzter Suche bei Rangmangel, erhaltener Nennerbedingung, Hold-out, Replay und Z-Projektion. Ihre Portfolio-/Experience-Einbindung bleibt offen.

**Zusätzlich implementiert in Core 0.13:** Viererportfolio mit quadratischen Nennern, gemeinsamer Budgetierung, erhaltener Nenneridentität, datenabhängiger Reihenfolge und Erfahrungsstimmen. Alte und neue Erfahrungsquellen können gemeinsam gespeichert werden; ihr Planungseinfluss bleibt auf die passende Familienmenge begrenzt.

**Zusätzlich implementiert in Core 0.14:** eigenständige Ausdruckssynthese mit `x`, rationalen Konstanten und den vier Grundrechenarten; bis neun Baumknoten, lazy Enumeration, separate Ganzzahl-Gegenrechnung, erhaltene Divisionsbedingungen, Hold-out, Replay und Z-Projektion. Keine allgemeine algebraische Gleichheitsprüfung.

**Zusätzlich implementiert in Core 0.15:** optionale exakte Normalformen für die Ausdruckssynthese, erhaltene Ausschlusspolynome, begrenzte Herkunftslisten und versionierter Replay. Gleiche Bruchformen mit gleicher Ausschlussbedingung bilden eine Gruppe; die Originalbäume bleiben erhalten.

**Zusätzlich implementiert in Core 0.16:** Fünferportfolio einschließlich Ausdruckssynthese, gemeinsame Grenzen, familienübergreifende Normalformen mit erhaltenen Ausschlüssen und vollständiger Herkunft, V8-Replay und Z-Projektion. Adaptive Reihenfolge und Experience für diese Familienmenge bleiben offen.

**Zusätzlich implementiert in Core 0.17:** adaptive und erfahrungsbasierte Planung des Fünferportfolios, kontextgebundene Stimmen einschließlich Ausdruckssuche, gemischte historische Erfahrungsspeicher und Widerruf, stabile Gruppenidentitäten sowie V9/V10-Replay mit eingebettetem Speicherstand.

**Zusätzlich implementiert in Core 0.18:** optionaler indizierter Ausdrucksplaner mit unveränderten Kandidaten und Prüfungen, begrenzten Rangtabellen, budgetiertem Aufbau und V3-Replay. Der Referenzlauf benötigt 12221 logische Operationseinheiten weniger; die Portfolio-Einbindung dieses Planers bleibt offen.

**Zusätzlich implementiert in Core 0.19:** optionaler Grammatikindex im festen, adaptiven und erfahrungsbasierten Fünferportfolio; getrennte Erfahrungskontexte, gemischte V4-Speicher mit Widerruf, V11–V13-Replay und Z-Projektion. Der feste Referenzlauf benötigt 93523 statt 105744 logische Einheiten bei gleichen Befunden.

**Zusätzlich implementiert in Core 0.20:** explizite Operatorauswahl für die eigenständige Ausdruckssuche, beide Planer und Gleichheitsmodi, grammatikgebundene Kandidaten-IDs, V4-Replay und Z-Projektion. Im Beispiel sinkt der gewählte Raum von 184 auf 32 Ausdrücke; der Befund gilt nur für die deklarierte Grammatik.

**Zusätzlich implementiert in Core 0.21:** explizite Operatorauswahl im festen, adaptiven und erfahrungsbasierten Fünferportfolio; andere Familien bleiben unverändert, Erfahrungskontexte trennen Grammatik und Ausdrucksplaner. Gemischte V5-Speicher, Widerruf, V14–V16-Replay und Z-Projektion sind integriert. Beispielarbeit: 5431 → 1822 logische Einheiten.

**Zusätzlich implementiert in Core 0.22:** optionaler Potenzoperator mit literalen Exponenten 2–8 in der eigenständigen Ausdruckssuche; beide Planer, getrennte Gegenrechnung, erhaltene Definitionslücken, V5-Replay und Z-Projektion. Das Beispiel x³+x benötigt fünf statt sieben Baumknoten. Portfolio-/Experience-Einbindung bleibt offen.

**Zusätzlich implementiert in Core 0.23:** literale Potenzen im festen, adaptiven und erfahrungsbasierten Fünferportfolio; beide Ausdrucksplaner, getrennte Erfahrungskontexte, gemischte V6-Speicher, Widerruf, V17–V19-Replay und Z-Projektion. Beispielarbeit: 62278 → 3717 logische Einheiten bei unterschiedlichen Ausdrucksgrammatiken und Knotengrenzen.

**Zusätzlich implementiert in Core 0.24:** optionaler Teilbaumcache in der eigenständigen Ausdruckssuche, begrenzt auf 64 Dreiknotenbäume, mit unveränderten Kandidaten und mathematischen Prüfungen, transparenten Kosten/Zählern, V6-Replay und Z-Projektion. Beispielarbeit: 93297 → 86860 logische Einheiten. Portfolio-/Experience-Einbindung bleibt offen.

**Zusätzlich implementiert in Core 0.25:** optionaler Teilbaumcache im festen, adaptiven und erfahrungsbasierten Fünferportfolio; getrennte Erfahrungskontexte, gemischte V7-Speicher, Widerruf, V20–V22-Replay und Z-Projektion. Gleiche Beispielbefunde bei 87086 statt 93523 logischen Portfolioeinheiten.

**Weiterhin Konzept:** allgemeine Formelentdeckung, umfassendes Erfahrungslernen/externe Evidenzprüfung, freie Sprache, NanoCell-Einbindung in den allgemeinen Aufgabenlauf, Cockpit, Phase-DAG und GPU. Der NanoCell-Kern samt eigenem CLI-/Replaypfad und die begrenzte Sprachgrammatik sind dagegen implementiert. Der vollständige Experience-Vertrag ist damit nicht abgeschlossen.

## Nicht verhandelbare Grenzen

- Adresse, Wert, Bedeutung und Evidenz bleiben getrennt.
- Grosse Raeume werden lazy und nur innerhalb expliziter Budgets ausgewertet.
- Ein Sprachmodell kann vorschlagen und erklaeren, aber nie Wahrheit oder Gates setzen.
- BRUCE erzeugt keine Aussage ueber reale Private-Key-, BSGS-, Bitcoin- oder Mining-Beschleunigung.

Neu in Core 0.42: [Grid-Stapel mit geprüfter Herkunft fortsetzen](docs/implementation/89-grid-resume-usage.md).

Neu in Core 0.43: [Grid-Forschung als geprüfte Erfahrung verwenden](docs/implementation/91-grid-experience-usage.md).

Neu in Core 0.44: [Erfahrungen mit geprüften Nachfolgern korrigieren](docs/implementation/93-experience-replacement-usage.md).

Neu in Core 0.45: [Grid- und Experience-Import gemeinsam begrenzen](docs/implementation/95-grid-experience-budget-usage.md).

Neu in Core 0.46: [Gemeinsame Forschungs-, Strategie- und Gate-Verträge](docs/implementation/97-research-contract-usage.md).

Neu in Core 0.47: [Gemeinsamer typisierter Einstieg für acht Laufarten](docs/implementation/99-application-task-usage.md).

Neu in Core 0.48: [Laufbefunde einheitlich replayen und einordnen](docs/implementation/101-application-result-usage.md).

Neu in Core 0.49: [Optionale Ausführung mit gemeinsamer Phasen- und Replayprüfung](docs/implementation/103-execution-phases-usage.md).

Neu in Core 0.50: [Fehlgeschlagene Ausführungen strukturiert untersuchen](docs/implementation/105-execution-failure-usage.md).

Neu in Core 0.51: [Replay-Eingang mit optionalem Bytebudget begrenzen](docs/implementation/107-replay-byte-budget-usage.md).

Neu in Core 0.52: [Prüfsitzungen an Phasengrenzen abbrechen](docs/implementation/109-verification-cancellation-usage.md).

Neu in Core 0.53: [Grid-Replay innerhalb von Stapeln und Fortsetzungen abbrechen](docs/implementation/111-grid-replay-checkpoints-usage.md).

Neu in Core 0.54: [Forschungsreplay an Suchcheckpoints abbrechen](docs/implementation/113-research-replay-checkpoints-usage.md).

Neu in Core 0.55: [Zusammengesetzte Grid-Forschung im Replay abbrechen](docs/implementation/115-grid-research-replay-usage.md).

Neu in Core 0.56: [Bruchvergleichsreplay an Rechencheckpoints abbrechen](docs/implementation/117-comparison-replay-checkpoints-usage.md).

Neu in Core 0.57: [Z-Kompatibilitätsreplay intern abbrechen](docs/implementation/119-compatibility-replay-checkpoints-usage.md).

Neu in Core 0.58: [Erfahrungs-Quellaudits im Replay unterbrechen](docs/implementation/121-experience-replay-checkpoints-usage.md).

Neu in Core 0.59: [Äußere Bundle-Prüfsummen blockweise abbrechen](docs/implementation/123-serialization-digest-checkpoints-usage.md).

Neu in Core 0.60: [Grid-Evidenz zwischen Datensätzen abbrechen](docs/implementation/125-grid-serialization-records-usage.md).

Neu in Core 0.61: [Erfahrungs-Quellaudits im Erstlauf abbrechen](docs/implementation/127-execution-experience-checkpoints-usage.md).

Neu in Core 0.62: [Grid-Eingangsquellen im Erstlauf abbrechen](docs/implementation/129-execution-grid-source-checkpoints-usage.md).

Neu in Core 0.63: [Forschungsquellen der Z-Auswahl im Erstlauf abbrechen](docs/implementation/131-execution-compatibility-source-usage.md).

Neu in Core 0.64: [Native Fortsetzungsimporte abbrechen](docs/implementation/133-execution-resume-import-usage.md).

Neu in Core 0.65: [Gemeinsames Prüfpunktbudget über Ausführung, Importe, Serialisierung und Replay](docs/implementation/135-session-work-budget-usage.md). [Aktuelle M0–M2-Abnahmematrix](docs/implementation/136-m0-m2-acceptance-matrix.md).

Neu in Core 0.66: [Versionierte lokale Installation, Buildidentität und geprüfte CLI-Archive](docs/implementation/138-installation-usage.md).

Neu in Core 0.67: [Große Vorgänger bei Grid-Fortsetzungen blockweise kopieren](docs/implementation/140-resume-copy-blocks-usage.md).
Neu in Core 0.68: [Serialisierte Sitzungsdaten vor Replay begrenzen](docs/implementation/142-serialized-byte-budget-usage.md).

Neu in Core 0.69: [Verschachtelte GridResearch-Daten blockweise serialisieren](docs/implementation/144-grid-research-copy-usage.md).

Der aktuelle Architekturstand ist in [Core 0.69 Status](docs/implementation/145-core-069-status.md) zusammengefasst.

Neu in Core 0.70: Erfahrungsdaten in `ExactResearch` werden beim Serialisieren ebenfalls blockweise beobachtbar.

Neu in Core 0.71: Auch große `Compatibility`-Payloads nutzen diesen Blockpfad.

Neu in Core 0.72: `FractionComparison` nutzt ebenfalls blockweise Serialisierungskontrollen.
