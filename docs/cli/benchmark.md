# BRUCE Benchmark-Suite V1

[CLI-Handbuch](README.md)

## Ziel und Messvertrag

Die Suite misst vollständige CLI-Prozesse mit festen Eingaben, unabhängigen Python-Gegenrechnungen und kontrollierter Ergebnisqualität. Sie verändert den mathematischen Core nicht. Python 3 und Linux werden benötigt; keine zusätzlichen Python-Pakete. Jeder Prozess läuft sequenziell mit neuer CLI-Instanz. Die Reihenfolge der Fälle wird mit dokumentiertem Seed gemischt. Messungen verschiedener Falltypen ergeben keine gemeinsame Leistungsrangliste.

Gemessen werden Wandzeit einschließlich Prozesserzeugung und Ausgabe, CPU-Zeit und maximaler RSS durch Linux `wait4`, Ausgabe-/Bundlebytes und vorhandene fachliche Zähler. Der Maximal-RSS gehört zu genau diesem Kindprozess, nicht zum historischen Höchststand aller bisherigen Kindprozesse. Er kann trotzdem den kurzen Fork-/Startabschnitt vor `exec` enthalten; insbesondere kleine Programme liegen dadurch an der Startspeicher-Untergrenze des Python-Runners. Die Millisekunden-Abfrage des Prozessendes beeinflusst sehr kurze Zeitmessungen. V1 ist kein isolierter C++-Mikrobenchmark.

Ein externer Timeout beendet die gesamte Prozessgruppe. `RLIMIT_AS` begrenzt virtuellen Adressraum, nicht physischen RSS; Core-Dumps sind deaktiviert. Setup, Gegenrechnung und zusätzliches Replay sind von der primären Fallzeit getrennt. Die Replay-Messung beim Export steht separat in den Rohdaten. CPU-Affinität wird erfasst, aber nicht verändert. Betriebssystemcaches und CPU-Takt sind nicht kontrolliert; es gibt keinen behaupteten Kaltstart. Grid-Caches sind je CLI-Prozess leer.

## Ausführen

Vom Projektstamm, mit Release-Binary:

```bash
python3 scripts/benchmark.py --binary ./build-release/bruce \
  --profile quick --output /tmp/bruce-bench-quick-01
```

Ein bestehender Ausgabeordner wird abgelehnt. Andere Messungen bleiben erhalten.

| Option | Standard / Bereich |
| --- | --- |
| `--binary` | Pflicht: Pfad zum auszuführenden Programm |
| `--output` | Pflicht: neuer Ergebnisordner |
| `--profile` | `quick`, `standard` oder `stress`; Standard quick |
| `--repeats` | quick 3, standard 10, stress 30; überschreibbar mit 1..1000 |
| `--timeout` | 30 Sekunden pro Kindprozess, positiv |
| `--memory-mib` | 1024 MiB virtueller Adressraum, mindestens 64 |
| `--seed` | 20260925, bestimmt die Messreihenfolge |

```bash
python3 scripts/benchmark.py --binary ./build-release/bruce \
  --profile standard --output /tmp/bruce-bench-standard-01
python3 scripts/benchmark.py --binary ./build-release/bruce \
  --profile stress --timeout 60 --memory-mib 2048 \
  --output /tmp/bruce-bench-stress-01
```

Der Seed verändert die Reihenfolge, nicht die festen Datensätze. Die Profile sind eine erste Referenzmenge, keine statistisch repräsentative Zufallsaufgabenbank. Gleiche Seeds erzeugen keine identischen Laufzeiten.

## Enthaltene Fälle

- BigInt-Multiplikation und rationale Addition bei 64/512 Bits, ab Standard zusätzlich 2048/8192 Bits. Python `int`/`Fraction` rechnet die erwarteten Ergebnisse unabhängig aus.
- Suche nach x² mit Power-Enumeration, adaptiver Ableitung und Polynominterpolation. Gefundene Hypothesen werden zusätzlich bei −3, 13 und weiteren nicht als Training verwendeten Stellen mit Python geprüft.
- Ausdruckssuche nach x+1 mit grammar, indexed-grammar und cached-grammar, mit 3 Knoten in Quick bzw. 5 in Standard/Stress. Normalformwerte werden separat substituiert. Dies ist zusätzliche Sampleprüfung, kein allgemeiner Identitätsbeweis.
- Rational-Linear und Drei-Familien-Portfolio: feste, adaptive und erfahrungsbasierte Reihenfolge auf demselben Ziel. Erfahrung entsteht aus einer getrennten Funktion; dieselben Eingabe-X-Werte, aber andere Zielwerte ergeben hier unterschiedliche Lerndatensätze. Ein verwendeter Erfahrungseintrag wird explizit verlangt. Quellauditkosten bleiben im gemessenen Erfahrungslauf enthalten.
- Grid mit wiederholten Adressen, Cache an/aus, verschiedenen Adressen, Fortsetzung und separatem Replay. Quick 64, Standard 512, Stress 1024 Anfragen. Der historische Fallname `grid-unique` bezeichnet bis zu 512 unterschiedliche Adressen; im Stressprofil werden diese zweimal durchlaufen. Da der FIFO-Cache nur 64 Einträge hält, entstehen dabei weiterhin 1024 Auswertungen. Fortsetzungs-/Replaybeispiele bleiben kleine kontrollierte Fälle.
- Regelprüfung mit Quick 64 Kandidaten × 8 Regeln, Standard 1024 × 128, Stress 4096 × 128. Bei Standard/Stress ist `INCOMPLETE/TraceBudgetExceeded` ausdrücklich erwartet: Das öffentliche Trace-Maximum ist 65536. Diese Fälle messen die korrekte Begrenzung, keine vollständige Auswertung sämtlicher Kombinationen.
- Polynombrüche mit gleicher gekürzter Formel, aber verschiedenen Definitionslücken unter drei Vergleichsverfahren.
- Geprüfter Export mit anschließendem separatem Replay und Ergebnisvergleich.
- Alle drei gemeinsamen Sitzungsbudgets bei Grenze 0 und beschädigtes Bundle. Erwartete Fehler gelten als bestandene Robustheitsprüfungen, nicht als gelöste Forschungsaufgaben.

## Ergebnisdateien

`report.md` enthält die lesbare Tabelle; `summary.csv` und `summary.json` die Aggregate. `measurements.jsonl` wird nach jeder Messung geschrieben. Jedes `*.stdout`/`*.stderr` hält die volle Antwort fest. `manifest.json` enthält Argumente, Sollwerte, Hardware-/Kernelangaben, CPU-Affinität, Binary-/Quell-/Runnerhashes und Eingabehashes. Die Bibliotheksauflösung steht unter `libraries`; die gefundene Systemcompiler-Version ist Zusatzkontext und beweist allein nicht den Compiler des übergebenen Binaries.

Bei erfolgreicher Prüfung aller Fälle Exit 0, bei Abweichungen Exit 1. Falsche Ergebnisse, unerwartete Exitcodes und Timeouts werden von Erfolgsmedianen ausgeschlossen, aber als Fehlschläge gezählt. Das verhindert, dass ein früh gescheiterter Aufruf als besonders schnell erscheint. Median, Minimum und Maximum werden berichtet; es wird aus drei oder zehn Läufen kein verlässliches p99 abgeleitet.

Die primären Fallnamen und ihr Manifest bestimmen den Sollstatus. Für Vergleiche müssen auch Status, Grenzen, Suchraum und Nachweisqualität gleich sein. Logische Operationen verschiedener Strategien sind keine universelle CPU-Einheit. Große theoretische Gridräume sind keine Anzahl ausgeführter Berechnungen.

## Pflege und Grenzen

Runner-Vertragstests: `python3 tests/integration/test_benchmark_runner.py`, auch als CTest `benchmark_runner_contracts` registriert. Sie prüfen unabhängige Sollwertvergleiche, Auswertung ohne Fehlmessungen, Zeitlimit und exklusive Ausgabeordner.

V1 misst noch keine injizierten Signale an allen Phasen, keine garantierte Abbruchlatenz, keinen Langzeitprozess mit gemeinsamem Speicher und keine Hardware-Leistungszähler. Höhere Wiederholungszahlen sind wiederholte neue Prozesse, kein Beweis gegen Lecks in einer künftigen dauerhaften Runtime. Weitere Gegenbeispielfamilien, mehr Ausdruckstiefen, längere Fortsetzungsketten und isolierte C++-Mikrobenchmarks sind spätere Erweiterungen. Ein einziger Benchmarkrechner erlaubt keine Aussagen über andere CPUs.
