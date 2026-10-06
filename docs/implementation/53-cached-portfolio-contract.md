# Teilbaumcache im Portfolio und in der Erfahrungsplanung – Core 0.25

Freigegebene Fortsetzung von Core 0.24: Das Fünferportfolio unterstützt `cached-grammar` als Ausdrucksplaner. Feste, adaptive und erfahrungsbasierte Reihenfolge sowie sämtliche vorhandenen Operatorauswahlen einschließlich literaler Potenzen sind integriert.

## Ausführung und Budget

`research --strategy portfolio-expression --expression-planner cached-grammar` verwendet den bestehenden Teilbaumcache ausschließlich im Ausdruckskind. Die übrigen vier Familien behalten ihre bisherigen Aufträge. Ohne explizite Auswahl bleibt der bisherige Ausdrucksplaner aktiv.

Jeder Ausdrucksteilrun erzeugt eine neue Grammatikinstanz mit leerem Cache. Die bestehende Policy `direct-map-64-size3-v1` begrenzt ihn auf 64 echte Teilbäume mit jeweils drei Knoten. Rechenwerte werden nicht gespeichert; Kandidatenrangfolge, IDs, Hauptrechnung, Gegenprüfung, Hold-out und Normalisierung bleiben unverändert. Es gibt keinen gemeinsamen Cache zwischen Familien, Erfahrungsprüfung und neuer Zielsuche.

Die Cachekosten gehören zu den Planungsoperationen des Ausdruckskindes und damit zum gemeinsamen Operationsbudget des Portfolios. Checkpoints, Kandidaten-, Ergebnis- und Tracegrenzen gelten weiter. Unvollständige Kinder oder Zusammenführungen halten den Gesamtstatus auf INCOMPLETE. Cachezähler stehen im Ausdrucksteilrun; der Elternlauf weist weiterhin die aufsummierte Arbeit aus. Herkunft und Definitionslücken bleiben bei der familienübergreifenden Zusammenführung erhalten.

## Forschungsexporte und Replay

Alle Portfolios mit dem Cacheplaner verwenden neue Versionen, unabhängig von der Operatorauswahl:

| Reihenfolge | Container | Engine | Planerversion |
| --- | --- | --- | --- |
| Fest | BRUCE_PORTFOLIO_BUNDLE_V20 | bruce-portfolio-v20 | five-family-cached-v1 |
| Adaptiv | BRUCE_PORTFOLIO_BUNDLE_V21 | bruce-portfolio-v21 | sample-shape-v7 |
| Erfahrung | BRUCE_PORTFOLIO_BUNDLE_V22 | bruce-portfolio-v22 | experience-order-v7 |

Die bestehenden Regeln `normal-form-union-v1`, `shared-budget-v2` und `rational-domain-v1` bleiben erhalten. Explizite Felder sind:

- `expression_planner`: `subtree-cache-v1`;
- `expression_operators`: kanonische nichtleere Auswahl aus `+-*/^`;
- `cache_policy`: `direct-map-64-size3-v1`;
- `power_exponent_policy`: `literal-2-to-8-v1`, auch bei Auswahl ohne `^`.

Die Exponentenpolicy erweitert die gewählte Grammatik nicht. Der Formplaner bleibt dieselbe Heuristik; seine neue Versionskennung bindet ihn an den Cachevertrag. Das Ausdruckskind verwendet seine vorhandene Engine V6.

Replay validiert die Felder und erzeugt den Lauf einschließlich Cachezustand, Kosten, Abbrüchen, Trace und Herkünften erneut aus leerem Speicher. V22 enthält zusätzlich den damaligen Erfahrungssnapshot mit vollständigen Quellnachweisen. Historische Portfolio-Versionen erlauben den Cacheplaner nicht nachträglich; ihre bisherigen Exporte bleiben unverändert reproduzierbar.

Z-Import und Auswahl-Replay akzeptieren V20–V22. Ihre Herkunftskennung enthält Portfolio-Version, Operator-Bitmaske, Cachemerkmal und Zahlendomäne. Die vorhandenen Maskenbits bleiben `+`=1, `-`=2, `*`=4, `/`=8, `^`=16. Polstellen, Int64-Projektionsgrenzen und die Sperre unvollständiger Gesamtbefunde bleiben unverändert.

## Routing-Erfahrung V7

`BRUCE_EXPERIENCE_V7`, Engine `bruce-experience-v7`, Policy `routing-experience-v7` akzeptieren unabhängige V20-/V21-Quellen. V22 bleibt als neue Erfahrungsquelle gesperrt. Quellen benötigen wie bisher tatsächlichen Replay, vollständige Ausführung, Hold-out und mindestens eine bestätigte Hypothese.

Der neue Kontext `routing-context-v7-five-cached` enthält Planer-, Cache- und Exponentenpolicy, kanonische Operatorauswahl sowie die bisherigen Domänen-, Suchgrenzen-, Gleichheits- und Formmerkmale. Historische ungecachete Kontexte und andere Operatorauswahlen wirken nicht auf diesen Kontext. Ein leerer Cache ist der feste Anfangszustand; sein Inhalt wird nicht zum Erfahrungswissen.

Alte und neue Quellen können in einem V7-Speicher liegen. Das erste Cacheereignis verlangt die neue Speicherversion; frühere Ereignisse und Record-IDs bleiben erhalten. Die Rekonstruktion aus Ereignissen verhindert einen bloßen Format-Downgrade. Globale Deduplizierung gleicher Lernpaare, Selbstausschluss, Widerruf und historische Snapshots gelten weiterhin.

Die Grenzen bleiben 8 Records, 16 Ereignisse, 1 MiB und höchstens eine Million deklarierte Suchoperationen pro Quelle. Der Aufwand für Quellenprüfung wird separat von der neuen Suche ausgewiesen. Erfahrungsstimmen ordnen Suchwege, verändern aber keine mathematischen Wahrheits- oder Freigabegates.

## Abnahme und Grenzen

[Benutzung](54-cached-portfolio-usage.md) und [Abnahmebericht](acceptance/cached-portfolio-v1-2026-09-22.md) dokumentieren Beispiele, Abbrüche und Versionsprüfungen. Die gemessene feste Portfolioarbeit sinkt bei identischen Befunden von 93523 auf 87086 logische Einheiten. Andere Aufgaben können Cache-Mehrarbeit verursachen; eine allgemeine CPU-Beschleunigung wird nicht behauptet.

Weitergehende Suchoptimierungen, allgemeinere Operatoren und Grammatiken, vollständigere Definitionsbereichsvergleiche, externe Evidenz und umfassendere Experience-Korrekturketten bleiben offen. Dies ist kein vollständiger M2-/M4-Abschluss.
