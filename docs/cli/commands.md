# Befehlsreferenz – Core 0.102.0

[Zum Handbuch](README.md) · [Dateiformate](formats.md) · [Ergebnisse und Steuerung](results-and-control.md)

## Hilfe, Version und Vertragskataloge

| Aufruf | Wirkung |
| --- | --- |
| `bruce`, `bruce help`, `bruce --help` | Textuelle Gesamthilfe |
| `bruce --version` | `{"contract":"bruce-version-v1","version":"0.102.0"}`; keine weiteren Argumente |
| `bruce contracts` | Eingebaute Forschungsstrategien mit Standardplaner und zulässigen Planern; keine weiteren Argumente |
| `bruce gate-contracts` | Versionierter Forschungs-Gatevertrag mit Ablehnungsgründen und Priorität; keine Argumente, keine Prüfung eines konkreten Laufs |
| `bruce task-contracts` | Acht Anwendungstypen und Architekturgrenzen; keine weiteren Argumente |

`contracts` ist kein vollständiges JSON-Schema für Ergebnisse und enthält nicht alle numerischen Grenzwerte. `task-contracts` nennt `research-int64`, `research-exact`, `grid`, `grid-batch`, `grid-resume`, `grid-research`, `fraction-comparison`, `compatibility`. Es weist ausdrücklich auf fehlende dynamische Plugins und uneinheitliche mathematische Budgeteinheiten hin.

## `language` – optionale deterministische Language-Mod

```text
bruce language interpret --text TEXT --train TRAIN.csv --holdout HOLDOUT.csv
               [--max-bits N] [--max-denominator N] [--max-candidates N]
               [--max-operations N] [--max-trace N] [--max-results N] --output DRAFT
bruce language select DRAFT --alternative ID --output SELECTED_DRAFT
bruce language execute DRAFT --output LANGUAGE_RUN
bruce language replay LANGUAGE_RUN
```

`interpret` liest beide CSV-Dateien begrenzt ein und schreibt ausschließlich einen kanonischen Entwurf. `--text`, `--train`, `--holdout` und `--output` sind Pflicht. Die sechs optionalen Grenzen entsprechen der rationalen Core-Anfrage: `--max-bits` (Standard 4096), `--max-denominator` (8), `--max-candidates` (100000), `--max-operations` (1000000), `--max-trace` (100002) und `--max-results` (128). Für diese Optionen gelten die bestehenden Grenzen der exakten Forschung. Andere Forschungs-, Sitzungs-, Replay- oder Ausführungsoptionen sind im Sprachpfad nicht zulässig. `interpret` startet den Core nicht.

Die deterministische Mini-Grammatik erkennt genau zwei normalisierte Sätze: `Untersuche @train mit @holdout auf rationale Formeln.` und `Untersuche @left und @right auf rationale Formeln.` Die Eingabepfade heißen in beiden Fällen ausschließlich `--train` und `--holdout`. Beim ersten Satz werden ihre Bytes intern als `train`/`holdout` gebunden; beim zweiten als `left`/`right`, in derselben Reihenfolge. Die zweite Form liefert zwei Alternativen und erfordert `select`; `--alternative` nimmt exakt eine der ausgegebenen IDs. Nicht erkannter Text, ungültiges UTF-8 und eine fehlende Auswahl führen zu Exit 2 ohne neuen Export.

`select` nimmt einen Entwurf als einziges Positionsargument sowie `--alternative` und `--output`. `execute` nimmt einen kanonischen, gegebenenfalls ausgewählten Entwurf und `--output`. `replay` nimmt nur den Sprachlauf als Positionsargument. Der Befehl existiert nur mit `BRUCE_BUILD_MOD_LANGUAGE=ON`; mit `OFF` folgt die normale `UnknownCommand: language`-Route. Entwurf und Lauf werden vor Verwendung kanonisch geprüft; alle Exporte sind exklusiv und überschreiben vorhandene Ziele nicht.

Die Antworten sind `language-interpretation-v1`, `language-selection-v1`, `language-execution-v1` und `language-replay-v1`. Die ersten beiden zeigen den Status `UNDERSTOOD` oder `AMBIGUOUS`, Grammatikversion, Alternativen, gewählte ID und Eingabealiase mit Digests. `execute` exportiert einen `BRUCE_MOD_RUN_V1`-Umschlag mit Language-Mod-Manifest, kanonischem Entwurf und dem unveränderten inneren `BRUCE_LANGUAGE_RUN_V1`. `replay` akzeptiert sowohl diesen Umschlag als auch direkte historische V1-Läufe. Dessen `core` enthält die geprüfte native Sitzung und den Core-Befund einschließlich `INCOMPLETE`, wenn ein Budget erschöpft ist. Dieser Sprachpfad setzt keine mathematische Wahrheit oder Gates und ist kein allgemeiner Chat. [Format und Bindung](formats.md#sprachentwurf-und-sprachlauf).

## `task-spec` – explizit gebundene Token-Aufgabe V1

```text
bruce task-spec create --train TRAIN.csv --holdout HOLDOUT.csv
                       --route rational-portfolio-v1 --token-table-version v1 --output SPEC
                       [--min N] [--max N] [--max-bits N] [--max-denominator N]
                       [--max-power N] [--max-degree N] [--max-trace-bytes N]
                       [--max-candidates N] [--max-operations N] [--max-trace N] [--max-results N]
bruce task-spec inspect SPEC
bruce task-spec execute SPEC --output RUN
bruce task-spec replay RUN
```

`create` verlangt alle fünf angegebenen Optionen. V1 akzeptiert ausschließlich die Route `rational-portfolio-v1` mit Token-Tabelle `v1`. Die beiden CSV-Dateien müssen verschieden sein; ihre Originalbytes werden als Assets `train` und `holdout` mit SHA-256-Digests eingebettet. Eine V1-Datei enthält genau die Token `Train`, `Holdout` und `Rational Portfolio` in dieser Reihenfolge; ihre versionsgebundenen IDs und ihre Reihenfolge sind Teil des Vertrags. Die Rollen `train` und `holdout` sind ausdrücklich an diese IDs und die gleichnamigen Asset-Aliase `train` beziehungsweise `holdout` gebunden; `route` hat keinen Asset-Alias. Aus freiem Text werden weder Pfade noch Rollen erraten. `inspect` zeigt die Bindungen, geordneten Token-IDs, Asset-Digests, Route, Domäne, Grenzen und Budgets, ohne den Core auszuführen. Eventuelle Alternativen müssen vor der Ausführung eindeutig ausgewählt sein.

Die Standardwerte für `--min`, `--max`, `--max-bits`, `--max-denominator`, `--max-power`, `--max-degree`, `--max-trace-bytes`, `--max-candidates`, `--max-operations`, `--max-trace` und `--max-results` sind `-8`, `8`, `4096`, `8`, `4`, `3`, `1048576`, `100000`, `1000000`, `100002` und `128`. Die Grenzen steuern die native rationale Portfolio-Forschung; erschöpfte Budgets können `INCOMPLETE` ergeben. Ungültige Grenzen werden abgewiesen. `execute` akzeptiert nur einen kanonischen Spec und `--output`; zusätzliche Forschungs-, Erfahrungs-, Lern-, Publikations- oder Gateoptionen sind keine TaskSpec-Optionen.

Bereits vor dem Spec-Export und bei der Kompilierung gelten: 1..16384 Bits, 1..16 als Nennergrenze, jeweils 1..8 für Potenz und Grad, 256..4194304 Tracebytes, 1..1000000 Kandidaten, 1..10000000 Operationen, 2..100002 Traceereignisse und 1..128 Ergebnisse. Das ganzzahlige Intervall von `--min` bis `--max` muss 1..4096 Werte enthalten. Diese Prüfungen gelten auch für eingelesene kanonische Artefakte; eine andere Token-Tabellenversion als `v1` wird unabhängig vom Erzeuger abgewiesen.

`create`/`inspect` liefern `tokenized-task-spec-v1`. Die kanonische Binärdatei beginnt mit `BRUCE_TOKENIZED_TASK_SPEC_V1`, gefolgt von Body-SHA-256 und längenpräfixierten Feldern. `execute`/`replay` liefern `tokenized-task-run-v1`; die Laufdatei beginnt mit `BRUCE_TOKENIZED_TASK_RUN_V1` und bindet den vollständigen Spec, seinen Digest, das native Bundle samt Digest, Phasen, Erklärung und Core-Evidenz. Beide Exporte sind exklusiv; vorhandene Dateien werden nicht überschrieben. `replay` benötigt die ursprünglichen CSV-Pfade nicht. Es prüft Header, kanonische Bytes und Digests, kompiliert die gebundenen Rollen erneut und replayt das native Bundle. Ein nicht passender Spec, veränderte Bytes oder abweichende native Evidenz führen zu Exit 2 statt zu einer umgedeuteten Antwort.

Im JSON stehen die äußeren Phasen von `RECEIVED` bis `COMPLETED` und unter `core` die unveränderte native Sitzung. Zu den erreichbaren nativen Status gehören beispielsweise `VERIFIED_ON_SAMPLES`, `REFUTED`, `INCOMPLETE`, `NO_CANDIDATE` und `AMBIGUOUS`; dies ist keine abschließende Statusliste. `NO_CANDIDATE` bezeichnet eine vollständig untersuchte Familie ohne passenden Kandidaten, `AMBIGUOUS` mehrere nicht eindeutig entschiedene Kandidaten. `general_proof`, `learn_gate` und `publish_gate` bleiben `false`. V1 ist weder freie Sprache noch eine Wahrheitsmaschine und aktiviert weder Regeln noch Gates. Ein Sample-Befund ist kein allgemeiner Beweis.

Beispiel mit zwei verschiedenen CSV-Dateien im Format `x,y`:

```bash
bruce task-spec create --train identity.csv --holdout identity-hold.csv \
  --route rational-portfolio-v1 --token-table-version v1 --output identity.task-spec
bruce task-spec inspect identity.task-spec
bruce task-spec execute identity.task-spec --output identity.task-run
bruce task-spec replay identity.task-run
```

Mit den festen `identity.csv`-/`identity-hold.csv`-Eingaben der [Dokumentationsprüfung](verify_examples.py) und den Standardbudgets ergibt dieser Lauf exakt `INCOMPLETE` mit `stop_reason: "ChildIncomplete"`; das Replay bewahrt diesen Befund.

## `nanocell` – optionaler statischer NanoCell-Mod

```text
bruce nanocell run --core HEX16 --active-index 0..7 --context-id ID
                   --context-file FILE --technique TECHNIQUE
                   [--max-operations N] [--max-trace-entries N] [--output NEU]
bruce nanocell replay RUN
```

Der Befehl existiert nur mit `BRUCE_BUILD_MOD_NANOCELL=ON`; mit `OFF` ist er weder im Katalog noch in der Hilfe registriert und folgt der normalen `UnknownCommand: nanocell`-Route. `run` verlangt die fünf erstgenannten Optionen. `--core` sind genau 16 Hexziffern für acht Bytes, ohne `0x` oder Trenner; Groß- und Kleinbuchstaben sind erlaubt. Der Index wählt eines dieser Bytes. `--context-id` ist ein gültiger Bezeichner und `--context-file` liefert die tatsächlichen, höchstens 4096 Kontextbytes; die CLI berechnet ihren SHA-256-Digest. Für die Referenztechniken `masked-match-v1` und `rotate-match-v1` enthält die Datei exakt `nanocell-reference-context-v1` (ohne Zeilenumbruch). `context-conflict-v1` verlangt dagegen `nanocell-reference-context-conflict-v1`; mit dem ersten Kontext ergibt sie regulär `CONTEXT_REJECTED`.

`--technique` akzeptiert ausschließlich diese drei eingebauten IDs; eigene Programme oder frei formulierte Techniken akzeptiert die CLI nicht. `--max-operations` ist standardmäßig 16, `--max-trace-entries` 18. Beide sind begrenzte UInt64-Werte; die NanoCell-Kernvalidierung erzwingt 0..16 Operationen und 2..18 Traceeinträge. Ein Budgetende ergibt `INCOMPLETE` mit Exit 0, keine automatische Fortsetzung. `--output` schreibt ausschließlich einen äußeren `BRUCE_MOD_RUN_V1`-Umschlag mit Manifest `nanocell`/`nanocell-mod-v1`, kanonischer NanoCell-Anfrage und unverändertem inneren `BRUCE_NANOCELL_RUN_V1`; ohne Option bleibt nur die JSON-Ausgabe. `replay` akzeptiert genau einen Bundlepfad: Neue äußere Umschläge müssen Manifest, Codecs, Digests und die Anfrage-zu-Run-Bindung bestehen; historische direkte V1-Dateien bleiben ein read-only Kompatibilitätsweg. Keine allgemeinen Sitzungs-, Forschungs- oder Gateoptionen gelten hier; `MATCHED`, `NO_MATCH`, `CONTEXT_REJECTED` und `INCOMPLETE` bleiben ausschließlich technische NanoCell-Status. [Format](formats.md#nanocell-kontext-und-bundle) · [Status und Felder](results-and-control.md#nanocell-ergebnis-und-replay).

## `coordinate` – reversible Adressen

```text
bruce coordinate --x X --y Y
bruce coordinate --z Z
```

Genau eine der beiden Formen verwenden. X, Y und Z sind dezimale vorzeichenlose 64-Bit-Ganzzahlen. Negative Werte sind unzulässig. Vorwärts gilt `z=(x+y)*(x+y+1)/2+y`; ein nicht in UInt64 darstellbares Ergebnis führt zu `CoordinateOverflow`. Das ist eine Adresse, nicht der Grid-Wert `x^y`.

Ausgabe: `x`, `y`, `z` als Dezimalstrings. Sind beide Koordinaten kleiner als 8, kommt `local_z` als JSON-Zahl hinzu: `local_z=8*y+x`. Lokale und globale Adresse sind unterschiedliche Zuordnungen. `--z` erlaubt keine zusätzlichen Optionen; ansonsten sind `--x` und `--y` Pflicht.

## `calculate` – einzelne exakte Rechenoperation

```text
bruce calculate --op OP --left A --right B
                [--domain bigint|rational] [--max-bits N] [--modulus M]
```

| Option | Standard | Bedeutung / Grenze |
| --- | --- | --- |
| `--op` | Pflicht | `add`, `sub`, `mul`, `div`, `divmod`, `pow`, `modpow` |
| `--left`, `--right` | Pflicht | Zahlen der gewählten Domäne; rechts bei Potenzen UInt64-Exponent |
| `--domain` | `bigint` | `bigint` oder `rational`; kein `int64` |
| `--max-bits` | 4096 | 1..16384 Bits; Betrag des Zählers und Nenner getrennt begrenzt |
| `--modulus` | keiner | Positiver ganzzahliger Modul für `modpow`; bei anderen Operationen nicht verwenden |

`add/sub/mul/div` rechnen A+B, A−B, A×B bzw. A/B. BigInt-`div` verlangt Rest 0, sonst `NonIntegralDivision` mit Quotient und Rest. `divmod` ist nur für BigInt zulässig; es liefert A=B×q+r mit `0 <= r < abs(B)`, auch für negative Operanden. Division durch 0 scheitert.

`pow` verwendet einen nichtnegativen UInt64-Exponenten; negative und rationale Exponenten sind nicht unterstützt. `0^0` ergibt hier 1. `modpow` ist BigInt-only und berechnet `A^B mod M`; das Fehlen des Moduls scheitert beim Zahlenparser. Zwischenwerte und Resultate unterliegen den implementierten Domänenprüfungen, aber `--max-bits` ist kein Prozessspeicherlimit.

JSON: `domain`, `operation`, `max_bits` sowie `value` als String oder `quotient` und `remainder` als Strings. Dieser Befehl besitzt weder `--output` noch Replay-/Sitzungsoptionen.

## `grid` – einen Pfad auswerten

```text
bruce grid --path '2,6/4,3' [--mode exact|modular|metric]
           [--max-bits N] [--modulus M] [--zero-zero one|stop]
           [gemeinsame Sitzungsoptionen] [--output DATEI]
```

Ein Pfad besteht aus 1..64 Paaren `x,y`, getrennt durch `/`. Jede lokale Koordinate liegt in 0..7. Alle Schritte beschreiben die Adresse; ausgewertet wird ausschließlich das letzte Paar als `x^y`. `2,6/4,3` ergibt deshalb 64. Es ist kein Potenzturm und keine Auswertung aller `64^Tiefe` Knoten.

| Option | Standard | Bedeutung |
| --- | --- | --- |
| `--path` | Pflicht | Lokaler Pfad |
| `--mode` | `exact` | `exact`: UInt64-Potenz; `modular`: Rest; `metric`: exakte Bitlänge |
| `--max-bits` | 64 | 1..64; im Modus `exact` Ergebnisgrenze. In `metric` wird intern mit 64 Bits gerechnet, in `modular` begrenzt der Modul das Ergebnis; der Parameter wird trotzdem validiert |
| `--modulus` | keiner | In `modular` Pflicht, UInt64 >0; sonst verboten |
| `--zero-zero` | `one` | `one`: 0^0=1 (modular anschließend Rest); `stop`: letztes Paar 0,0 beendet nur diesen Zweig ohne Auswertung |

`metric` liefert `bit_length` und `metric_exact`, sonst erscheint `value`. Ein gestoppter Zweig liefert keines davon. `evaluations` ist 0 oder 1; `theoretical_space` ist eine symbolische Größenbeschreibung.

## `grid-batch` – mehrere Pfade und begrenzter Cache

```text
bruce grid-batch --paths '2,6;4,3;2,6'
                 [--cache-bytes N] [--max-query-steps N]
                 [Grid-Modi und Sitzungsoptionen] [--output DATEI]
```

`--paths` ersetzt `--path`. Es sind 1..1024 Pfade, getrennt durch `;`, mit insgesamt höchstens 262144 Bytes erlaubt. Leere Pfade sind ungültig. Alle verwenden dieselben Grid-Modi und Grenzen.

`--cache-bytes` ist 0..1048576, Standard 65536. 0 deaktiviert die Speicherung. Der Cache ist lokal zum Lauf, FIFO und auf höchstens 64 Einträge begrenzt. Gezählt werden kanonischer Schlüssel plus Ergebnisbundle, nicht gesamter RAM oder Verwaltungsobjekte. Gleicher Zahlenwert an anderer Adresse ist kein identischer Cacheeintrag.

`--max-query-steps` ist optional, 0..1024. Ohne Angabe werden alle angegebenen Pfade verarbeitet. 0 erzeugt einen unvollständigen Lauf ohne bearbeitete Anfrage. Auch ein Cachetreffer zählt als Anfrage. Ein Teilbefund ist exportierbar und mit `grid-resume` fortsetzbar.

## `grid-resume` – unvollständigen Stapel fortsetzen

```text
bruce grid-resume --from-run PARTIAL.bruce --additional-queries N
                 [Sitzungsoptionen] [--output NEU.bruce]
```

Beide fachlichen Optionen sind Pflicht. N liegt in 1..1024. Quelle muss ein replaybares, `INCOMPLETE`-Grid-Batch- oder Grid-Resume-Bundle sein. Abgeschlossene Quellen werden abgelehnt. Höchstens acht verschachtelte Fortsetzungsgenerationen sind erlaubt.

Die alten Konfigurationen, Reihenfolge und Cacheparameter werden übernommen; dafür gibt es hier keine Änderungsoptionen. Der geprüfte bisherige Ablauf wird rekonstruiert, danach werden weitere Anfragen verarbeitet. `audit_evaluations` und `new_evaluations` trennen Rekonstruktionsaufwand und neue Auswertungen. Es ist keine RAM-Abbild-Wiederherstellung. Mehr angeforderte Schritte als verbleibende Pfade sind möglich; der Lauf endet nach dem letzten Pfad.

## `research` – gemeinsame Optionen

```text
bruce research --train TRAIN.csv [--holdout HOLD.csv] [Optionen]
bruce research --grid-train TRAIN.bruce [--grid-holdout HOLD.bruce] [Optionen]
```

Die Quellenformen dürfen nicht gemischt werden. Ohne `--strategy` gilt `affine`. Ohne Domäne ist reine CSV-affine Forschung `int64`; die anderen Standards stehen in der Strategietabelle.

| Option | Standard | Zulässigkeit / Wirkung |
| --- | --- | --- |
| `--train` | Pflicht bei CSV | 1..4096 Lernpaare |
| `--holdout` | nicht gesetzt | Zusätzliche disjunkte Prüfdaten; wenn angegeben, Datei nicht leer |
| `--strategy` | `affine` | Eine der zehn Familien unten |
| `--domain` | abhängig | `int64`, `bigint`, `rational` |
| `--planner` | abhängig | Ausschließlich zur Familie passende Planer |
| `--min`, `--max` | −8, 8; Ausdrucksfamilien 0, 1 | Inklusive ganzzahlige Grenzen für Parameterzähler/Konstanten; keine Grenze für Sample-X |
| `--max-candidates` | 100000 | 1..1000000 gestartete Kandidaten |
| `--max-operations` | 1000000 | 1..10000000 logische Recheneinheiten der Strategie |
| `--max-trace` | 100002 | 2..100002 Traceereignisse |
| `--max-results` | 128 | Int64: 1..4096; exakte Forschung: 1..128; Expression/Portfolio-Expression mit `rational-q-witness`: 1..256 |
| `--max-bits` | 4096 | Nur BigInt/rational, 1..16384 |
| `--max-denominator` | rational 8; Ausdrucksfamilien und BigInt 1 | 1..16; BigInt verlangt 1 |
| `--max-trace-bytes` | 1048576 | Nur BigInt/rational; 256..4194304 |
| `--max-power` | 4 bei Power/Portfolios | 1..8; nur Power/Portfolios |
| `--max-degree` | 3 | 0..8; nur Polynomial/Portfolios |
| `--max-nodes` | 7 | 1, 3, 5, 7 oder 9; nur Expression/Portfolio-Expression |
| `--operators` | `+-*/` | Nichtleere Auswahl eindeutiger Zeichen aus `+-*/^`; nur Ausdrucksfamilien |
| `--equivalence` | `syntax` bei Expression; `rational` bei Portfolio-Expression | Siehe Äquivalenz unten |
| `--expression-planner` | `grammar` | Nur Portfolio-Expression; `grammar`, `indexed-grammar`, `cached-grammar` |
| `--domain-cache` | `none` | `none` oder `equal-64`; siehe unten |
| `--experience` | keine | Snapshot ausschließlich zusammen mit `experience-order` |
| `--output` | keine | Replaybares Bundle, neuer Pfad |

Int64 verlangt `-1000000 <= min <= max <= 1000000`. Bei exakten Domänen darf die inklusive Breite `max-min+1` nur 1..4096 sein; Grenzen selbst müssen in die gewählte Bitgrenze passen. Rationale Parameter entstehen aus Zählern in diesem Intervall und Nennern 1..`max-denominator`, anschließend gekürzt und dedupliziert. `--max-denominator` beschränkt den Parametersuchraum, nicht allgemein die Nenner der eingegebenen Samples.

Ausdrucksfamilien verlangen zusätzlich `Breite * max-denominator <= 64`, rational-quadratic und portfolio-quadratic höchstens 32768 rohe Parameterslots. Polynome werden aus Daten abgeleitet; `min/max` begrenzen dabei zulässige Koeffizienten. Mehr Budget erweitert nicht automatisch die deklarierte Familie.

## Forschung: Strategie-/Planermatrix

| Strategie | Familie | Domäne standardmäßig / zulässig | Standardplaner; weitere |
| --- | --- | --- | --- |
| `affine` | a*x+b | int64; bigint, rational (Grid standardmäßig bigint) | `enumerate` |
| `power-affine` | a*x^p+b, p=1..max-power | bigint; rational | `enumerate`; `adaptive` |
| `polynomial` | Summe c_i*x^i, Grad 0..max-degree | bigint; rational | `interpolate` |
| `rational-linear` | (a*x+b)/(x+c) | rational ausschließlich | `solve` |
| `rational-quadratic` | (a*x+b)/(x²+c*x+d) | rational ausschließlich | `solve` |
| `expression` | Begrenzte Bäume aus x, Konstanten, binären Operatoren | rational ausschließlich | `grammar`; `indexed-grammar`, `cached-grammar` |
| `portfolio` | Power + Polynomial | bigint; rational | `portfolio` |
| `portfolio-rational` | Vorige + Rational-Linear | rational ausschließlich | `portfolio`; `adaptive-order`, `experience-order` |
| `portfolio-quadratic` | Vorige + Rational-Quadratic | rational ausschließlich | `portfolio`; `adaptive-order`, `experience-order` |
| `portfolio-expression` | Vorige + Expression | rational ausschließlich | `portfolio`; `adaptive-order`, `experience-order` |

`adaptive` entscheidet bei Power zwischen Ableitung und Enumeration. `interpolate` untersucht begrenzte Polynomgrade; `solve` löst die begrenzten Bruchfamilien. Es sind keine freien Formelsyntheseverfahren. Portfolios teilen Suchbudgets unter ihren Strategien. `adaptive-order` nutzt versionierte Datenheuristiken; `experience-order` ergänzt passende geprüfte Stimmen aus einem Snapshot. Das ändert die Suchreihenfolge und kann bei gleichem endlichem Budget andere Teilbefunde liefern.

Bei Ausdrucksbäumen zählt jeder Operand und jeder binäre Operator als Knoten. `^` ist nur mit literalem ganzzahligem Exponenten 2..8 zulässig; `x^x`, freie rationale Exponenten und negative Exponenten gehören nicht zu dieser Grammatik. Der Exponent muss im Konstantenvorrat verfügbar sein. `--max-power` steuert bei einem Portfolio dessen Power-Familie, nicht diese Ausdrucksexponenten; bei eigenständigem `expression` ist die Option verboten.

### Ausdrucksäquivalenz

| Wert | Bedeutung |
| --- | --- |
| `syntax` | Eigenständige Ausdruckssuche ohne algebraische Zusammenführung; im Portfolio nicht erlaubt |
| `rational` | Rationale Normalformen unter Erhalt von Ausschlussbedingungen |
| `rational-q` | Zusätzlich begrenzter Vergleich der rationalen Definitionsbereiche |
| `rational-q-modular` | Zusätzliche modulare Zertifikate für Definitionsbereichsvergleiche |
| `rational-q-witness` | Zusätzlich begrenzte Suche nach rationalen Unterscheidungszeugen; Resultatbudget höchstens 256 |

Nicht entschiedene Definitionsbereichsvergleiche dürfen nicht als bewiesene Gleichheit gelesen werden. `--domain-cache equal-64` ist ausschließlich mit `rational-q-modular` in einer Ausdrucksfamilie erlaubt; er speichert bewiesene Gleichheiten, höchstens 64 Einträge. Selbst ausdrücklich gesetztes `--domain-cache none` ist bei anderen Strategien bereits vom CLI-Parser untersagt.

`--experience` muss zu `experience-order` passen und umgekehrt. Ein Snapshot mit nichtpassendem Kontext kann 0 verwendete Einträge ergeben. Derselbe Lerninput zählt nicht als neue unabhängige Erfahrung. Keine automatische Regelpromotion, kein allgemeines Modelltraining.

## `evidence` – lokale Exact-Affine-Evidenzkette

```text
bruce evidence create --train FILE --holdout FILE --output FILE
  [--domain bigint|rational] [--min N] [--max N] [--max-bits N]
  [--max-denominator N] [--max-trace-bytes N] [--max-candidates N]
  [--max-operations N] [--max-trace N] [--max-results N]
bruce evidence replay CHAIN
```

`create` verlangt beide nichtleeren CSV-Quellen und einen neuen Ausgabepfad. Die Domäne ist standardmäßig `bigint`, wahlweise `rational`; nur die feste Exact-Route `strategy=affine`, `planner=enumerate` ist zulässig. Die optionalen Grenzen verwenden die bestehenden Exact-Affine-Standards und -Validierungen; andere Forschungs-, Sitzungs-, Strategie-, Planner-, Experience- oder Regeloptionen sind hier nicht zulässig. Die CSV-Dateien werden lokal begrenzt gelesen und ihre unveränderten Bytes in `BRUCE_EVIDENCE_CHAIN_V1` eingebettet. Der Export überschreibt keinen bestehenden Pfad. [Byteformat und Bindung](formats.md#exact-affine-evidenzkette).

Die feste Policy `affine-inverse-v1` und der Scope `short-circuit-training-then-holdout-v1` beschreiben den unveränderten Exact-Lauf. Jeder erreichte Kandidat wird zuerst an Training geprüft; die erste Kontradiktion beendet seine Prüfung. Die inverse Gegenprüfung läuft nur nach einer passenden Vorwärtsauswertung. Holdout beginnt nur nach bestandener Trainingsprüfung und wenn Resultat-/Budgetfluss es erreicht. Budgets können innerhalb eines Kandidaten stoppen; `INCOMPLETE` ist kein Abdeckungsnachweis.

`replay` nimmt genau einen lokalen Chain-Pfad und benötigt die ursprünglichen CSV-Dateien nicht. Es prüft die feste Policy, die eingebetteten Samples, das native Exact-Bundle und den Ergebnisabschnitt erneut; es ist kein allgemeiner `replay`-Auftrag und gehört nicht zu `ApplicationTask`. `declared-disjoint-v1` bezeichnet technisch geprüfte x-Disjunktheit plus lokale Deklaration, keine extern nachgewiesene Unabhängigkeit oder Datenherkunft. Kandidat, Score, `VERIFIED_ON_SAMPLES`, Digest und Replay sind kein allgemeiner mathematischer Beweis; die Kette öffnet kein Learn-, Publish- oder Regelaktivierungsgate. Weder Cloud/Veröffentlichung noch Strategy-Promotion, BSGS-, Private-Key- oder Mining-Behauptungen sind Teil dieses Pfads. [Ergebnisgrenzen](results-and-control.md#exact-affine-evidenzergebnis).

## Forschung mit Grid-Quellen

`--grid-train` und optional `--grid-holdout` erwarten vollständige exakte Grid-Batches oder vollständige Grid-Fortsetzungen, keine einzelnen `grid`-Bundles. Quellen werden replaygeprüft. Jede darf höchstens 2097152 Bytes haben.

`--grid-axis x|y` (Standard x) bestimmt den Sample-Eingang. Bei x variiert die Blattbasis, der Exponent bleibt fest; bei y variiert der Blattexponent, die Basis bleibt fest. Alle Pfade müssen denselben Präfix und dieselbe feste andere Koordinate besitzen. Spezifikation und Betriebsmodus müssen übereinstimmen. Gestoppte Blätter und doppelte/überlappende Achsenwerte sind nicht zulässig. `int64`-Forschung ist hier verboten.

`--max-total-operations N` ist nur hier erlaubt (1..10000000). Das spezielle Konto verrechnet Eingabebytes der Gridquellen, bei erfahrungsbasierter Planung auch Snapshotbytes, und danach Forschungsoperationen. Effektives Forschungsbudget: `min(max-operations, N - Importbytes)`. N muss größer als die Importbytes sein. Das ist ein explizites gemischtes Kostenmodell, keine Messung der echten CPU-Operationen des Imports.

## `compare` – rationale Polynombrüche vergleichen

```text
bruce compare --left-numerator 'A0,A1,...' --left-denominator 'B0,B1,...'
              --right-numerator 'C0,C1,...' --right-denominator 'D0,D1,...'
              [--domain-policy quadratic|modular|modular-witness]
              [--max-bits N] [--max-operations N] [--output DATEI]
              [Sitzungsoptionen]
```

Alle vier Koeffizientenlisten sind Pflicht, aufsteigend nach Potenz: `1,2,3` bedeutet 1+2x+3x². Koeffizienten sind exakte rationale Zahlen. Bis zu 65 Einträge pro Liste (Grad höchstens 64); Normalisierung kann zusätzliche Grenzen erreichen. Ein Nullpolynom im Nenner bedeutet leeren Definitionsbereich und wird entsprechend behandelt.

Standard: `quadratic`, 4096 Bits (1..16384), 1000000 Operationen (1..1000000). `quadratic` nutzt GCD und begrenzte rationale Wurzelanalyse, `modular` ergänzt Zertifikate über die Primzahlen 2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, `modular-witness` zusätzlich eine begrenzte Zeugenprüfung über gekürzte a/b mit a in −16..16 und b in 1..16. Ein nicht entscheidbarer Fall bleibt `UNDECIDED`; mehr Operatorbudget ist keine Zusage allgemeiner Entscheidbarkeit.

Wertgleichheit und Definitionsbereich werden getrennt berichtet. Beispiel: `(x²−1)/(x−1)` und `x+1` haben dieselbe gekürzte Formel, aber bei x=1 verschiedene Definitionsbereiche. Diese Unterscheidung bleibt erhalten.

## `compatibility` – numerische Auswahl mit Regeln

```text
bruce compatibility --rules RULES --candidates CANDIDATES.csv [Optionen]
bruce compatibility --rules RULES --from-run RUN.bruce --at X [Optionen]
```

Genau eine Kandidatenquelle. `--rules` ist immer Pflicht. `--at` ist bei `--from-run` Pflicht, sonst verboten; es ist ein Int64-Wert. Für Grid-Forschungsquellen zusätzlich 0..7. Forschungsquellen werden replaygeprüft und müssen abgeschlossen sein. Formeln werden bei X ausgewertet; Bruchwerte oder Ergebnisse außerhalb Int64 sind nicht projizierbar. Leere Kandidatenmengen und Polstellen sind keine erfolgreiche Auswahl.

| Option | Standard | Bereich |
| --- | --- | --- |
| `--max-candidates` | 4096 | 1..4096 |
| `--max-checks` | 1048576 | 1..1048576 Regelprüfungen |
| `--max-trace` | 65536 | 1..65536 Traceereignisse |
| `--output` | keine | Neuer Bundlepfad |

Die gemeinsamen Sitzungsoptionen sind möglich. Regel-/Kandidatendateien sind je auf 1048576 Bytes begrenzt. Genaue Grammatiken und Auswahlsemantik stehen in [Dateiformate](formats.md). Eine Evidence-ID in einer Regel ist ein Verweis, keine externe Evidenzprüfung.

## `experience` – unveränderliche Erfahrungssnapshots

```text
bruce experience add --from-run RUN [--store ALT] --output NEU
                     [--max-transfer-checkpoints N] [--transfer-cancellation none|checkpoints]
bruce experience inspect SNAPSHOT
bruce experience revoke --store ALT --id ID --reason TEXT --output NEU
                     [--max-transfer-checkpoints N] [--transfer-cancellation none|checkpoints]
bruce experience replace --store ALT --id ID --reason TEXT --from-run RUN --output NEU
                      [--max-transfer-checkpoints N] [--transfer-cancellation none|checkpoints]
```

`add` startet ohne `--store` einen neuen Snapshot oder erweitert einen alten. Zulässige Quellen sind unabhängige vollständige rationale Drei-/Vier-/Fünferportfolios mit Holdout und mindestens einer `VERIFIED_ON_SAMPLES`-Hypothese, auch eingebettet in Grid-Forschung. Erfahrungsgesteuerte Quellen dürfen nicht erneut ihre eigene Evidenzbasis bilden. Quellaudits prüfen ein begrenztes Operationsbudget von höchstens 1000000; Snapshot/Quellimporte höchstens 1048576 Bytes.

`inspect` prüft den Snapshot samt Kette und eingebetteter Evidenz und liefert JSON. `revoke` deaktiviert einen aktiven Eintrag; `replace` deaktiviert ihn und fügt eine geprüfte Nachfolgequelle samt Korrekturverknüpfung ein. IDs kommen aus `records[].id`; keine erfundenen Positionsnummern verwenden.

Ein Korrekturgrund umfasst 1..256 Bytes, keine Zeilenumbrüche oder NUL-Zeichen. Höchstens 16 Ereignisse und acht historische Datensätze pro Snapshot; auch widerrufene/ersetzte Datensätze zählen zur historischen Grenze. Eine unveränderte Ersatzquelle und fremde doppelte Lerninputs werden zurückgewiesen.

Alle schreibenden Aktionen erfordern einen neuen `--output`-Pfad. Das kurze Hilfebeispiel `--store STORE ... --output STORE` ist kein In-place-Verfahren: Wenn beide denselben existierenden Pfad bezeichnen, scheitert der Export. Die CLI erzwingt bei revoke/replace `--store` nicht über `MissingOption`; ohne Quelle scheitert stattdessen die Snapshot-/ID-Validierung. Für sinnvolle Aufrufe ist die Angabe erforderlich.

Direktes `add`, `revoke`, `replace`, `plan-add` und `apply-add` akzeptieren die Transferoptionen `--max-transfer-checkpoints N` (UInt64, 0 zulässig) und `--transfer-cancellation none|checkpoints`. Bei Angabe mindestens einer Option umschließt `controlled-transfer-v1` den bisherigen Bericht mit dem Verbrauch; ohne diese Optionen bleiben direkter Bericht und Snapshotbytes unverändert. `inspect` erhält keine Transfer- oder Sitzungsoptionen und erwartet genau einen Positionsparameter. [Zählung, Veröffentlichung und Grenzen](transfer-control.md).

## `inspect-run`, `assess-run` und `replay`

Neu in Core 0.73: `bruce assess-run RUN.bruce` führt ebenfalls Replay aus und liefert den gemeinsamen Vertrag `application-assessment-v1`: Auftragstyp, Status, Stopgrund, `result_complete`, `claim_scope`, `proof_established` und `replay_checked`. Sampleforschung wird niemals zum allgemeinen Beweis hochgestuft. Ein Beweiskennzeichen beim Bruchvergleich bezieht sich nur auf die gelieferten Polynombrüche über Q. [Details und Beispiel](../implementation/153-result-assessment-usage.md). Genau ein Positionsparameter, keine Optionen.

```text
bruce inspect-run RUN.bruce
bruce replay RUN.bruce
```

Je genau ein Positionsparameter, keine weiteren Optionen. Beide lesen begrenzt, prüfen Bundle und Replay. `inspect-run` ist ausdrücklich keine billige Headerabfrage: Es rekonstruiert das Ergebnis und liefert die kompakte Zusammenfassung mit Typ, Status und Stopgrund. `replay` liefert das native Ergebnis-JSON; die äußere `execution`-Hülle des ursprünglichen CLI-Aufrufs wird nicht exportiert und erscheint hier nicht erneut.

Auch abgebrochene, aber gültig exportierte Läufe sind replaybar. Replay ist keine Fortsetzung; dafür gibt es nur den speziellen Grid-Fortsetzungspfad. Eine veränderte Prüfsumme oder abweichende rekonstruierte Rechnung scheitert. Mitgelieferte ältere Formate werden anhand ihres Version-Headers erkannt; unbekannte Versionen nicht automatisch migriert.

## `gate-run` – Forschungszulassung nach Replay

```text
bruce gate-run RUN.bruce [--max-session-checkpoints N] [--max-replay-bytes N] [--cancellation none|checkpoints]
```

Genau ein Dateiparameter; optional `--max-session-checkpoints N` und `--max-replay-bytes N`. Die effektive Eingabegrenze ist min(N, 32 MiB), standardmäßig 32 MiB. Überschreitung beim Lesen liefert `InputByteLimit` vor Replay. [Details](../implementation/159-gate-input-budget-usage.md). Zuerst wird der Lauf vollständig wiederhergestellt und geprüft. Fehler beim Lesen oder Replay ergeben Exit 2. Eine reguläre Ablehnung im Gate ist ein JSON-Befund mit Exit 0.

Ohne Budget bleibt die direkte Ausgabe erhalten. Mit `--max-session-checkpoints N` liefert `application-gates-budget-v1` die Felder `result` und `usage`. Replay und Quellenaudit teilen sich N Prüfpunkte unter REPLAYING. Budgetüberschreitung liefert Exit 2 ohne Teilbericht; weitere Grenzen siehe [Gate-Budget](../implementation/158-gate-budget-usage.md).

`application-gates-v1` enthält `kind`, `replay_checked`, `applicable`, `selection` und `experience_source`. Die beiden Entscheidungen enthalten `allowed` und `reason`. `selection` nutzt die gemeinsamen Forschungsregeln. `experience_source` nutzt die vorhandene Erfahrungsquellenprüfung einschließlich deren engeren Größen-, Format-, Unabhängigkeits- und Arbeitsgrenzen. Sie prüft keinen Zielspeicher. `learn_promotion`, `publish` und `general_proof` bleiben false.

Für andere Anwendungstypen als Int64-, exakte oder Grid-Forschung sind `applicable` und beide Zulassungen false (`UnsupportedApplicationKind`). Das ist keine Aussage über die Gültigkeit dieser Ergebnisse. [Details und Grenzen](../implementation/155-run-gates-usage.md).

Für `gate-run` aktiviert `--cancellation checkpoints` SIGINT/SIGTERM-Abbruch an vorhandenen Prüfpunkten. Standard ist `none`. Abbruch: `GateInspectionCancelled`, Exit 2, kein Teilbericht. Das Signal hat am Prüfpunkt Vorrang vor dem Budget. Dateilesen und maximale Signallatenz sind nicht garantiert steuerbar. [Details](../implementation/160-gate-signal-control-usage.md).

Seit Core 0.81 unterstützen auch einzelne Grid-Bundles die Gate-Prüfpunkt- und Signalsteuerung. Fachlich bleibt das Forschungsgate für sie nicht anwendbar. [Details](../implementation/161-single-grid-replay-control-usage.md).

## Lokale Regelprüfung

`rules-propose OLD NEW PROPOSAL` erstellt einen ungeprüften Vorschlag aus zwei gültigen Regeldateien. `rules-approve PROPOSAL CURRENT OUTPUT` zeigt beide Fassungen im Terminal und exportiert erst nach exakter Bestätigung. Alle drei Argumente sind Pflicht. Vollständige Referenz für jeden Parameter, Terminalverhalten, JSON-Felder, Paketformat, Fehler und ein durchgängiges Beispiel: [Lokale Regelprüfung](rule-review.md).

## Aktive Regeln und Journal

`rules-stage STORE NEW PROPOSAL` erstellt einen Vorschlag gegen die aktuelle Speichergeneration. `rules-activate STORE PROPOSAL` aktiviert nach exakter Terminalbestätigung und erneuter Basisprüfung. `rules-status STORE` prüft und zeigt die Historie. Keine weiteren Optionen.

`compatibility --rule-store STORE` verwendet die aktive Regelfassung anstelle von `--rules DATEI`; beide Optionen schließen sich aus. Alle übrigen Kompatibilitätsoptionen gelten weiter. [Vollständige Referenz: Parameter, Journal, Fehler, Beispiel und Vertrauensgrenzen](active-rules.md).

## Explizite Erfahrungsübergabe

`experience plan-add --from-run RUN [--store PREVIOUS] [--max-source-bytes N] [--max-store-bytes N] --output PLAN` prüft einen Import im Speicher und bindet seine Eingabebytes. Grenzen jeweils 0..1048576, Standard 1048576.

`experience apply-add PLAN --from-run RUN [--store PREVIOUS] --output NEW` prüft den Plan, beide Digests und den Import erneut und exportiert exklusiv. Keine Budgetüberschreibung bei Ausführung; keine Regelaktivierung. [Vollständige Referenz und Beispiel](experience-transfer.md).

## Geplante Z-Auswahl

`compatibility-plan --from-run SOURCE --rules RULES --at X [--max-source-bytes N] [--max-candidates N] [--max-checks N] [--max-trace N] --output PLAN` bindet Quelle, kanonische Regeln, Auswertungsstelle und Budgets.

`compatibility-apply PLAN --from-run SOURCE --rules RULES [--output RUN]` prüft Bindungen und fachliche Quelle erneut. Bei beiden Befehlen ist `--rule-store STORE` alternativ zu `--rules` zulässig. Anwendung übernimmt Budgets ausschließlich aus dem Plan. [Standards, Grenzen, Status, Format und vollständiges Beispiel](compatibility-transfer.md).

## Übergabesteuerung ab Core 0.94

`experience apply-add`, `compatibility-apply` sowie direktes `experience add`, `experience revoke` und `experience replace` akzeptieren zusätzlich `--max-transfer-checkpoints N` (UInt64, 0 zulässig) und `--transfer-cancellation none|checkpoints`. Bei Angabe einer Option umschließt `controlled-transfer-v1` den bisherigen Bericht mit dem Verbrauch. [Vollständiger Vertrag und Grenzen](transfer-control.md).

Die beiden Planbefehle akzeptieren seit Core 0.95 zusätzlich `--max-transfer-checkpoints N` und `--transfer-cancellation none|checkpoints`; die [Transfersteuerung](transfer-control.md) beschreibt Ausgabehülle, Zählung und Grenzen.
