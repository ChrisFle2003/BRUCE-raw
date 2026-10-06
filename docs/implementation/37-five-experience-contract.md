# Fünferplanung und Routing-Erfahrung – Core 0.17

Freigegebene Fortsetzung des Fünferportfolios: Die bestehende Strategie `portfolio-expression` erhält die Planer `adaptive-order` und `experience-order`. Alle fünf Familien bleiben Teil des begrenzten Suchraums. Erfahrung darf die Reihenfolge verändern, aber keine neue Aufgabe als gelöst übernehmen und keine Wahrheits- oder Freigabegates setzen.

## Formheuristik und stabile Identität

`sample-shape-v3` übernimmt die bestehende Viererheuristik auf den ersten höchstens vier Lernpaaren und ergänzt die Ausdruckssuche als fünfte Familie am Ende. Affine, reziprok-affine, reziprok-quadratische und direkte quadratische Präfixe können eine der bisherigen Familien vorziehen. Die bisherige Priorität des reziprok-quadratischen Hinweises bleibt erhalten. Ohne Hinweis gilt die feste Grundreihenfolge. Die Ausdruckssuche kann anschließend über passende Erfahrung nach vorne rücken.

Die Heuristik sieht keine Hold-out-Werte. Ein kurzer Präfix kann irreführen: Beispielsweise wirken die Kehrwerte von x²+1 an x=0,1,2,3 selbst quadratisch. Das ist ausschließlich ein Reihenfolgehinweis; alle Familien und ihre bisherigen unabhängigen Sampleprüfungen bleiben nötig.

Die globale Gruppen-ID folgt weiterhin der festen Familienpriorität und danach der Quell-ID. Eine später ausgeführte Familie mit früherer kanonischer Priorität ersetzt gegebenenfalls die zunächst gewählte ID. Beim Abschluss werden Gruppen und Ursprünge in kanonische Reihenfolge gebracht. Damit stimmen bei vollständigen Läufen Normalformen, Status, IDs und Herkunft zwischen fester, adaptiver und erfahrungsbasierter Planung überein. Teilergebnisse können sich abhängig vom Budget und der Reihenfolge unterscheiden und bleiben INCOMPLETE.

## Erfahrung und Kontext

Der Erfahrungsspeicher V3 nimmt zusätzlich unabhängige Fünferquellen aus V8 und V9 auf. V10-Quellen sind ausgeschlossen, damit aus Erfahrung geplante Läufe keine rekursiven Erfahrungsnachweise bilden. Der vorhandene Vertrag für begrenzte Dateigröße, höchstens acht Quellen und sechzehn Ereignisse, begrenzte Quellprüfung, doppelte Lerninputs und Widerruf bleibt erhalten.

Jede Quelle wird replay-geprüft. Erforderlich sind ein vollständiger Lauf, Hold-out und mindestens eine dort positiv geprüfte Hypothese. Je Familie zählt höchstens eine Stimme, unabhängig von der Zahl ihrer Formeln, Gruppen oder Baumherkünfte. Die fünf Positionen lauten Potenz, Polynom, linearer Bruch, quadratischer Bruch und Ausdruck.

Ein Eintrag beeinflusst eine Zielaufgabe nur, wenn er aktiv ist, andere Lernpaare besitzt und sein Kontext übereinstimmt. Für die neue Familienmenge umfasst dieser Kontext:

- eigene Versionskennung für fünf Familien;
- Zahlenbereich, Zählergrenzen, Nennergrenze und Bitlimit;
- maximale Potenz, Polynomgrad und Baumgröße;
- rationale Gleichheitsregel und Version der familienübergreifenden Normalisierung;
- Formklasse des Lernpräfixes vor der Erfahrungsgewichtung.

Lernpaare werden für den Selbstabgleich unabhängig von ihrer Zeilenreihenfolge kanonisiert. Gleiche Lerninputs werden nicht als neue unabhängige Quelle gezählt. Hold-out-Werte bestimmen die Quellenzulässigkeit, aber nicht das Ranking einer neuen Zielaufgabe. Quellen mit abweichender Familienmenge oder Suchkonfiguration bleiben im Speicher sichtbar, liefern für diese Aufgabe jedoch keine Stimme.

Eine stabile Sortierung nach Stimmen erhält bei Gleichstand die heuristische Reihenfolge. Ein Widerruf deaktiviert den Planungseinfluss der Quelle; ihr historischer Eintrag bleibt erhalten. Alte Drei-/Viererquellen dürfen mit Fünferquellen in einem V3-Snapshot liegen. Ihre Ereignistexte und IDs werden unverändert übernommen, und sie beeinflussen weiterhin nur ihren passenden Kontext.

## Kosten, Grenzen und Replay

Die Planung zählt zum gemeinsamen Operationsbudget und besitzt einen globalen Checkpoint. Quellenprüfung wird weiterhin separat unter `experience_audit_operations` ausgewiesen; sie findet vor der Zielsuche statt und gehört nicht zu deren `--max-operations`. Auch kleine Zielbudgets sparen daher nicht automatisch die Nachweisprüfung. Sämtliche bestehenden Snapshot-/Quellgrenzen bleiben wirksam.

Für geordnete Fünferläufe gilt:

`operations_used = Summe der Teilrun-Arbeit + union_operations + ordering_operations`.

Normalisierung, Herkunftsanzahl, Abbruch und Trace bleiben wie im Fünfervertrag begrenzt. Der geordnete Wrapper reserviert konservativ 1280 Tracebytes. Unterbrochene Heuristik, Stimmensammlung oder Sortierung geben keine fertige Reihenfolge aus. Ein früher positiver Treffer macht offene Familien nicht vollständig.

Neue Versionen: adaptiver Forschungscontainer V9 mit Engine `bruce-portfolio-v9` und Planer `sample-shape-v3`; erfahrungsbasierter Forschungscontainer V10 mit Engine `bruce-portfolio-v10` und Planer `experience-order-v3`. Normalformregel `normal-form-union-v1` und Policy `shared-budget-v2` bleiben bestehen. Erfahrungssnapshot `BRUCE_EXPERIENCE_V3` mit Policy `routing-experience-v3`.

V10 enthält den gesamten damals verwendeten Snapshot samt Quellen und Widerrufen. Replay benötigt keine früheren CSV-, Speicher- oder Forschungsdateien und reproduziert auch nach späteren Widerrufen den damaligen Lauf. Die Z-Auswahl verarbeitet V9/V10 mit den erhaltenen Ausschlussbedingungen. Historische V8-Läufe und V1/V2-Erfahrungssnapshots bleiben unverändert replaybar.

## Abnahme und Grenze

Neue Anforderungen zuerst rot; Formhinweise und tatsächlich vorgezogene Ausdruckssuche; vollständige Befunde gegen festen Lauf; Kontext-/Selbstausschluss, getrennte Hold-out-Nutzung, gemischte Speicher und Widerruf; rekursive Quelle zurückweisen; alte Fixture und manipulierter Replay; alle kleinen Operations-, Checkpoint- und Kandidatenabbrüche einschließlich Quellenübernahme; Debug-/UBSan-Suiten, unabhängiges Review und Gesamtdemo. [Abnahmebericht](acceptance/five-experience-v1-2026-09-22.md).

Die Lieferung ist begrenztes erfahrungsbasiertes Routing. Sie ersetzt weder allgemeines Lernen noch automatische Regelpromotion, externe Evidenz oder umfassende Experience-Korrekturketten. Keine allgemeine Formel aus Samples bewiesen und kein vollständiger M2-/M4-Abschluss.
