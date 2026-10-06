# Quadratische Nenner – Core 0.12

Freigegebene Fortsetzung der begrenzten Bruchfunktionsforschung im bestehenden Math-/Research-/Replay-/Z-Pfad. Neue Strategie `rational-quadratic`, feste Familie `(a*x+b)/(x²+c*x+d)` mit rationalen Koeffizienten, Nennerleitkoeffizient 1 und erhaltener Bedingung `x²+c*x+d != 0`. Zahlenbereich `rational`, Planer `solve`; keine freie Ausdrucksgrammatik.

## Ableitung und Prüfung

Nur Lernpaare liefern die linearen Gleichungen `a*x+b-c*x*y-d*y=x²*y`. Der gemeinsame inkrementelle Gaußsolver verwendet hier vier Unbekannte. Jede Zeilentransformation, Pivotprüfung und Eliminationsoperation wird vor der Ausführung budgetiert. Alle Lernzeilen werden bis zum Widerspruch verarbeitet; auch zusätzliche Zeilen nach Erreichen von Rang 4 werden geprüft.

- Rang 4: höchstens ein Koeffiziententupel. Nur kanonische Zähler im angegebenen Intervall und Nenner innerhalb der Grenze gehören zur Familie.
- Rang kleiner 4: lazy vollständige Enumeration der begrenzten kanonischen Tupel; keine künstliche Eindeutigkeit.
- Inkonsistentes System oder eindeutiges Tupel außerhalb der Familie: `NO_CANDIDATE`, sofern die Planung vollständig war. Das schließt keine anderen Formelfamilien aus.

Alle erzeugten Kandidaten werden an den Lernpaaren erneut geprüft. Die Vorwärtsrechnung verwendet `(a*x+b)/((x+c)*x+d)`. Die getrennte Gegenrechnung prüft `y*x*x+y*c*x+y*d=a*x+b` und den separat expandierten Nenner. Pro Sample werden vorab 24 logische Operationseinheiten reserviert. Es handelt sich um konservative Arbeitszählung, nicht um CPU-Instruktionen.

Hold-out-Daten beeinflussen weder Ableitung noch Kandidatenreihenfolge. Eine Nennernullstelle widerspricht einer dort geforderten endlichen Ausgabe. Auch bei kürzbaren Faktoren bleibt die Definitionslücke erhalten. Die Ausgabe bewahrt den Nenner als Koeffizienten `[d,c,1]` und exakte Bedingung; irrationale Nullstellen müssen dafür nicht angenähert oder explizit aufgelöst werden. Keine allgemeine Beweisaussage aus Samples.

## Grenzen und Identität

Sei `S = (maximum-minimum+1) * max_denominator`. Zusätzlich zu den bisherigen Grenzen (Zählerbreite höchstens 4096, Nennergrenze höchstens 16) gilt für diese Strategie `S <= 32768`. Dadurch bleiben die vierdimensionalen Rohslots `S^4 <= 2^60` adressierbar. Nicht gekürzte Bruchdarstellungen werden übersprungen; `raw_parameter_combinations` zählt Rohslots, nicht gültige oder ausgeführte Kandidaten.

Die ID lautet `((a_slot*S+b_slot)*S+c_slot)*S+d_slot`. Ableitung und Enumeration geben demselben Tupel dieselbe ID. Z-Koordinaten sind `(id / 2^30, id % 2^30)`; beide passen in 30 Bit und ihre Cantor-Adresse bleibt im vorhandenen 64-Bit-Kern. Es wird kein Grid für alle Rohslots materialisiert.

Bit-, Kandidaten-, Operations-, Ergebnis-, Trace- und Abbruchgrenzen folgen dem bestehenden Runner. Ein nicht abgeschlossener Lauf bleibt `INCOMPLETE`, auch wenn bereits positive Hypothesen vorliegen. Ein Callback-Abbruch wird als konkreter Checkpoint im Export eingefroren. Z-Projektion rechnet die Formel tatsächlich aus, weist Pole ausdrücklich ab und verlangt weiterhin einen ganzzahligen 64-Bit-Wert.

## Versionen und Abgrenzung

Container `BRUCE_RATIONAL_QUADRATIC_BUNDLE_V1`, Engine `bruce-rational-quadratic-v1`, Regel `monic-quadratic-fraction-v1`, Policy `defined-samples-v1`, Planer `quadratic-system-v1`. Replay enthält Eingaben, Grenzen, Befund und Trace. Vorhandene lineare Bruchfunktions-/Portfolio-/Experience-Versionen bleiben unverändert.

Die Familie ist zunächst eine eigenständige Strategie. Das bestehende Dreistrategien-Portfolio und dessen Erfahrungsranking nehmen sie noch nicht auf. Die vollständige Einbindung erfordert eine eigene Erweiterung von Portfolioidentität, Definitionsbereich und Experience-Versionen.

## Abnahmeweg

Historisches Core-0.11-V4-Paket vor Änderungen sichern; neue CLI-Anforderung rot nachweisen; gemeinsamen Solver und Runner erweitern; Vorwärts-/Gegenrechnung, Rangmangel, Pole, Hold-out, Replay und Z-Projektion testen; unabhängige Fraction-Referenz und Review; Debug-/UBSan-Suiten, ausführbare Beispiele und Anleitung. Nachweise im [Abnahmebericht](acceptance/rational-quadratic-v1-2026-09-22.md).
