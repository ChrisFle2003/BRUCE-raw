# Bruce-Core 0.72: aktueller Architekturstatus

## Umgesetzt

- Ausführung, Serialisierung und Replay laufen weiterhin über die lineare Sitzung `EXECUTING → SERIALIZING → REPLAYING → READY`.
- Das Sitzungsbudget zählt bestehende Kontrollpunkte über alle drei Phasen.
- Das separate Serialisierungsbudget misst die fertige Bundlegröße vor dem Replay.
- `GridResume` kopiert große Elterninhalte blockweise.
- `GridResearch` kopiert verschachtelte Forschungsbundles jetzt ebenfalls in Blöcken bis 64 KiB. Die vorhandenen Bytes, Digests und Replayverträge bleiben gleich.
- `ExactResearch` kopiert große `experience_snapshot`-Inhalte über denselben Blockpfad.
- `GridResearch` reicht diese Kontrolle in eingebettete `ExactResearch`-Bundles weiter.
- `Compatibility` kopiert Regeln, Kandidaten und Ursprungspayloads ebenfalls blockweise.
- `FractionComparison` kopiert Aufgaben- und Ergebnispayloads ebenfalls blockweise.
- Die Abnahme umfasst 106 CTest-Ziele; der lokale Stand besteht vollständig.

## Bewusst offen

- Die Blockgrenzen sind Abbruchpunkte, keine Zusage einer maximalen Signallatenz.
- Es gibt weiterhin keinen gemeinsamen mathematischen Operationszähler für alle Strategien.
- Native Fortsetzungsimporte, weitere Payloadfamilien, externe Evidenz, freie Ausdruckssynthese, Sprache, NanoCell, Cockpit, Phase-DAG, GPU und BigInt im Grid/Z bleiben eigene Roadmap-Pakete.
- Die M0–M2-Matrix ist eine Nachweisübersicht und keine Gesamtfreigabe.

## Nächster sinnvoller Schritt

Nach Wiederherstellung der Schreibverbindung zum Remote-Arbeitsordner folgt zuerst der Remote-Build von Core 0.72. Danach kann die nächste Payloadfamilie mit demselben unveränderten Byte-/Replayvertrag angeschlossen werden.
