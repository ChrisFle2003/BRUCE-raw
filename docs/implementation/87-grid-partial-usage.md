# Grid-Stapel begrenzen und Teilbefunde behalten – Core 0.41

```bash
./build/bruce grid-batch --paths '2,6;02,06;0,0;4,3;2,6' \
  --zero-zero stop --max-query-steps 3 --output grid-teil.bruce
./build/bruce replay grid-teil.bruce
```

Bruce bearbeitet die ersten drei Abfragen: Wert 64 berechnen, denselben Wert für dieselbe Adresse wiederverwenden und das letzte `(0,0)` lokal stoppen. Danach meldet der Stapel `INCOMPLETE / QueryBudgetExceeded`, drei bearbeitete von fünf angeforderten Abfragen und eine tatsächliche Potenzberechnung.

`--max-query-steps` zählt Abfragen, auch Cachetreffer und lokale Stopps. Mit null wird vor der ersten Abfrage ein leerer Teilbefund erzeugt. Mit fünf oder mehr Schritten wird die obige Liste vollständig abgearbeitet. Die üblichen Grenzen von höchstens 1024 Abfragen und 64 Schritten je Pfad bleiben erhalten.

## Abbrechen und wiederholen

Bei laufender Grid-Stapelausführung wird Strg+C zwischen zwei Abfragen als Abbruch erfasst. Mit `--output` kann der bereits abgearbeitete Teil als `INCOMPLETE / Cancelled` gespeichert werden. Replay stellt die gespeicherte Abbruchposition ohne neues Signal nach. Sehr kurze Stapel können bereits fertig sein, bevor der Abbruch eintrifft.

Der Export enthält weiterhin die ursprüngliche Liste sowie die Ergebnisse und Cacheentscheidungen des bearbeiteten Teils. Er überschreibt keine vorhandene Datei. Eine erneute Ausführung mit größerem Budget beginnt von vorn; eine Fortsetzung aus gespeichertem Cachezustand ist noch nicht implementiert.

## Welche Teile sind geprüft?

Alle Pfade werden vor Beginn strukturell geprüft. Nur die abgearbeiteten Abfragen besitzen ein berechnetes Ergebnis. Ungültige Pfade oder auftretende Rechenfehler bleiben Fehler ohne Export.

Die Formelforschung lehnt unvollständige Grid-Quellen ausdrücklich ab. Ein vollständig abgearbeiteter Stapel mit gesetztem Schrittbudget ist dagegen als Quelle zulässig. Bruce verwendet nicht automatisch nur den Anfang einer abgebrochenen Lern- oder Prüfliste.

Teilbefunde verwenden das neue Grid-Stapelformat V2. Normale unbeschränkte vollständige Läufe behalten V1. Die Quellenprüfung innerhalb der Forschung bleibt weiterhin atomar; allgemeine Phasensteuerung und Wiederaufnahme sind weitere Roadmap-Arbeit.

[Vertrag](86-grid-partial-contract.md) · [Abnahme](acceptance/grid-partial-v1-2026-09-22.md)
