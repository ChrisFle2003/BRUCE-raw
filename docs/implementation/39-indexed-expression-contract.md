# Indizierte Ausdruckserzeugung – Core 0.18

Freigegebene Fortsetzung der effizienteren Ausdruckssuche: Der optionale Planer `indexed-grammar` ergänzt die vorhandene Strategie `expression`. Er beschleunigt die Auswahl eines Grammatikblocks anhand vorbereiteter Rangintervalle. Grammatik, Kandidatenreihenfolge, IDs, Bäume und mathematische Prüfungen bleiben erhalten.

## Index und Enumeration

Der vorhandene `ExpressionGrammar` berechnet weiterhin die Anzahl der Ausdrücke jeder ungeraden Größe bis neun Knoten. Der neue Modus hält zusätzlich für jeden zulässigen Operator-/Teilgrößenblock dessen Operator, linke und rechte Baumgröße sowie Beginn und Ende des Rangintervalls fest. Reine Kommutativitätsduplikate werden wie bisher ausgeschlossen.

Beim Erzeugen eines Teilbaums wird das passende Intervall binär gesucht. Innerhalb des Blocks bleibt die bisherige Aufteilung in linke und rechte Ränge erhalten: kartesisches Produkt für geordnete Operationen, Dreiecksrangfolge für gleich große kommutative Teilbäume. Die Rekursion erzeugt denselben Postorder-Baum wie der bisherige Planer.

Es gibt maximal 32 Indexeinträge: 4 für Größe 3, 6 für Größe 5, 10 für Größe 7 und 12 für Größe 9. Terminalsymbole benötigen keine Blockeinträge. Der Index hält weder sämtliche Ausdrucksbäume noch Samplewerte vor. Der größte beschreibbare Raum bleibt 1327858375545 Ausdrücke; einzelne Bäume werden weiterhin lazy erzeugt.

Der ursprüngliche Konstruktor-/Enumerationspfad bleibt Standard. Sein Index ist leer, seine bisherige logische Kostenabrechnung und historische Replays bleiben unverändert. Die öffentlich nutzbare Grammatik erlaubt auch keine Konstanten, also nur das Terminal x; dieser Grenzfall wird separat geprüft. Die CLI behält ihren bisherigen nichtleeren Konstantenbereich.

## Budgets und Befunde

Indexaufbau zählt zur Planung: ein zusätzlicher Arbeitsschritt vor jedem gespeicherten Intervall. Binäre Vergleiche bei der Auswahl eines Blocks werden vor Ausführung berechnet. Die bisherige Erzeugungs- und Dreiecksrangarbeit bleibt budgetiert. `planning_operations` enthält Aufbau und Benutzung des Index; `operations_used` enthält zusätzlich die unveränderte Auswertung, Gegenrechnung und gegebenenfalls Normalisierung.

`index_entries` nennt die fertigen Indexeinträge; vor vollständig abgeschlossener Grammatikplanung steht der Wert wie die Grammatikgröße auf 0. Kandidaten-, Ergebnis-, Trace-, Bit- und Abbruchgrenzen bleiben wirksam. Ein größerer Index ist keine Ausnahme vom Operationsbudget. Ein unterbrochener Aufbau oder Baum ergibt INCOMPLETE, keinen mathematischen Ausschluss.

Beide vorhandenen Gleichheitsmodi werden unterstützt: `syntax` und `rational`. Im vollständigen Lauf müssen Hypothesen einschließlich Originalbäumen, Status, Normalformen, Definitionsbedingungen und Herkunft exakt dem bisherigen Modus entsprechen. Unverändert bleiben rekursive Vorwärtsauswertung, separate Ganzzahl-Bruch-Gegenrechnung, Hold-out und sämtliche ursprünglichen Divisionsbedingungen.

Die Optimierung garantiert keine Ersparnis für jeden einzelnen Rang oder jede winzige Aufgabe. Der Indexaufbau hat Zusatzkosten; binäre Auswahl kann bei einem sehr frühen Block mehr Schritte als der direkte erste Treffer benötigen. Abgenommen wird eine konkrete Reduktion auf dem bestehenden Referenzlauf, keine pauschale CPU-/Laufzeitbeschleunigung.

## CLI, Version und Projektion

`research --strategy expression --planner indexed-grammar` aktiviert den neuen Modus. Der Planer ist für andere Strategien ausdrücklich unzulässig. Bestehende Portfolios verwenden weiterhin ihren bisherigen Ausdrucksplaner; seine spätere Einbindung benötigt eigene Portfolio- und Experience-Verträge.

Container `BRUCE_EXPRESSION_BUNDLE_V3`, Engine `bruce-expression-v3`, Planer `block-index-v1`, Grammatikregel `binary-arithmetic-v1`, Policy `defined-expression-samples-v3`. Die Gleichheitsversion wird explizit als `syntax-v1` oder `rational-domain-v1` gespeichert. Replay baut den Index erneut auf und vergleicht Eingaben, Bäume, Ergebnis, Kosten, Indexgröße und Trace.

Z-Projektion importiert die V3-Ergebnisse und wertet weiterhin die Originalbäume aus. Polstellen, Int64-Wertgrenzen und die Sperre für unvollständige Gesamtläufe bleiben bestehen. Auswahl-Replay enthält den Ursprungsrun. Historische Ausdrucks- und Portfolio-Versionen einschließlich eingebetteter Erfahrung behalten ihren bisherigen Pfad.

## Abnahme

Neue CLI-Anforderung zuerst rot; beide Gleichheitsmodi mit exakt gleichen Hypothesen und weniger Planungsarbeit auf dem Referenzfall; gleiche Operationsgrenze lässt neuen Lauf vollständig und alten unvollständig enden; Indexgröße begrenzt; sämtliche Bäume kleiner Räume und große Rand-/Zufallsränge gegen den alten Generator; alle kleinen Operations-/Checkpoint-/Kandidatenabbrüche replaybar; manipulierte Versionen und Metadaten zurückweisen; historische Fixtures einschließlich tatsächlichem Core-0.17-V10; unabhängiges Review; vollständige Debug-/UBSan-Suiten und Demo. [Abnahmebericht](acceptance/indexed-expression-v1-2026-09-22.md).
