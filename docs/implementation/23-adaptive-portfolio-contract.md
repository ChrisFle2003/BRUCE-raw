# Core 0.10: datenabhängige Reihenfolge im Portfolio

Die freigegebene Fortsetzung ergänzt den bestehenden Dreistrategien-Modus um `--planner adaptive-order`. Kein neuer Formelraum: Alle drei Familien bleiben Teil der Suche. Planung ist ein begrenzter Hinweis, keine Hypothesenprüfung und kein gelerntes Ranking.

## Auswahlregel sample-shape-v1

Es werden höchstens die ersten vier Lernpaare in ihrer Eingabereihenfolge gelesen. Kein Hold-out-Zugriff, keine Sortierung oder externen Erfahrungen. Alle Eingaben wurden zuvor durch den gemeinsamen Taskvalidator normalisiert und auf doppelte x-Werte geprüft.

1. Weniger als drei Lernpaare: feste Reihenfolge Potenz, Polynom, Bruchfunktion; Grund `insufficient_shape_samples`.
2. Gleiche exakte erste dividierte Differenzen auf den ersten drei Paaren: dieselbe Reihenfolge; Grund `affine_prefix`.
3. Sonst, wenn alle ersten drei y-Werte ungleich null sind: gleiche dividierte Differenzen für 1/y setzen die Bruchfunktion zuerst, dann Potenz und Polynom; Grund `reciprocal_affine_prefix`.
4. Sonst, bei mindestens vier Paaren: gleiche zweite dividierte Differenzen setzen das Polynom zuerst, dann Potenz und Bruchfunktion; Grund `quadratic_prefix`.
5. Andernfalls feste Reihenfolge, Grund `no_shape_hint`.

Die Teilstrategien prüfen danach wie bisher sämtliche Lern- und Hold-out-Paare und ihre eigenen Koeffizientenbereiche. Ein passendes Präfix darf auf späteren Lernpaaren scheitern. Auch die Reihenfolge bei konstanten Daten ist bewusst fest und priorisiert nicht die mehrdeutige Bruchfamilie.

## Ressourcen und Abbruch

Jede arithmetische Operation und jeder Wertvergleich der Vorprüfung wird vor Ausführung gegen das gemeinsame Operationsbudget gebucht. Bitlimits gelten auch für Differenzen, Kehrwerte und Quotienten. Planungslimit ergibt INCOMPLETE; keine still unbudgetierte Ausweichplanung.

Ein globaler Checkpoint kommt vor der Vorprüfung hinzu. Trace enthält START, ORDER und END sowie sämtliche inneren Traces. Das ORDER-Ereignis und END werden vor Planung reserviert; bei zu knappen Tracegrenzen wird ohne Vorprüfung beendet. Der Wrapper reserviert 512 Tracebytes vor einem Teilrun, die Teilstrategie erhält den Rest. Reservierung darf konservativer als die spätere tatsächliche Textlänge sein.

`ordering_operations` ist Teil von `planning_operations` und `operations_used`, nicht zusätzliche Arbeit außerhalb des Budgets. `plans_completed` enthält eine erfolgreiche Vorprüfung. Ein erfolgreicher Teilbefund beendet die Gesamtforschung nicht. Abbruch nach frühem Treffer bleibt INCOMPLETE.

## Identität und historische Versionen

Die bestehenden Familie-Indizes bleiben unabhängig von der Ausführungsposition: Potenz 0, Polynom 1, Bruchfunktion 2. IDs und Koordinaten folgen dem Core-0.9-Vertrag. Wird ein zuerst gefundenes Polynom später auch von der Potenzstrategie gefunden, verwendet die Zusammenführung deren bisher bevorzugte ID. V3 sortiert die fertigen Hypothesen nach ID und stellt die Potenzherkunft bei zusammengeführten Polynomen zuerst dar. Vollständig untersuchte Läufe behalten die fachlichen Identitäten des festen Portfolios; begrenzte Teilruns können andere Teilmengen erreichen.

Neuer Container `BRUCE_PORTFOLIO_BUNDLE_V3`, Engine `bruce-portfolio-v3`, Regel `domain-union-v1`, Policy `shared-budget-v1`, Planer `sample-shape-v1`. Replay berechnet Hinweis, Reihenfolge, Kosten, Teilruns und Abbruch erneut. Historische V1-/V2-Ausgaben bleiben unverändert. Z nutzt die tatsächliche Funktion samt Definitionslücke; Kontextkennung `portfolio-v3-rational-v1`.
