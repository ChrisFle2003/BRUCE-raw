# Ausdrucksgruppen mit rationalem Definitionsbereich – Core 0.27

## Umfang

Die freigegebene Fortsetzung integriert Core 0.26 in die eigenständige Ausdruckssuche. `research --strategy expression --equivalence rational-q` vergleicht Definitionsbereiche als Teilmengen von Q. Alle drei bestehenden Planer (`grammar`, `indexed-grammar`, `cached-grammar`), Operatorauswahlen und literalen Potenzen bleiben verwendbar. Der Standard bleibt `syntax`; der bisherige Modus `rational` behält seine konservative Regel und seine historischen Formate.

Portfolio und Experience erhalten diesen Modus noch nicht. Die vorhandene Portfolioprüfung weist ihn zurück. Die Erweiterung ist keine Vergrößerung der Grammatik, der maximalen Baumgröße oder der Speicherbudgets.

## Zusammenführung

Die ursprünglichen Bäume durchlaufen weiterhin Lernprüfung, unabhängige Gegenrechnung und Hold-out. Erst anschließend werden sie normalisiert und auf allen angegebenen Eingaben gegen den ursprünglichen Baum geprüft. Die Normalform behält die ursprünglichen Ausschlüsse, auch wenn eine Division algebraisch gekürzt wurde.

Im neuen Modus werden die bereits gespeicherten Gruppen in ihrer Entstehungsreihenfolge geprüft. Unterschiedliche gekürzte Wertefunktionen werden nicht zusammengeführt. Bei gleicher Wertefunktion vergleicht `rational-domain-gcd-quadratic-v1` die beiden Ausschlusspolynome: Quadratfreiheit, gemeinsamer GGT, dann exakte rationale Nullstellenentscheidung für Restgrade bis 2. Eine unbekannte Entscheidung bedeutet **keine** Zusammenführung. Bei nachgewiesener Gleichheit wird der Ursprung der ersten passenden Gruppe angehängt. Zwei leere Definitionsbereiche gelten als gleich; sie können wegen der nichtleeren Lernmenge allerdings keine regulären Suchtreffer sein.

Die erste Gruppen-ID, ihr Baum und ihre Normalform bleiben unverändert. Alle Originalbäume und ihre Statuswerte bleiben in `expression_origins` erhalten. Ein vorhandener Repräsentant wird nicht durch eine frei erfundene vereinfachte Formel ersetzt. Eine spätere Gruppe wird nicht rückwirkend umsortiert oder mit anderen bereits gespeicherten Gruppen vereinigt. Der konservative Vergleich höherer Restgrade kann mathematisch gleiche Funktionen in getrennten Gruppen belassen.

Die Gleichheit einzelner Formeln ist eine algebraische Aussage über Q. Die Beziehung zu den vorgegebenen Zahlenpaaren bleibt eine Stichprobenhypothese: `general_proof`, LearnGate und PublishGate bleiben false. Irrationale Lücken werden nicht als rationale Eingaben behandelt. `1` und `x/x` bleiben wegen 0 verschieden.

## Budgets, Spur und Abbruch

- Gemeinsames Operationsbudget für Grammatikaufbau, Sampleprüfung, Normalisierung und neuen Gruppenvergleich.
- `domain_comparison_operations`: gesamte zusätzliche Arbeit beim Durchlaufen der Gruppen, Koeffizientenvergleich und Bereichsprüfung. Sie ist Teil von `operations_used`, separat von `normalization_operations` und `planning_operations`.
- `domain_comparisons`: begonnene Bereichsprüfungen nach Gleichheit der Wertefunktion. Ein Abbruch kann eine begonnene, noch nicht abgeschlossene Prüfung enthalten.
- `domain_unknown`: abgeschlossene, unentschiedene Bereichsprüfungen. Eine solche Entscheidung ist kein Rechenabbruch.
- Pro Kandidat ergänzt die vorhandene Spur `domain_checks=N domain_unknown=N`; eine Zusammenführung nennt wie bisher `merge_into=ID`. Die vollständige Berechnung ist aus den gespeicherten Bäumen und der Policy beim Replay wiederherstellbar. Es wird kein unbegrenzt wachsender Katalog aller paarweisen Zertifikate angelegt.
- Die bestehende 512-Byte-Reserve pro Kandidatenereignis deckt auch diese kurzen Zähler ab. Spur-, Byte-, Kandidaten- und Ergebnisgrenzen bleiben erhalten.
- Das Ergebnisbudget zählt weiterhin **Originaltreffer**, höchstens 128, nicht nur verbleibende Gruppen. Genauere Zusammenführung erlaubt keine unbegrenzte Speicherung ihrer Ursprünge.
- Bei Operations-, Bit- oder Normalisierungsgrenze während eines Vergleichs bleibt der aktuelle Kandidat separat als `INCOMPLETE` mit Ursprung erhalten. Erst ein abgeschlossener Gleichheitsnachweis darf die Gruppe verändern.
- Die bestehenden Abbruch-Checkpoints gelten vor Planung und Kandidaten. Neue Bereichsprüfungen sind operationsbegrenzt; es gibt keine zusätzlichen Signal-Checkpoints innerhalb eines Kandidaten.

Eine vollständige Suche kann `AMBIGUOUS` bleiben, weil verschiedene Funktionen, tatsächliche Definitionslücken oder ungelöste Bereichsvergleiche bestehen. `UNDECIDED` aus dem eigenständigen Vergleich wird in der Suche als Zähler/Spur ausgewiesen, nicht als neuer Gesamtstatus. `INCOMPLETE` bleibt von fachlicher Mehrdeutigkeit getrennt.

## Version, Replay und Auswahl

| Feld | Wert |
| --- | --- |
| Export | `BRUCE_EXPRESSION_BUNDLE_V7` |
| Engine | `bruce-expression-v7` |
| Regel | `q-domain-binary-v1` |
| Samplepolicy | `defined-expression-samples-v7` |
| Gleichheit | `rational-q-domain-v1` |
| Bereichsvergleich | `rational-domain-gcd-quadratic-v1` |
| Exponenten | `literal-2-to-8-v1` |

Planer und Operatorauswahl sind explizit. Die Cachepolicy ist `none` oder bei `cached-grammar` unverändert `direct-map-64-size3-v1`. Jeder Replay baut einen leeren Cache auf. Alte Ausdrucks-/Portfolio-/Experience-Versionen werden unverändert gelesen und ausgeführt.

Replay prüft vollständige Eingaben, Versionen, Ergebnis, Herkunft, Zähler, Abbruch und Spur durch erneute Berechnung. Unbekannte oder nichtkanonische Metadaten werden zurückgewiesen. Auswahl akzeptiert V7 mit Herkunft `expression-q-<Operatormaske>-<Planer>-rational-v1`. Sie nutzt den behaltenen Repräsentanten. Da die Einbindung ausschließlich über Q erfolgt, gilt dessen Definitionsbereich an ganzzahligen Projektionsstellen für alle Gruppenmitglieder. Polstellen, nichtganzzahlige Ergebnisse, Int64-Grenzen und unvollständige Forschung bleiben gesperrt. Eingebettetes Auswahl-Replay benötigt die ursprüngliche Forschungsdatei nicht.

## Nachweise und Grenzen

[Benutzung](58-q-expression-usage.md) · [Abnahme](acceptance/q-expression-v1-2026-09-22.md)

Die bestehenden Grenzen können größere Beispiele vor vollständiger Enumeration stoppen. Insbesondere bleibt ein Lauf mit 128 gespeicherten Ursprüngen und weiterem Treffer unvollständig, auch wenn genaueres Gruppieren die Gruppenzahl reduziert. Aus einem solchen Beispiel werden weder vollständige Suche noch freigegebene Auswahl abgeleitet.

Portfolio-/Experience-Einbindung, allgemeinere Nullstellenentscheidungen, effizientere Paarvergleiche und weitere Roadmap-Pakete bleiben offen. Kein vollständiger M2-/Gesamtprojektabschluss.
