# Benchmark-Suite V1: erster Release-Lauf auf Bruce-AI

Stand: 25. September 2026, Core 0.72.0. Der mathematische Core wurde für diese Lieferung nicht geändert. Neu sind der Benchmarkrunner, vier Runner-Vertragstests und die [Bedienungsdokumentation](../cli/benchmark.md). CTest kennt dafür das zusätzliche Ziel `benchmark_runner_contracts`; ein vollständiger erneuter Core-Testlauf wird mit dieser Lieferung nicht behauptet.

## Messbedingungen

Remote-Projekt: `/home/chris/Desktop/Bruce_raw` auf Bruce-AI. Release-Konfiguration des Binaries in `/tmp/bruce072-owner-p8uriP`; CMake wurde aktualisiert und das CLI-Ziel geprüft. Gemeldete CPU: **QEMU Virtual CPU version 2.5+**. Das ist eine virtualisierte Messumgebung, keine Aussage über einen bestimmten physischen Prozessor.

Standard: 32 Fälle × 10 Wiederholungen = **320/320 erwartungsgemäß**. Stress: 32 Fälle × 30 Wiederholungen = **960/960 erwartungsgemäß**. Keine unerwarteten Exitcodes, Sollwertabweichungen oder Timeouts. Diese Zahl enthält erfolgreiche Robustheitsprüfungen mit erwartetem Exitcode 2 und im Stressprofil bewusst unvollständige Regelläufe. Sie bedeutet nicht 1280 vollständig gelöste mathematische Aufgaben.

Zeitgrenze 30 Sekunden pro Kindprozess, virtueller Adressraum höchstens 1024 MiB, Seed 20260925. Sequenzielle frische CLI-Prozesse, gemischte Fallreihenfolge, keine ausgeschlossene Aufwärmphase. Betriebssystemcache und VM-Auslastung nicht kontrolliert. Python-Referenzprüfung außerhalb der gemessenen CLI-Laufzeit. Vollständige Parameter, Hashes und Hardwareangaben stehen im jeweiligen Manifest.

## Beobachtungen im Standardprofil

| Vergleich | Variante A | Variante B | Aussage dieses Datensatzes |
| --- | --- | --- | --- |
| 512 wiederholte Grid-Anfragen | Cache aus: 18,43 ms Median, 512 Auswertungen | Cache an: 13,99 ms, 2 Auswertungen, 510 Treffer | Ca. 24 % weniger gesamte CLI-Wandzeit; Ausgabe und Anfragen bleiben zu verarbeiten |
| Power-Suche nach x² | Enumeration: 507 logische Operationen, 45 Kandidaten, 6,95 ms | Adaptiv: 117 Operationen, 2 Kandidaten, 6,90 ms | Deutlich weniger Sucharbeit, aber auf dieser kleinen Aufgabe keine belastbare große Zeitbeschleunigung |
| Rationale Portfolioaufgabe | Feste Reihenfolge: 7,01 ms | Erfahrungsreihenfolge: 9,52 ms | Erfahrung ist hier ca. 36 % langsamer; Quellenaudit gehört zur End-to-End-Messung |

Die Operationszähler sind implementierungsbezogen; sie dürfen nicht quer über beliebige Familien als identische CPU-Kosten behandelt werden. Prozentwerte beziehen sich nur auf die gemessenen Medianwerte dieses einen Datensatzes und Rechners.

## Beobachtungen im Stressprofil

Bei 1024 wiederholten Grid-Anfragen: **28,89 ms ohne Cache**, **19,59 ms mit Cache**, ca. 32 % weniger Wandzeit. Der Regelfall fordert 4096 Kandidaten mit 128 Regeln an und endet erwartungsgemäß mit `INCOMPLETE/TraceBudgetExceeded` nach Erreichen der Tracegrenze. Sein Median von 62,03 ms ist die Zeit bis zum korrekten Teilbefund, keine Geschwindigkeit einer vollständigen Prüfung aller 524288 Kombinationen.

Die drei Ausdrucksplaner liegen beim begrenzten x+1-Fall mit fünf Knoten zwischen 8,58 und 8,79 ms Median. Daraus wird kein allgemeiner Sieger erklärt. Größere Suchräume, wechselnde Formeln und mehr unabhängige Aufgaben sind für eine solche Aussage nötig.

## Speicher und Messgrenzen

CPU-Zeit und Maximal-RSS werden je Kindprozess über Linux `wait4` erfasst. Der kurze Forkabschnitt vor dem Programmwechsel kann den Peak-RSS mitbestimmen; insbesondere kleine Programme liegen deshalb an einer vom Python-Runner beeinflussten Untergrenze. V1 eignet sich hier nicht für Aussagen über minimale C++-Heapbelegung. Millisekundenkurze CLI-Aufrufe enthalten außerdem Startkosten und Beobachtungsauflösung.

Der Stresslauf ist eine wiederholte Referenzsuite, kein Langzeitnachweis für einen einzelnen dauerhaft laufenden Core-Prozess. Signale wurden im Messrunner-Zeitlimit-Test geprüft, aber nicht systematisch in alle Core-Phasen injiziert. Externe Zeitgrenzen und Speichergrenzen werden nicht als neue Eigenschaften der BRUCE-CLI ausgegeben.

## Reproduzierbare Artefakte

- [Standardbericht](../../benchmarks/2026-09-25-standard-v1/report.md), [CSV](../../benchmarks/2026-09-25-standard-v1/summary.csv), [Manifest](../../benchmarks/2026-09-25-standard-v1/manifest.json).
- [Stressbericht](../../benchmarks/2026-09-25-stress-v1/report.md), [CSV](../../benchmarks/2026-09-25-stress-v1/summary.csv), [Manifest](../../benchmarks/2026-09-25-stress-v1/manifest.json).
- Die jeweiligen Ordner enthalten zusätzlich `measurements.jsonl`, sämtliche Eingaben, Exporte und volle stdout/stderr-Dateien. Die vollständigen Rohdaten beider Läufe benötigen zusammen ungefähr 252 MB.

Runner-Vertragstests bestehen lokal und auf Bruce-AI. Lokal bestanden zusätzlich Vorläufe des Standard- und Stressprofils; diese Debug-Vorläufe sind keine Grundlage der obigen Release-Leistungszahlen. Suite und Dokumentation wurden per SCP geliefert, die Remote-Rohdaten per SCP zurückgeholt.

## Nächste begründete Messausbaustufe

Mehrere deterministisch erzeugte Aufgaben pro Formelfamilie und Größenklasse, getrennte Experience-Quell-/Zielgruppen sowie isolierte C++-Messschleifen ergänzen. Letztere sollen Rechenzeit und Speicher unterhalb der CLI-Startkosten sichtbar machen. Erst danach lassen sich Strategieranking, dauerhafter Cache oder Parallelisierung anhand breiterer Daten priorisieren.
