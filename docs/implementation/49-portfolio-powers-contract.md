# Potenzausdrücke im Fünferportfolio – Core 0.23

Freigegebene Fortsetzung von Core 0.22: Der vorhandene literale Potenzoperator `^` steht auch im Ausdruckskind des Fünferportfolios zur Verfügung. Beide Ausdrucksplaner und feste, adaptive sowie erfahrungsbasierte Reihenfolge sind unterstützt. Die bestehenden Regeln zur Exaktheit und zu Ausschlussbedingungen bleiben maßgeblich.

## Ausführung und Begrenzung

`research --strategy portfolio-expression --operators '+^'` wählt die Ausdrucksgrammatik. Die anderen vier Familien behalten ihre mathematischen Definitionen und Parametergrenzen. `--max-power` begrenzt weiterhin die separate Potenzformelfamilie; das Ausdruckskind verwendet ausschließlich literale Exponenten 2 bis 8 aus seinem deklarierten Konstantenvorrat. Die vollständige Operatorauswahl einschließlich Potenzen ist `+-*/^`; der bisherige Standard ohne explizite Auswahl bleibt `+-*/`.

Feste Reihenfolge (`portfolio`), Formheuristik (`adaptive-order`) und Erfahrungsreihenfolge (`experience-order`) laufen mit `grammar` oder `indexed-grammar` als Ausdrucksplaner. Alle fünf Familien bleiben Teil des Gesamtauftrags. Andere Kinder erhalten weiterhin die Standardoperatoren; ausschließlich das Ausdruckskind bekommt die ausgewählte Grammatik und rationale Normalisierung.

Die bestehenden gemeinsamen Budgets, Checkpoints und Abbruchregeln bleiben erhalten. Unvollständige Kinder oder Zusammenführungen halten den Gesamtstatus auf INCOMPLETE. Originalbäume bleiben im Ausdrucksteilrun, familienübergreifende Herkünfte an den zusammengeführten Normalformen erhalten. Definitionslücken werden weder durch Kürzung noch durch positive Potenzierung aufgehoben. Externe Evidenz oder ein allgemeiner Beweis werden durch Familienübereinstimmung nicht erzeugt.

## Versionsvertrag

Nur Fünferportfolios mit `^` verwenden die neuen Versionen:

| Reihenfolge | Container | Engine | Planerversion |
| --- | --- | --- | --- |
| Fest | BRUCE_PORTFOLIO_BUNDLE_V17 | bruce-portfolio-v17 | five-family-power-v1 |
| Adaptiv | BRUCE_PORTFOLIO_BUNDLE_V18 | bruce-portfolio-v18 | sample-shape-v6 |
| Erfahrung | BRUCE_PORTFOLIO_BUNDLE_V19 | bruce-portfolio-v19 | experience-order-v6 |

Alle drei behalten `normal-form-union-v1`, `shared-budget-v2` und `rational-domain-v1`. Explizite Felder sind `expression_operators`, `expression_planner` (`size-ranked-v1` oder `block-index-v1`) und `power_exponent_policy` (`literal-2-to-8-v1`). Die Formheuristik selbst ist unverändert; ihre neue Planerversion bindet sie an den erweiterten Suchvertrag. Das Ausdruckskind verwendet seine bestehende Engine V5.

Replay verlangt kanonische Operatoren mit `^`, einen bekannten Ausdrucksplaner und exakt die deklarierte Exponentenpolicy. Alte Portfolio-Versionen V14–V16 erlauben weiterhin keine Potenzen. Der komplette Lauf mit Kindern, Kosten, Herkunft, Abbruchpositionen und Trace wird reproduziert und kanonisch verglichen. V19 enthält den damaligen Erfahrungssnapshot mit seinen vollständigen Quellnachweisen. Ohne `^` bleiben historische Formatpfade und Ergebnisse unverändert.

Z-Import und Auswahl-Replay unterstützen V17–V19. Ihre Herkunftskennung enthält Portfolio-Version, Operator-Bitmaske, Ausdrucksplaner und Zahlendomäne. `^` ist weiterhin Bit 16 neben `+`=1, `-`=2, `*`=4 und `/`=8. Polstellensperren, Int64-Projektionsgrenzen und die Sperre unvollständiger Gesamtbefunde gelten unverändert.

## Erfahrung V6

`BRUCE_EXPERIENCE_V6`, Engine `bruce-experience-v6`, Policy `routing-experience-v6` nehmen unabhängige Quellläufe aus V17 und V18 auf. Ein V19-Lauf bleibt als neue Erfahrungsquelle gesperrt. Quellen werden tatsächlich replay-geprüft und benötigen einen vollständigen Lauf mit Hold-out und mindestens einer bestätigten Hypothese.

Der Kontext `routing-context-v6-five-power` enthält die Exponentenpolicy `literal-2-to-8-v1`, kanonische Operatorauswahl, Ausdrucksplanerversion sowie die bestehenden Zahlendomänen-, Suchgrenzen-, Gleichheits- und Formmerkmale. Eine Quelle mit `+^` kann weder `+*^` noch eine Grammatik ohne Potenzen beeinflussen. Ebenso bleiben verschiedene Ausdrucksplaner getrennt.

Gemischte Speicher dürfen historische Quellen und neue Potenzquellen gemeinsam enthalten. Sobald eine Potenzquelle hinzugefügt wird, wird V6 benötigt. Vorhandene Ereignisse und Record-IDs bleiben unverändert. Die Rekonstruktion aus Ereignissen bestimmt die Speicherversion; ein bloß umbenannter V5-Container darf keine V6-Quelle enthalten. Widerruf entfernt Planungseinfluss, erhält aber Ereignisse und historische Snapshots.

Identische Lernpaare werden weiterhin global dedupliziert und vom eigenen Planungseinfluss ausgeschlossen, auch bei anderer Grammatik. Die Grenzen bleiben 8 Records, 16 Ereignisse, 1 MiB und maximal eine Million deklarierte Suchoperationen je Quelle. Die Quellenprüfung wird getrennt von der neuen Suche ausgewiesen. Erfahrungsstimmen verändern die Reihenfolge, keine Wahrheits- oder Freigabegates.

## Abnahme und weitere Arbeit

[Benutzung](50-portfolio-powers-usage.md) und [Abnahmebericht](acceptance/portfolio-powers-v1-2026-09-22.md) beschreiben Beispiele und Prüffälle. Abgedeckt sind alle sechs Planerkombinationen, gemeinsame Budget- und Abbruchreihen, Familienisolation und Zusammenführung, getrennte und gemischte Erfahrungskontexte, Widerruf, manipulierte Exporte, historische Replays und Auswahl ohne Ursprungsdateien.

Allgemeinere Operatoren oder Exponenten, weitere Suchoptimierungen, vollständigere Definitionsbereichsvergleiche und umfassendere Experience-Verträge bleiben offen. Dies ist kein vollständiger M2-/M4-Abschluss.
