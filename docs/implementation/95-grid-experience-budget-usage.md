# Core 0.45: Grid- und Experience-Import gemeinsam begrenzen

Bei Grid-Forschung mit Erfahrungsspeicher berücksichtigt `--max-total-operations` jetzt auch die vollständige Größe des eingebetteten Snapshots.

```bash
./build/bruce research --strategy portfolio-rational --max-degree 2 \
  --min -2 --max 2 --max-power 2 --max-denominator 1 \
  --grid-train lernen.bruce --grid-holdout pruefen.bruce \
  --planner experience-order --experience erfahrung.bruce \
  --max-total-operations 200000 --output ergebnis.bruce
./build/bruce replay ergebnis.bruce
```

Die Gridquellen und ein zulässiger Erfahrungsspeicher müssen bereits vorliegen; ihre Erstellung beschreibt [Core 0.43](91-grid-experience-usage.md). Auch ein [korrigierter Snapshot](93-experience-replacement-usage.md) ist zulässig.

Unter `work_budget` zeigt Bruce getrennt die Gridquellbytes, `experience_import_operations`, den verbleibenden Suchrahmen und den Verbrauch. Beispielsweise bleiben bei 10.000 Gridquellbytes, 30.000 Experiencebytes und einem Gesamtrahmen von 50.000 höchstens 10.000 neue Suchoperationen. Ein kleineres `--max-operations` begrenzt die Suche zusätzlich.

Reicht der Rahmen nicht einmal für beide Importe plus eine Suchoperation, wird der Auftrag ohne Export abgewiesen. Reicht er für den Import, aber nicht für die gesamte Suche, entsteht ein replaybarer `INCOMPLETE`-Befund. Abgeschlossene Ergebnisse bleiben für die Z-Auswahl nutzbar.

Die Rechnung verwendet Quellbytes als logische Importkosten. Sie misst weder Prüflaufzeit noch Prozessspeicher. Die separat angezeigte Auditbilanz wird nicht zusätzlich vom Rahmen abgezogen. Frühere V3-Dateien behalten beim Replay ihre ursprüngliche Abrechnung; neue erfahrungsgeleitete Läufe mit Gesamtrahmen verwenden V4.

## Weitere Roadmap

Der gemeinsame logische Rahmen für Gridimport, Experienceimport und neue Suche ist implementiert. Unterbrechbare Quellen-/Snapshotprüfung, tatsächliche phasenübergreifende Arbeitsmessung, allgemeine Strategie-/Gate-Verträge und fortsetzbare Forschung bleiben offen. Ebenso externe Evidenz, automatische Regelpromotion, Sprache, NanoCell, Cockpit, Phase-DAG, GPU und BigInt im Grid/Z. Dies ist kein vollständiger M2-/M4-Abschluss.

[Vertrag](94-grid-experience-budget-contract.md) · [Abnahme](acceptance/grid-experience-budget-v1-2026-09-22.md)
