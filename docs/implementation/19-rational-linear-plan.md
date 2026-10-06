# Rationale lineare Bruchfunktionen – Core 0.8

Freigegebene Fortsetzung: `research --strategy rational-linear` untersucht `(a*x+b)/(x+c)`, x != -c, im bestehenden Forschungs-/Replay-/Z-Pfad. Domain ausschließlich rational (neuer Default für diese Strategie), Planer `solve` / linear-system-v1. Alle bisherigen Zahlengrenzen und Koeffizienten-Zähler-/Nennerintervalle gelten für a,b,c. Nenner-Leitkoeffizient fest 1; keine freie rationale Ausdruckssynthese.

## Mathematischer Vertrag

Nur Lernpaare gehen in die Ableitung `a*x+b-c*y=x*y` ein. Ein inkrementelles exaktes lineares Gleichungssystem mit drei Unbekannten verarbeitet alle Lernpaare unter Operations-/Bitlimit. Widerspruch schließt die Familie aus; Rang 3 bestimmt höchstens einen Kandidaten; geringerer Rang fällt auf lazy vollständige Koeffizientensuche zurück. Kein singuläres System wird künstlich eindeutig erklärt.

Jeder Kandidat prüft alle Lern-/Hold-out-Paare erneut. Vorwärtsrechnung dividiert erst nach expliziter Prüfung x+c != 0. Gegenprüfung nutzt die separat expandierte Identität y*x+y*c=a*x+b. Eine an einem Sample gefundene Nennernullstelle ist ein Gegenbeispiel für die dort erwartete endliche Ausgabe. Kürzbare Faktoren entfernen keinen ausgeschlossenen Definitionspunkt. Verschiedene Pole bleiben verschiedene Kandidaten, auch bei gleichen Werten auf allen bisherigen Paaren.

Lazy Slots für a,b,c; ID `(a_slot*slots+b_slot)*slots+c_slot`, Z-Koordinaten `(a_slot*slots+b_slot,c_slot)`. Eindeutig abgeleitete Kandidaten erhalten dieselbe ID wie Enumeration. Ergebnis enthält Numerator-/Denominator-Koeffizienten, c, ausgeschlossenen Wert -c und Status. Kein allgemeiner Beweis.

Planung budgetiert jede Transformation, Pivotprüfung und Eliminationsoperation vor Ausführung. Pro Sampleprüfung reserviert der Runner 12 logische Operationen für Quotient und getrennte Identität. Trace und Checkpointprinzip analog zu bisherigen Strategien. Zahlen-/Arbeitslimits ergeben INCOMPLETE. Pole bei der Z-Projektion führen ausdrücklich zu einem Fehler; keine Rundung oder stilles Weglassen.

Neuer Container BRUCE_RATIONAL_LINEAR_BUNDLE_V1, Engine bruce-rational-linear-v1, Regel monic-linear-fraction-v1, Policy defined-samples-v1, Planer linear-system-v1. Das bestehende Zweistrategien-Portfolio bleibt unverändert versioniert; diese neue Familie wird ausdrücklich per Strategie gewählt.

## Schritte

- [x] Core-0.7-Fixture sichern; CLI-Abnahmen rot nachweisen.
- [x] Exakten Rangsolver, Bruchauswertung und unabhängige Kreuzprüfung implementieren.
- [x] Bounded Runner, CLI, Replay und Z-Import ergänzen; Pole und Definitionsbereich sichtbar halten.
- [x] Unabhängige Fraction-Referenzen, singuläre Familien, Pole/Kürzung, Grenzen, Abbruch und Manipulation prüfen.
- [x] Review, Debug-/UBSan-Gesamttests, Demo, Anleitung und Status fortschreiben.
