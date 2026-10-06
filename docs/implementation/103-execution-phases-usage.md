# Core 0.49: Ausführen, replayprüfen, dann ausgeben

Bruce besitzt jetzt eine erste gemeinsame Phasensteuerung für die acht vorhandenen Laufarten. Sie ist optional:

```bash
./build/bruce grid-batch --paths '2,6;4,3' --max-query-steps 1 \
  --verification replay --output gepruefter-teil.bruce
```

Bruce führt den Auftrag aus, serialisiert den Befund und replayt ihn vollständig. Erst wenn Ergebnisart und Inhalt übereinstimmen, liefert die Sitzung READY und die CLI schreibt die Datei.

Die Ausgabe enthält zwei Bereiche:

- `execution`: Phasenfolge, bestandene Replayprüfung und ob der native Auftrag vollständig ist.
- `result`: bisherige fachliche Ausgabe mit Ergebnissen, Status und Stopgrund.

Im Beispiel ist `replay_verified` true, aber `result_complete` false. Der Teilbefund wurde korrekt reproduziert; die zweite Abfrage bleibt unbearbeitet. Ein mathematischer Beweis wird daraus nicht abgeleitet.

```bash
./build/bruce replay gepruefter-teil.bruce
```

Die Datei enthält das unveränderte native Bundle. Replay zeigt deshalb direkt den fachlichen Befund, nicht die Sitzungshülle des früheren Aufrufs. Ohne `--verification` oder mit `--verification none` bleiben Ausgabe und Ablauf wie bisher.

Die Option ist auch für Einzel-Grid, Grid-Fortsetzung, Grid-Forschung, beide Forschungsdomänen, Bruchvergleich und Z-Auswahl verfügbar. Bei Fehlern wird die betroffene Arbeitsphase genannt und kein Ergebnisbundle exportiert.

## Grenzen und nächster Architekturschritt

Die zusätzliche Prüfung benötigt zusätzliche Rechenzeit und kann den Speicherbedarf erhöhen. Sie hat noch kein eigenes gemeinsames Kostenbudget. Ein Benutzerabbruch wird in der eigentlichen Berechnung an ihren lokalen Checkpoints erfasst; die anschließende Prüfung verwendet die gespeicherte Abbruchposition und läuft vollständig.

Damit ist ein konkreter linearer Phasenablauf implementiert. Allgemeine Phasengraphen, unterbrechbare Prüfphasen, phasenübergreifende Budgetsteuerung und persistente Sitzungsfortsetzung bleiben offen. Die vorhandenen Forschungs-Gates bleiben gültig; M0–M2 werden nicht pauschal abgeschlossen.

[Vertrag](102-execution-phases-contract.md) · [Abnahme](acceptance/execution-phases-v1-2026-09-22.md)
