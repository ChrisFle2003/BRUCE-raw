# Abnahme gemeinsamer Grid-/Forschungsrahmen / Core 0.40 – 22.09.2026

## Lieferung

Optionaler gemeinsamer logischer Arbeitsrahmen für die atomare Zulassung des Grid-Quellenimports und die anschließende exakte Forschung. Der Quellenimport wird mit einem Schritt je vollständigem Quellenbyte berechnet; die Forschung erhält höchstens den Rest und ihr bisheriges Einzelbudget. V3-Export und Replay erhalten Abrechnung, X-/Y-Projektion, vollständige Quellen und Teilbefunde. Ohne Gesamtbudget bleiben V1/V2 unverändert.

## Ausgeführte Prüfungen

```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure -j4
cmake --build build-ubsan -j4
ctest --test-dir build-ubsan --output-on-failure -j4
python3 scripts/demo.py
```

**75 von 75 Testgruppen bestanden**, sowohl Debug (61,18 s) als auch UndefinedBehaviorSanitizer (111,55 s; `-fsanitize=undefined -fno-sanitize-recover=all`). Keine Sanitizer-Meldung. Gesamtdemo erfolgreich; alle 26 historischen Bundle-Fixtures liefern unveränderte Ergebnisse. Lokale Links in 135 Markdown-Dateien geprüft.

Die drei neuen CLI-Testmethoden scheiterten vor Umsetzung an der unbekannten Option `--max-total-operations`. Nach Umsetzung bestehen sie; die bestehende Grid-Research-Unit enthält zusätzliche Budget-/Abbruch-/Zulassungsprüfungen.

- Quellenkosten stimmen exakt mit den tatsächlichen Dateibytes überein. Effektive Forschungsgrenze ist `min(requested, total-source)`; Gesamtnutzung bleibt unter oder gleich dem Budget.
- Für den quadratischen Referenzfall wird jede positive Forschungsrestgrenze unterhalb der vollständigen Arbeit geprüft: stets `INCOMPLETE`, innerhalb des Budgets und identisch replaybar. Genau ausreichendes Budget erreicht den bisherigen vollständigen Befund.
- Ein strengeres lokales Forschungsbudget bleibt wirksam. Null, übergroßes Gesamtbudget sowie Gesamtbudget kleiner oder gleich den Importkosten werden abgewiesen; fehlerhafte CLI-Aufträge erzeugen keinen Export.
- Callback-Abbruch in Y-Richtung bleibt auch mit Gesamtbudget `INCOMPLETE` und replaybar.
- Eine beschädigte Quelle bei unzureichendem Gesamtbudget wird vor ihrem Replay abgewiesen. Die Fehlermeldung bestätigt, dass zunächst die Importzulassung wirkt.
- Neu gehashte Änderungen an Politik, Gesamtbudget, ursprünglichem Forschungsbudget und Abrechnung sowie zusätzliche Inhalte werden verworfen.
- Vollständige Y-Forschung ist über Z auswählbar, unvollständige Forschung nicht. Forschungs- und Auswahl-Replay funktionieren nach Löschen der ursprünglichen Quellen.
- Gesamtdemo: 4005 Importschritte plus 144 Forschungsschritte ergeben `INCOMPLETE`; mit 145 Forschungsschritten entsteht `VERIFIED_ON_SAMPLES`. Beide Exporte bleiben ohne ursprüngliche Quelldateien replaybar.

## Unabhängiges Review

Keine konkreten Fehlerbefunde. Die drei Integrationstests und die erweiterte C++-Suite bestanden im Review. Acht Fälle ohne Gesamtbudget gegen Core 0.39 hatten identische Ausgaben und bytegleiche V1-/V2-Exporte.

32 zusätzliche X-/Y-Kombinationen bestätigten Restbudgetrechnung, Verbrauchszähler und identisches Replay. Ein neu gehashter V3-Wrapper mit eingebettetem V3 wurde abgewiesen. Codeprüfung bestätigte Importzulassung vor Quellenreplay und Prüfung der deklarierten Forschungsgrenze vor Forschungsreplay. Vollständige Suiten und endgültige Dokumentation wurden vom Hauptlauf geprüft.

## Grenzen

Die Abrechnung entspricht der benannten logischen Politik, nicht einer Messung sämtlicher CPU-Instruktionen, Laufzeit oder Prozess-RAM. Quellenimport bleibt atomar und nicht inkrementell unterbrechbar. Einlesen, äußere Metadaten-/Digestprüfung, Darstellung und spätere Z-Auswahl werden nicht zusätzlich als Einzelschritte gezählt. Andere Phasen besitzen weiterhin eigene Grenzen.

Allgemeine Aufgaben-/Strategie-/Gate-Verträge, Wiederaufnahme, Grid-Expansion und Experience-Import der Grid-Forschungswrapper bleiben offen. Kein vollständiger M0-/M2- oder Gesamtprojektabschluss.

[Vertrag](../84-grid-budget-contract.md) · [Benutzung](../85-grid-budget-usage.md)
