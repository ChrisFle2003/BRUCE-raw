# Mehrtermige Polynome – Core 0.6

Freigegebene Fortsetzung im bestehenden Forschungspfad: `research --strategy polynomial --max-degree D` mit D=0..8, Standard 3. Domänen BigInt (Standard) und rational. Keine neue Abhängigkeit oder Datenbank.

## Vertrag

Untersucht werden kanonische Polynome `c0+c1*x+...+cd*x^d`, d=0..D. Koeffizienten folgen den bisherigen Zähler-/Nennergrenzen und Bitlimits. Pro Grad werden die ersten d+1 unterschiedlichen Lern-x zur exakten Newton-Interpolation verwendet; Umrechnung liefert Monomkoeffizienten. Führende Nullkoeffizienten werden nur beim niedrigsten tatsächlichen Grad behalten. Kandidaten-ID = tatsächlicher Grad, Z-Koordinaten (Grad,0), Kontext polynomial-bigint-v1/rational-v1.

Interpolation erzeugt nur einen Kandidaten. Alle Lernpaare werden per Horner-Auswertung und separater Potenzsummen-Gegenprüfung ausgewertet; erst danach Hold-out. Für Grad d genügen d+1 verschiedene x zur Eindeutigkeit innerhalb der Klasse Grad<=d, nicht zum allgemeinen Datenbeweis. Bei weniger als D+1 Lernpaaren endet die gesamte Suche INCOMPLETE / InsufficientTrainingSamples, auch wenn ein niedrigerer Kandidat passt. Es wird keine vollständige Koeffizientenenumeration vorgetäuscht.

Planer ist fest `newton-v1` (CLI `--planner interpolate`); andere Planer und `--max-power` sind für polynomial ungültig. Neue Gradoption bei alten Strategien ungültig. Alte Ausgabe- und Replayversionen bleiben bytegleich; neuer Container BRUCE_POLYNOMIAL_BUNDLE_V1, Engine bruce-polynomial-v1, Regel polynomial-v1, Policy polynomial-samples-v1. Gegenprüfung, Ableitung und Konvertierung zählen im gemeinsamen Operationsbudget. Numerische/Budgetstopps bleiben unvollständig. Kontrollierter Abbruch an Plan-/Kandidatengrenzen mit bestehendem Checkpointprinzip.

## Umsetzung

- [x] Historischen Core-0.5-Export sichern, neue CLI-Abnahmen rot beobachten.
- [x] Begrenzte Newton-Ableitung, Horner-Auswertung und unabhängige Potenzsumme implementieren.
- [x] Polynomialstrategie im bestehenden research-/Replay-/Z-Pfad integrieren; bestehende Validierung und Dateiformate weiterverwenden.
- [x] Mehrtermige Ganzzahl-/Bruchpolynome, unabhängige Referenzwerte, fehlende Daten, Grad-/Koeffizientengrenzen, Hold-out-Widerlegung, Budgets/Abbruch und Manipulation testen.
- [x] Review, Debug-/UBSan-Gesamttests, Demo, Anleitung und Roadmap abschließen.

Außerhalb: beliebige Ausdrucksgrammatik, rationale Funktionen, Strategieorchestrierung, Lernen oder allgemeine Beweise.
