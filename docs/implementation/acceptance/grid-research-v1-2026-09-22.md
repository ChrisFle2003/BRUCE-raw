# Abnahme Grid-Forschung / Core 0.38 – 22.09.2026

## Lieferung

Geprüfte exakte Grid-Stapelexporte dienen als Trainings-/Holdout-Quelle für die bestehende exakte Forschung. Eine versionierte Projektion hält Vorfahrenpfad und Exponent fest, verwendet die letzte X-Koordinate als Eingabe und erhält die vollständigen Quellen. Forschungs- und Auswahl-Replay benötigen die ursprünglichen Dateien nicht. Alte CSV-Aufträge behalten ihre bisherigen Ergebnisse und Formate.

## Prüfungen

```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure -j4
cmake --build build-ubsan -j4
ctest --test-dir build-ubsan --output-on-failure -j4
python3 scripts/demo.py
```

**73 von 73 Testgruppen bestanden**, sowohl Debug (60,89 s) als auch UndefinedBehaviorSanitizer (111,49 s; `-fsanitize=undefined -fno-sanitize-recover=all`). Keine Sanitizer-Meldung. Gesamtdemo erfolgreich; alle 25 historischen Bundle-Fixtures liefern unveränderte Ergebnisse. Lokale Links in 129 Markdown-Dateien geprüft.

Die ursprünglichen positiven Integrationstests scheiterten vor Implementierung an der unbekannten neuen CLI-Option. Nach Umsetzung bestehen sechs Integrationstestmethoden und eine direkte C++-Suite.

- Grid-Training `(0,0),(1,1),(2,4)` und Grid-Holdout `(3,9),(4,16)` ergeben mit der Polynomstrategie Koeffizienten `0,0,1` und `VERIFIED_ON_SAMPLES`.
- Z-Auswahl bei 5 liefert 25 und enthält die vollständige Herkunft. Auswahl außerhalb 0–7 wird abgelehnt. Forschungs- und Auswahl-Replay bleiben nach Löschen der ursprünglichen Dateien identisch.
- Vergleich mit CSV-Eingaben für Polynom, affine Forschung, adaptive Potenzsuche, Zweierportfolio und Fünferportfolio: innere Forschungsbefunde identisch; die Wrapper sind replaybar.
- Unterschiedliche Exponenten, Vorfahrenpfade und GridSpecs sowie doppelte/überlappende X-Eingaben werden abgewiesen. Kanonisch gleiche Präfixschreibweisen sind zulässig.
- Modulare Werte, Metriken, Stopps, gemischte CSV-/Grid-Optionen, `int64` und Holdout ohne Grid-Training werden abgewiesen.
- Replay verwirft einen Wrapper, dessen Grid-Quellen und innere Forschung jeweils gültig sind, aber unterschiedliche geordnete Samples besitzen.
- Geänderte Projektionsversion, Quelle, Blocklänge und Herkunftsmetadaten werden auch mit neuem äußerem Digest abgewiesen; gekürzte und zusätzliche Inhalte ebenfalls.
- Operationsbudget und Callback-Abbruch behalten `INCOMPLETE` samt identischem Replay. Explizite Samples neben API-Grid-Quellen sowie leere oder übergroße Trainingsquellen sind Fehler.
- Vorhandene Exportdateien werden nicht überschrieben; fehlerhafte Aufträge erzeugen keine Datei.

## Review und Korrektur

Das unabhängige Review fand einen P2-Randfall: Eine ausdrücklich angegebene leere Holdout-Datei wurde zunächst als fehlender Holdout behandelt. Ein Regressionstest bestätigte das Verhalten; die CLI weist es jetzt mit `EmptyGridHoldoutSource` ohne Export ab. Der korrigierte Fall ist Bestandteil der vollständigen Prüfläufe.

Keine weiteren konkreten Befunde. Zwölf unabhängige CSV-Vergleiche gegen Core 0.37 über drei Strategien, zwei Domänen und zwei Operationsbudgets waren in Exitcode, stdout und stderr identisch. Quellenbindung, Versionen, fester Ausschnitt, Z-Bereich und Ressourcenlimits wurden zusätzlich gelesen. Vollständige Suiten und endgültige Dokumentation wurden vom Hauptlauf geprüft.

## Grenzen

Der Ausschnitt enthält höchstens acht unterschiedliche X-Eingaben und eine bereits bekannte Grid-Wertregel. Wiedergefundene Formeln und Holdout sind Samplebefunde, kein neuer allgemeiner Beweis. Die Z-Auswahl führt keine neue Grid-Gegenprüfung durch.

Quellenprüfung und Forschung haben getrennte Grenzen; ein gemeinsames Phasenbudget fehlt. Experience-Import des neuen Wrappers, weitere Projektionen, Expansion, BigInt im Grid/Z und universelle Aufgaben-/Strategie-/Gate-Verträge bleiben offen. Keine vollständige M2- oder Gesamtprojektabnahme.

[Vertrag](../80-grid-research-contract.md) · [Benutzung](../81-grid-research-usage.md)
