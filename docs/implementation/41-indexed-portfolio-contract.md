# Indizierte Ausdruckssuche im Portfolio – Core 0.19

Freigegebene Fortsetzung von Core 0.18: Das Fünferportfolio kann seinen Ausdrucksplaner explizit auswählen. Die Einbindung umfasst feste, adaptive und erfahrungsbasierte Reihenfolgen, gemeinsame Budgets, Erfahrungskontexte, Replay und Z-Projektion.

## Auswahl und Ausführung

`research --strategy portfolio-expression --expression-planner grammar|indexed-grammar` wählt den Ausdrucksplaner. Standard ist `grammar`; `--planner portfolio|adaptive-order|experience-order` wählt unabhängig davon die Reihenfolge der fünf Familien. Die CLI weist `--expression-planner` bei anderen Strategien zurück. Die direkte API prüft den Wert und erlaubt den nicht standardmäßigen Modus ebenfalls nur für das Fünferportfolio.

Nur das Ausdruckskind verwendet `indexed-grammar`; die übrigen vier Familien behalten ihre bisherigen Planer. Das Portfolio setzt die nur für den Elternlauf gedachte Auswahl im Kind zurück. Rationale Normalformen, Ausschlusspolynome, Gruppen-IDs, Originalbäume und Herkunft bleiben gleich. Der Formplaner und die Stimmen bestimmen weiterhin nur die Reihenfolge; keine Familie wird übersprungen.

Der Index bleibt auf maximal 32 Intervalle begrenzt. Aufbau und Benutzung verbrauchen das verbleibende globale Operationsbudget; `planning_operations` enthält diese Arbeit. `index_entries` steht im Ausdrucks-Teilrun, `expression_planner: block-index-v1` im neuen Gesamtergebnis. Kandidaten-, Ergebnis-, Bit-, Trace-, Checkpoint- und Callback-Grenzen bleiben wirksam. Ein abgebrochenes Kind oder eine unvollständige Zusammenführung hält den Gesamtbefund auf INCOMPLETE.

## Versionierung

| Reihenfolge | Neuer Container | Engine | Reihenfolgeplaner |
| --- | --- | --- | --- |
| Fest | BRUCE_PORTFOLIO_BUNDLE_V11 | bruce-portfolio-v11 | five-family-indexed-v1 |
| Adaptiv | BRUCE_PORTFOLIO_BUNDLE_V12 | bruce-portfolio-v12 | sample-shape-v4 |
| Erfahrung | BRUCE_PORTFOLIO_BUNDLE_V13 | bruce-portfolio-v13 | experience-order-v4 |

Alle drei speichern explizit `expression_planner block-index-v1`, weiterhin `normal-form-union-v1`, `shared-budget-v2` und `rational-domain-v1`. Replay baut die Kinder erneut auf und vergleicht das gesamte kanonische Ergebnis einschließlich Kosten, Indexgröße, Herkunft und Trace. V13 enthält den damaligen Erfahrungssnapshot mit seinen Quellnachweisen. Fremde Versionen und manipulierte Ergebnisdaten werden zurückgewiesen.

Der Standardpfad behält V8/V9/V10 samt Kosten, JSON-Feldern und Trace. Historische Ausdrucks-V3-Läufe bleiben ebenfalls unverändert. Z-Projektion verarbeitet V11–V13 über die bestehenden Normalformen und ihre Ausschlussbedingungen; ihre Herkunftskennungen unterscheiden die neuen Versionen. Polstellen und unvollständige Gesamtläufe bleiben gesperrt. Auswahl-Replay enthält den Forschungsnachweis.

## Erfahrungsspeicher V4

`BRUCE_EXPERIENCE_V4`, Engine `bruce-experience-v4`, Policy `routing-experience-v4` erweitern den Speicher um unabhängige V11-/V12-Quellen. V13 ist als Quelle unzulässig, genau wie die bisherigen erfahrungsbasierten Quellen. Quellläufe müssen vollständig sein, Hold-out enthalten und mindestens eine auf Samples bestätigte Hypothese liefern. Quellen werden durch Replay geprüft, nicht allein durch ihr gespeichertes Ergebnis.

Der Indexkontext verwendet `routing-context-v4-five-indexed` und `block-index-v1` zusätzlich zu den bisherigen Domänen-/Suchgrenzen, Gleichheitsmodus und Formhinweis. Die bisherige Kontextberechnung für `grammar` bleibt unverändert. Stimmen einer anderen Planerart beeinflussen das Ranking nicht, auch wenn beide Quellen fünf Familien besitzen. Gleiche Lernpaare bleiben vom eigenen Planungseinfluss ausgeschlossen.

Ein Speicher kann alte und neue Quellen enthalten. Beim Hinzufügen der ersten Indexquelle steigt die Containerversion auf V4; bestehende Ereignistexte, Ketten und Record-IDs bleiben erhalten. Widerruf deaktiviert den Einfluss eines Records, ohne dessen Nachweis oder frühere Snapshots umzuschreiben. Die bisherige globale Deduplizierung gleicher Lernpaare bleibt bestehen, auch über Planerarten hinweg. Grenzen bleiben 8 Records, 16 Ereignisse, 1 MiB und maximal 1 Million deklarierte Suchoperationen je Quelle. Prüfaufwand des Speichers wird separat als `experience_audit_operations` ausgewiesen.

## Abnahme

Die CLI-Anforderung wurde vor Implementierung mit `UnknownOption: --expression-planner` rot geprüft. Abnahmefälle: gleiche vollständige Befunde beider Planer; geringere logische Beispielarbeit und exakte Budgetgrenze; alle drei Reihenfolgen; gemischte Speicher, Kontexttrennung, Selbstausschluss und Widerruf; Replay nach Entfernen externer Quellen; Z-Projektion mit gesperrter Polstelle; historische Fixtures; sämtliche kleinen Operations-, Kandidaten- und Checkpoint-Abbrüche; Callback-Abbruch, Metadatenmanipulation und unabhängiges Review. [Abnahmebericht](acceptance/indexed-portfolio-v1-2026-09-22.md).

Weitere Optimierung, allgemeinere Operatoren, vollständigere Definitionsbereichsvergleiche und weitergehende Experience-Verträge bleiben offen. Dies ist kein vollständiger M2-/M4-Abschluss.
