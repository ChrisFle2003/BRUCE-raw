# Begrenzte Potenzen in Ausdrucksbäumen – Core 0.22

Freigegebene Fortsetzung von Core 0.21: Die eigenständige Ausdruckssynthese erhält optional den Operator `^`. Er verbindet einen beliebigen zulässigen Basisausdruck mit einem **literalen ganzzahligen Exponenten von 2 bis 8** aus dem deklarierten Konstantenvorrat. Beide Ausdrucksplaner, beide Gleichheitsmodi, Budgetierung, Gegenprüfung, Replay und Z-Projektion sind integriert.

## Grammatik und Identität

`--operators` akzeptiert für `--strategy expression` eine nichtleere Auswahl aus `+-*/^` ohne Duplikate; die kanonische Reihenfolge entspricht diesem Alphabet. Der Standard bleibt `+-*/`. Das Fünferportfolio erlaubt weiterhin ausschließlich die vier Grundrechenarten; `^` wird dort ausdrücklich zurückgewiesen.

Ein Potenzknoten zählt wie andere binäre Knoten: Operator, linker Teilbaum und rechter Konstantenknoten. Der rechte Knoten muss unmittelbar eine deklarierte Konstante mit Nenner 1 und Wert 2 bis 8 sein. Variable, berechnete, gebrochene, negative Exponenten sowie 0 und 1 werden abgewiesen. Andere Konstanten dürfen weiterhin als Basen oder in anderen Operationen auftreten. Ohne geeignete Exponentenkonstante entstehen keine Potenzkandidaten; der verbleibende Suchraum ist weiterhin gültig. `--max-power` gehört zur bestehenden Potenzformelfamilie und steuert diese Grammatik nicht.

`ExpressionGrammar` zählt für `^` ausschließlich Blöcke mit rechtem Teilbaum der Größe 1. Die Blockgröße ist die Anzahl linker Bäume mal Anzahl geeigneter Exponenten. Beide Rangplaner bilden den Exponentenindex auf denselben ursprünglichen Konstantenrang ab. Baumreihenfolge und IDs bleiben innerhalb derselben Grammatik stabil; IDs verschiedener Grammatiken sind nicht austauschbar.

Bei neun Knoten besitzt der Index maximal 36 Blöcke. Mit 64 Konstanten von 0 bis 63 und allen fünf Operatoren enthält der theoretische Raum 1450321801175 Ausdrücke. Diese Zahl beschreibt den adressierbaren Raum, keine ausgeführte Suchleistung. Kandidaten werden weiterhin einzeln innerhalb der gesetzten Grenzen erzeugt.

## Exakte Auswertung und Definitionsbereich

Die Hauptrechnung verwendet wiederholtes Quadrieren mit exakter rationaler Multiplikation. Die getrennte Gegenrechnung multipliziert Zähler und Nenner linear als ganze Zahlen. Jede Multiplikation verbraucht logische Arbeit und unterliegt der Bitgrenze. Die Gegenrechnung übernimmt kein numerisches Ergebnis der Hauptrechnung.

Die rationale Normalform potenziert Zähler und Nenner mit vorhandenen Polynomoperationen. Ausschlusspolynome aus dem Basisausdruck bleiben erhalten, auch wenn sich dessen Wert algebraisch kürzen lässt oder null wird. Deshalb bleibt beispielsweise `(x/x)^2` bei x=0 undefiniert. Ein überall undefinierter Basisausdruck bleibt undefiniert. Für eine definierte Basis gilt `0^p = 0` bei allen erlaubten p.

Die vorhandene Normalformgrenze bleibt Grad 64; verschachtelte Potenzen können sie überschreiten. Grad-, Bit-, Operations-, Kandidaten-, Ergebnis- und Tracegrenzen liefern einen nachvollziehbaren unvollständigen Befund. Ein solcher Abbruch wird nicht als vollständiger negativer Suchbefund ausgegeben. Normalisierte Formen werden zusätzlich gegen die Originalausdrücke an den Samples geprüft.

## Ergebnis, Replay und Auswahl

Alle eigenständigen Grammatiken mit `^` verwenden:

| Feld | Wert |
| --- | --- |
| Container | `BRUCE_EXPRESSION_BUNDLE_V5` |
| Engine | `bruce-expression-v5` |
| Regel | `bounded-power-v1` |
| Prüfpolicy | `defined-expression-samples-v5` |
| Exponentenpolicy | `literal-2-to-8-v1` |
| Planer | `size-ranked-v1` oder `block-index-v1` |

Operatorauswahl, Exponentenpolicy, Gleichheitsmodus und Suchgrenzen sind Bestandteil des Exports. Replay validiert die Versionen, erzeugt den gesamten Lauf erneut und vergleicht Ergebnis, Kosten und Trace kanonisch. V5 verlangt `^`; ältere Ausdrucksversionen dürfen es nicht nachträglich enthalten. Ohne `^` bleiben historische Formate und Kosten unverändert. Die Ausdrucks-V5 ist unabhängig vom bereits existierenden Erfahrungsspeicherformat V5.

Z-Import und Auswahl-Replay akzeptieren das neue Format. Die Herkunftskennung beginnt mit `expression-power-` und enthält Operator-Bitmaske, Planer-/Normalisierungsmerkmale und Domäne. `^` verwendet das zusätzliche Maskenbit 16. Ursprüngliche Definitionslücken, Int64-Projektionsgrenzen und die Sperre unvollständiger Befunde gelten weiter.

## Abnahme und Grenzen

[Abnahmebericht](acceptance/expression-powers-v1-2026-09-22.md) und [Benutzung](48-expression-powers-usage.md) enthalten überprüfte Beispiele. Unabhängig materialisierte kleine Grammatiken prüfen beide Generatoren gegen eine separate Referenz. Zahlenorakel, Definitionslücken, Abbruchreihen, manipulierte Exporte und historische Replays sind abgedeckt.

Portfolio-/Experience-Einbindung von `^`, variabler oder rationaler Exponent, allgemeinere Funktionen und unbeschränkte Synthese bleiben offen. Eine bestätigte Samplebeziehung ist kein allgemeiner Beweis und kein vollständiger M2-Abschluss.
