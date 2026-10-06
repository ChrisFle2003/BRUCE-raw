# Historische Replay-Fixtures

`research-v1.bruce` wurde am 22.09.2026 mit dem noch unveränderten Core-0.1-Binary aus `build-ubsan/bruce` erzeugt, bevor dieses Binary auf Core 0.2 neu gebaut wurde. Es verwendet die mitgelieferten affinen Lern-/Hold-out-Paare und den Parameterbereich `[0,3]`.

Der Integrationstest vergleicht die Replay-Ausgabe des aktuellen Programms mit dem im historischen Paket gespeicherten Ergebnis. Die Fixture nicht bei normalen Implementierungsänderungen neu erzeugen: Sie schützt das bisherige Replay-Verhalten.

`compatibility-v1.bruce` wurde am selben Tag mit dem unveränderten Core-0.2-Binary aus `build-ubsan/bruce` vor dessen Ausbau auf Core 0.3 erzeugt. Es enthält `examples/compatibility.rules` und `examples/compatibility-candidates.csv`. Die Exact-Domain-Tests schützen damit auch den historischen Z-Replay.

`exact-v1.bruce` wurde mit dem Core-0.3-Binary aus `build/bruce` vor dessen Ausbau auf Core 0.4 erzeugt. Es enthält die rationalen Beispielpaare und den Zählerbereich [-2,2] bei Nennergrenze 3. Die Power-Research-Tests prüfen das unveränderte Ergebnis einschließlich Versionen und Trace.

`power-v1.bruce` wurde mit dem Core-0.4-Binary aus `build/bruce` vor dessen Ausbau auf Core 0.5 erzeugt. Es enthält die nichtlinearen Beispielpaare mit Koeffizientenbereich [0,3]. Die Adaptive-Research-Tests prüfen den unveränderten historischen Replay.

`adaptive-v1.bruce` wurde mit dem Core-0.5-Binary vor dessen Ausbau auf Core 0.6 erzeugt. Es enthält den adaptiv geplanten Potenz-Beispiellauf samt Hold-out unter Standardgrenzen. Die Polynomial-Tests prüfen dessen unveränderten Replay.

`polynomial-v1.bruce` wurde mit dem Core-0.6-Binary vor dessen Ausbau auf Core 0.7 erzeugt. Es enthält den mehrtermigen Polynom-Beispiellauf samt Hold-out. Die Portfolio-Tests prüfen dessen unveränderten Replay.

`portfolio-v1.bruce` wurde mit dem noch unveränderten Core-0.7-Binary vor dessen Ausbau auf Core 0.8 erzeugt. Es enthält den gemeinsamen Potenz-/Polynomlauf mit `portfolio-training.csv` und `power-holdout.csv`. Die Rational-Linear-Tests schützen den unveränderten historischen Replay.

`rational-linear-v1.bruce` wurde vor dem Ausbau auf Core 0.9 mit dem unveränderten Core-0.8-Binary erzeugt. Es enthält die Bruchfunktions-Beispielpaare samt Hold-out. Die Rational-Portfolio-Tests schützen dessen historischen Replay einschließlich Definitionsbereich und Erklärung.

`portfolio-v2.bruce` wurde mit dem unveränderten Core-0.9-Binary vor Ausbau auf Core 0.10 gesichert. Es enthält den festen Dreistrategien-Lauf auf den Bruchfunktions-Beispielpaaren mit Polynomgradgrenze 2. Die Adaptive-Portfolio-Tests prüfen den historischen Replay.

`portfolio-v3.bruce` wurde vor Ausbau auf Core 0.11 mit dem unveränderten Core-0.10-Binary erzeugt. Es enthält den adaptiv geordneten Lauf auf den Bruchfunktions-Beispielen. Die Experience-Tests schützen den unveränderten Replay des ursprünglichen Formplaners.

`portfolio-v4.bruce` wurde vor dem Ausbau auf Core 0.12 mit dem unveränderten Core-0.11-Binary erzeugt. Es enthält den mit Erfahrung geplanten Ziellauf aus den Experience-Beispielen einschließlich eingebettetem Quelllauf und Snapshot. Die Rational-Quadratic-Tests schützen den historischen Replay samt Erfahrungsnachweis.

`rational-quadratic-v1.bruce` wurde mit dem unveränderten Core-0.12-Binary vor dessen Ausbau auf Core 0.13 gesichert. Es enthält `1/(x²+1)` mit Lern- und Hold-out-Paaren. Die Quadratic-Portfolio-Tests schützen den ursprünglichen Einzelstrategie-Replay einschließlich Nennerbedingung.

`portfolio-v6.bruce` wurde mit dem unveränderten Core-0.13-Binary vor Ausbau auf Core 0.14 erzeugt. Es enthält das adaptiv geordnete Viererportfolio für `1/(x²+1)` samt Hold-out. Die Expression-Tests schützen dessen historischen Replay.

`expression-v1.bruce` wurde mit dem unveränderten Core-0.14-Binary vor Ausbau auf Core 0.15 erzeugt. Es enthält die Ausdruckssuche für x²/(x+1) mit Konstante 1 und sieben Knoten samt Hold-out. Die Expression-Equivalence-Tests schützen den ursprünglichen Syntaxmodus mit seinen fünf getrennten Lernkandidaten und Gesamtstatus AMBIGUOUS.

`expression-v2.bruce` wurde mit dem unveränderten Core-0.15-Binary vor Ausbau auf Core 0.16 erzeugt. Es enthält die normalisierte Ausdruckssuche für x²/(x+1), zwei Gruppen und fünf Ursprungsbäume. Die Expression-Portfolio-Tests schützen den unveränderten Einzelstrategie-Replay einschließlich Normalisierungskosten, Ausschlusspolynomen und Herkunft.

`portfolio-v8.bruce` wurde mit dem unveränderten Core-0.16-Binary vor Ausbau auf Core 0.17 erzeugt. Es enthält das feste Fünferportfolio für x²/(x+1) mit sieben Baumknoten samt Hold-out. Die Five-Experience-Tests schützen das historische Ergebnis einschließlich Normalformen, Gruppen-IDs, Herkunft und globaler Kosten.

`portfolio-v10.bruce` wurde mit dem unveränderten Core-0.17-Binary vor Ausbau auf Core 0.18 erzeugt. Es enthält das Fünferportfolio auf den Ausdrucks-Erfahrungszielpaaren, einschließlich eingebettetem V3-Snapshot und unabhängiger V9-Quelle. Die Indexed-Expression-Tests schützen den historischen Replay mit damaliger Reihenfolge, Herkunft, Kosten und Speicherstand.

`expression-v3.bruce` wurde vor Ausbau auf Core 0.19 mit dem unveränderten Core-0.18-Binary erzeugt. Es enthält den indizierten Ausdruckslauf mit rationaler Zusammenführung für x²/(x+1). Die Indexed-Portfolio-Tests schützen den historischen Stand einschließlich Indexmetadaten und prüfen zusätzlich die bisherigen V8-/V10-Fixtures.

`portfolio-v13.bruce` wurde vor Ausbau auf Core 0.20 mit dem unveränderten Core-0.19-Binary erzeugt. Es enthält den erfahrungsbasierten indizierten Fünferlauf auf den Ausdruckszielpaaren samt V4-Snapshot und adaptiver V12-Quelle. Die Operatorauswahl-Tests sichern dessen unveränderten Replay.

`expression-v4.bruce` wurde vor Ausbau auf Core 0.21 mit dem unveränderten Core-0.20-Binary erzeugt. Es enthält die eigenständige indizierte Suche nach x²+x mit Operatorauswahl `+*` und rationaler Zusammenführung. Die Portfolio-Operator-Tests prüfen ihren unveränderten historischen Replay.

`portfolio-v16.bruce` wurde vor Ausbau auf Core 0.22 mit dem unveränderten Core-0.21-Binary erzeugt. Es enthält das erfahrungsbasierte Fünferportfolio mit indizierter Ausdrucksgrammatik `+*`, eingebettetem V5-Snapshot und unabhängiger V14-Quelle. Die Expression-Powers-Tests schützen den historischen Replay einschließlich damaliger Kosten und Erfahrungskontexte.

`expression-v5.bruce` wurde vor Ausbau auf Core 0.23 mit dem unveränderten Core-0.22-Binary erzeugt. Es enthält die eigenständige indizierte Ausdruckssuche für x³+x mit `+^`, Konstanten 1–3, fünf Knoten und rationaler Zusammenführung. Die Portfolio-Powers-Tests sichern den historischen Replay einschließlich Exponentenpolicy, Kosten und Herkunft.

`portfolio-v19.bruce` wurde vor Ausbau auf Core 0.24 mit dem unveränderten Core-0.23-Binary erzeugt. Es enthält das erfahrungsbasierte Potenzportfolio auf den Zielpaaren für x³+x, mit indiziertem Ausdrucksplaner, eingebettetem V6-Snapshot und unabhängiger V17-Quelle. Die Cached-Expression-Tests sichern den historischen Replay.

`expression-v6.bruce` wurde vor Ausbau auf Core 0.25 mit dem unveränderten Core-0.24-Binary erzeugt. Es enthält die eigenständige Ausdruckssuche für x²/(x+1) mit Cacheplaner und rationaler Zusammenführung. Die Cached-Portfolio-Tests sichern den historischen Replay einschließlich Cachepolicy, Zählern und Kosten.
