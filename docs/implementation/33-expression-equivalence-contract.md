# Definitionsmengentreue Ausdruckszusammenführung – Core 0.15

Freigegebene Fortsetzung der begrenzten Ausdruckssynthese: `--equivalence rational` ergänzt die bestehende Strategie `expression`. Unterschiedliche Bäume dürfen zusammengeführt werden, wenn ihre exakten rationalen Normalformen und ihre erhaltenen Ausschlussbedingungen identisch sind. Der Standard `syntax` und dessen historischer Replay bleiben unverändert.

## Normalform und Definitionsbereich

Ein Ausdruck erhält ein Tripel aus Polynomen über Q: gekürzter Zähler N, monischer Nenner D und monisches quadratfreies Ausschlusspolynom G. Die Bedeutung ist N(x)/D(x) unter der Bedingung G(x) != 0. Koeffizienten stehen in aufsteigender Potenzreihenfolge. `[1]` bedeutet keine Ausschlüsse. Der überall undefinierte Ausdruck wird gesondert als `empty_domain=true`, N=0, D=1, G=0 dargestellt.

Die Normalisierung folgt dem originalen Baum von unten nach oben. Bei jeder Operation werden die Ausschlüsse beider Kinder übernommen. Bei Division kommt der Zähler des normalisierten rechten Kindes als weiterer Ausschluss hinzu. Ein überall undefiniertes Kind oder ein identisch verschwindender Divisor macht auch den Elternausdruck überall undefiniert. Multiplikation mit null entfernt keine Auswertungsbedingung.

N und D werden mit dem euklidischen Polynom-GGT gekürzt; D wird auf Leitkoeffizient 1 skaliert. G wird unabhängig davon über G/GGT(G,G') quadratfrei und monisch gemacht. Dadurch bleiben beispielsweise die Lücken von x/x, x²/x² und 1/(1/x) bei x=0 erhalten. Eine Kürzung des Gesamtbruchs löscht keine innere Division.

Der Gruppenschlüssel enthält N, D, G und die Kennzeichnung des leeren Bereichs. Identische Schlüssel sind ein hinreichendes algebraisches Gleichheitskriterium einschließlich Definitionsbereich. Dies ist keine vollständige Entscheidung über gleiche rationale Nullstellenmengen: Faktoren ohne rationale Nullstellen werden nicht entfernt. Zum Beispiel dürfen 0 und 0/(x²+1) getrennt bleiben, obwohl beide auf Q überall den Wert 0 haben. Ungleiche Schlüssel beweisen daher keine Ungleichheit der Funktionen.

## Forschung, Herkunft und Kosten

Die Grammatik, IDs, Reihenfolge und ursprünglichen Sampleprüfungen bleiben erhalten. Erst nach erfolgreicher Lernprüfung und gegebenenfalls Hold-out wird ein gespeicherter Kandidat normalisiert. Zusätzlich werden ursprünglicher Baum und Normalform an sämtlichen gelieferten x-Werten verglichen, einschließlich Polstellen und widerlegter Hold-out-Punkte.

Die erste Herkunft bleibt Repräsentant der Gruppe. `expression_origins` speichert IDs, Samplebefunde, Formeltexte und Originalbäume sämtlicher zusammengeführter Lernkandidaten. Die Gruppierung ersetzt keine unabhängige Evidenz; unterschiedliche Schreibweisen auf denselben Samples sind keine zusätzlichen unabhängigen Belege. Ein unerwarteter Statuskonflikt in einer Gruppe stoppt den Lauf unvollständig.

Alle Polynomrechnungen und Zusatzvergleiche zählen vor Ausführung zum gemeinsamen Operationsbudget. `normalization_operations` weist diese Teilkosten aus; `planning_operations` bleibt die Grammatik-/Erzeugungsarbeit. Symbolische Koeffizienten unterliegen dem bestehenden Bitlimit. Polynomzwischenformen sind zusätzlich auf 65 Koeffizienten begrenzt. Diese Ressourcengrenzen sind keine Aussage über mathematische Unmöglichkeit.

`--max-results` begrenzt im neuen Modus die gespeicherten ursprünglichen Lernkandidaten, nicht nur die Zahl ihrer Gruppen. Auch bei vielen gleichen Formeln bleibt damit die Herkunftsliste begrenzt. `stored_source_hypotheses` zählt diese Herkünfte einschließlich eines gegebenenfalls begonnenen unvollständigen Befunds. Wird die Normalisierung unterbrochen, bleibt dieser Befund separat `INCOMPLETE`; eine vorhandene Teilnormalform hebt diesen Status nicht auf. Jede unvollständige Enumeration hält den Gesamtstatus unvollständig.

## Replay und Z-Projektion

Neuer Container `BRUCE_EXPRESSION_BUNDLE_V2`, Engine `bruce-expression-v2`, Policy `defined-expression-samples-v2`, Gleichheitsregel `rational-domain-v1`. Grammatikregel `binary-arithmetic-v1` und Planer `size-ranked-v1` bleiben bestehen. Der Replay berechnet Normalformen, Ursprünge, Budgets und Trace erneut und vergleicht das vollständige Ergebnis. Eine nachträglich neu berechnete äußere Prüfsumme macht veränderte Ergebnisdaten nicht gültig.

Der V1-Pfad bleibt unverändert. Die Z-Projektion importiert eine überlebende Gruppe als Kandidaten und wertet deren Originalrepräsentanten aus; Definitionslücken und die bestehende Int64-Wertgrenze bleiben wirksam. Auswahl-Replay enthält den Quellrun und benötigt dessen ursprüngliche Datei nicht.

## Abnahme und Grenze

Identische Bruchfunktionen mit gleicher Ausschlussbedingung zusammenführen; x/x von 1 getrennt halten; Herkunftsmengen gegen den bisherigen Modus vergleichen; Kürzung, verschachtelte Division, Nullmultiplikation und Bruchkonstanten prüfen; alle kleinen Abbruchpositionen und Arbeitsgrenzen replayen; manipulierte Normalform und Herkunft zurückweisen; V1-Fixture unverändert; vollständige Debug-/UBSan-Tests und Demo. Nachweise im [Abnahmebericht](acceptance/expression-equivalence-v1-2026-09-22.md).

Die Normalform begründet die Gleichheit der zusammengeführten Ausdrücke. Die vermutete Beziehung zwischen Eingabedaten bleibt ein Samplebefund, kein allgemeiner Beweis. Portfolio-/Experience-Einbindung, weitere Operatoren und allgemeinere Grammatiken gehören nicht zu diesem Paket.
