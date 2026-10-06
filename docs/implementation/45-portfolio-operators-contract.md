# Operatorauswahl im Fünferportfolio – Core 0.21

Freigegebene Fortsetzung von Core 0.20: Die explizite Operatorauswahl der Ausdruckssuche ist auch im Fünferportfolio verfügbar, einschließlich fester, adaptiver und erfahrungsbasierter Reihenfolge. Die Auswahl ist Teil des Suchauftrags und des Erfahrungskontexts.

## Geltungsbereich und Ausführung

`research --strategy portfolio-expression --operators '+*'` beschränkt ausschließlich die Ausdrucksfamilie. Die übrigen vier Familien behalten ihre eigenen mathematischen Definitionen und Parametergrenzen. Insbesondere darf eine Polynomstrategie weiterhin eine Summe finden, wenn das Ausdruckskind nur multiplizieren darf. Ein negativer Ausdrucksbefund ist kein negativer Gesamtbefund, solange eine andere Familie passende Kandidaten liefert.

Beide Ausdrucksplaner (`grammar`, `indexed-grammar`) und alle drei Portfolio-Reihenfolgen (`portfolio`, `adaptive-order`, `experience-order`) sind kombinierbar. Die bisherigen Regeln zur Auswahl bleiben erhalten: nichtleere Teilmenge von `+-*/`, keine Duplikate, kanonische Zeichenreihenfolge. Vollständige Auswahl entspricht unverändert dem bisherigen Standard.

Die direkte API verwendet `ExactResearchTask::expression_operators`. Beim Start eines Kindes wird dieser Wert nur für `expression` übernommen und für alle anderen Familien auf den Standard zurückgesetzt. Die CLI erlaubt `--operators` für `expression` und `portfolio-expression`; andere Strategien weisen die Option zurück.

Alle Familien teilen weiterhin Operations-, Kandidaten-, Ergebnis- und Tracegrenzen sowie globale Abbruchpositionen. Unvollständige Kinder oder Normalformzusammenführungen halten den Gesamtstatus auf INCOMPLETE. Die Originalbäume, Gegenprüfung, Hold-out und Ausschlussbedingungen werden unverändert verarbeitet. IDs enthalten weiterhin die familienbezogene Identität, sind wegen lokaler Ausdrucksränge aber nur innerhalb der jeweiligen Grammatik vergleichbar.

## Ergebnis und Replay

Teilgrammatiken verwenden neue Portfolio-Versionen, unabhängig davon, welcher der beiden Ausdrucksplaner gewählt ist:

| Reihenfolge | Container | Engine | Reihenfolgeplaner |
| --- | --- | --- | --- |
| Fest | BRUCE_PORTFOLIO_BUNDLE_V14 | bruce-portfolio-v14 | five-family-selected-v1 |
| Adaptiv | BRUCE_PORTFOLIO_BUNDLE_V15 | bruce-portfolio-v15 | sample-shape-v5 |
| Erfahrung | BRUCE_PORTFOLIO_BUNDLE_V16 | bruce-portfolio-v16 | experience-order-v5 |

Alle drei behalten `normal-form-union-v1`, `shared-budget-v2` und `rational-domain-v1`. Sie speichern zusätzlich explizit `expression_operators` und `expression_planner` (`size-ranked-v1` oder `block-index-v1`). JSON und Trace machen die Einschränkung sichtbar; `claim_scope` erklärt, dass die anderen vier Familien davon unberührt bleiben.

Replay verlangt eine kanonische echte Teilmenge und eine bekannte Planerversion. Vollständige Operatorauswahl gehört zu den bisherigen V8–V13-Pfaden. Der gesamte Lauf einschließlich Kinder, Herkunft, Kosten, Abbruchpositionen und Trace wird erneut erzeugt und kanonisch verglichen. V16 enthält den damaligen Erfahrungssnapshot mit seinen Quellnachweisen.

Z-Projektion akzeptiert V14–V16 und wertet die vorhandenen Normalformen einschließlich Ausschlusspolynomen aus. Die Herkunftskennung enthält Portfolio-Version, Operator-Bitmaske, Ausdrucksplaner und Zahlendomäne. Die Maske verwendet `+`=1, `-`=2, `*`=4, `/`=8. Dadurch bleiben verschiedene Grammatikräume unterscheidbar. Int64-Projektionsgrenzen, Polstellensperre und Sperre unvollständiger Gesamtbefunde bleiben erhalten.

## Routing-Erfahrung V5

`BRUCE_EXPERIENCE_V5`, Engine `bruce-experience-v5`, Policy `routing-experience-v5` erweitern den Speicher um unabhängige Quellen aus V14/V15. Beide Ausdrucksplaner sind zulässig. V16 bleibt als Erfahrungsquelle gesperrt. Vollständigkeit, Hold-out und mindestens eine bestätigte Hypothese werden durch tatsächlichen Replay geprüft.

Der neue Kontext `routing-context-v5-five-selected` enthält die kanonischen Operatoren, die Ausdrucksplanerversion und die bisherigen Domänen-/Suchgrenzen, Gleichheitsversion, Normalformregel und den Formhinweis. Stimmen einer anderen Auswahl oder eines anderen Ausdrucksplaners bleiben ohne Einfluss. Alte vollständige Grammatiken behalten ihre historischen Kontextschlüssel.

Gemischte Speicher dürfen alte und neue Quellen enthalten. Beim ersten neuen Quelltyp steigt die Speicherversion auf V5. Vorhandene Ereignisse, Kette und Record-IDs bleiben erhalten. Widerruf entfernt Planungseinfluss, ohne historische Snapshots umzuschreiben. Identische Lernpaare werden weiterhin global dedupliziert und vom eigenen Planungseinfluss ausgeschlossen, auch über verschiedene Grammatiken hinweg.

Grenzen bleiben 8 Records, 16 Ereignisse, 1 MiB und maximal eine Million deklarierte Suchoperationen je Quelle. Der Quellenprüfaufwand wird weiterhin separat von der neuen Suche ausgegeben. Stimmen sind keine zusätzlichen mathematischen Beweise und verändern keine Wahrheits- oder Lerngates.

## Abnahme

Die neue Portfolio-Anforderung wurde zuerst rot mit `OperatorsRequireExpressionStrategy` nachgewiesen. Geprüft werden Kind-Isolation, sechs Planerkombinationen, stabile Befunde innerhalb derselben Grammatik, kleinerer Beispielraum, gemeinsame Grenzen, vollständige kleine Abbruchreihen, gemischte Erfahrungskontexte, Selbstausschluss, Widerruf, Quellenverbot, Polstellen, Manipulation und Replay ohne Ursprungsdateien. [Abnahmebericht](acceptance/portfolio-operators-v1-2026-09-22.md).

Neue Operatorarten, freiere Grammatiken, weitere Suchoptimierung und umfassendere Experience-Verträge bleiben offen. Dies ist kein vollständiger M2-/M4-Abschluss.
