# Vier Familien mit Erfahrungsplanung – Core 0.13

Freigegebener Anschluss an Core 0.12: Die vorhandene quadratische Bruchfamilie wird als vierte Strategie in den gemeinsamen Forschungs-, Planungs-, Experience- und Replaypfad integriert. Die neue Auswahl heißt `portfolio-quadratic`; `portfolio-rational` behält seinen bisherigen Dreifamilienumfang und seine alten Versionen.

## Familien und Identität

Feste Reihenfolge: `power-affine`, `polynomial`, `rational-linear`, `rational-quadratic`. Alle vier Familien teilen sich Kandidaten-, Operations-, Ergebnis-, Trace- und Abbruchbudgets. Die bisherigen Domänen-/Grad-/Potenzgrenzen gelten; zusätzlich begrenzt die quadratische Familie das Produkt aus Zählerbreite und Nennergrenze auf 32768 Slots.

Polynome werden weiterhin anhand gekürzter Koeffizientenlisten zusammengeführt, mit erhaltenen Herkünften. Rationale Kandidaten behalten die normalisierte Zählerdarstellung und den vollständigen monischen Nenner. Unterschiedliche Nenner und unterschiedliche Bruchfamilien bleiben getrennt. Es gibt keine allgemeine Prüfung algebraischer Funktionsgleichheit oder Gleichheit ihrer Nullstellenmengen. Auch mathematisch gleichwertige Darstellungen können deshalb mehrdeutig bleiben; kein Nennerfaktor wird stillschweigend gekürzt.

Die neue Gesamt-ID ist `4 * Quell-ID + Familienindex`. Bei zusammengeführten Polynomen hat die Potenzherkunft Vorrang, unabhängig von der Ausführungsreihenfolge. Mit `Quell-ID < 2^60` gilt `Gesamt-ID < 2^62`. Der Koordinatensplit bei `2^31` hält beide Koordinaten unter `2^31` und die Cantor-Adresse unter `2^63`. Historische Versionen behalten ihre eigenen Splits.

## Datenabhängige Planung

`adaptive-order` verwendet ausschließlich das kurze Lernpräfix, nie Hold-out-Werte. Die neue Heuristik `sample-shape-v2` erweitert die bisherige Reihenfolge um einen Hinweis auf quadratische Kehrwerte:

1. Gleiche Steigungen der ersten drei Lernpaare: Potenz zuerst.
2. Gleiche Steigungen der Kehrwerte: lineare Bruchfunktion zuerst.
3. Gleiche zweite dividierte Differenzen der ersten vier Kehrwerte: quadratische Bruchfunktion zuerst.
4. Gleiche zweite dividierte Differenzen der direkten Werte: Polynom zuerst.
5. Sonst feste Grundreihenfolge.

Die beiden Kehrwerthinweise verlangen die jeweils benötigten y-Werte ungleich null. Bei einem Gleichstand zwischen quadratischem Kehrwert- und direktem Polynomhinweis hat der Kehrwerthinweis Vorrang. Solche Gleichstände treten bereits bei `1/(x²+1)` auf x=0,1,2,3 auf. Ein Hinweis ist keine Garantie für die passende Familie. Alle vier Familien bleiben im Plan; Kosten und Abbruchposition sind budgetiert.

## Erfahrung und historische Zustände

`experience-order` gewichtet diesen Viererplan mit passenden früheren Erfahrungen. Stimmenreihenfolge: Potenz, Polynom, linearer Nenner, quadratischer Nenner. Neue Quellen müssen vollständige V5-/V6-Läufe mit Hold-out und mindestens einer positiv geprüften Hypothese sein. Alte V2-/V3-Quellen bleiben zulässig. V4-/V7-Läufe mit eingebetteter Erfahrung bleiben als neue Quellen ausgeschlossen, bevor deren Replay ausgeführt wird.

Ein Snapshot mit mindestens einer Viererquelle erhält `BRUCE_EXPERIENCE_V2` und Policy `routing-experience-v2`. Vorhandene Ereignisse und ihre IDs bleiben beim Anhängen unverändert. V1-Snapshots und ihr historischer Befund bleiben unterstützt. Gemischte Speicher enthalten drei- und vierstellige Stimmen; ein eigener Kontextfingerprint für das Viererportfolio verhindert Übertragung zwischen unterschiedlichen Familienmengen. Gleiche Lernpaare zählen weiterhin nicht erneut, auch nicht durch Wechsel der Portfolioversion.

Widerruf ergänzt ein Ereignis und entfernt den künftigen Planungseinfluss bei Verwendung des neuen Snapshots. Er ändert keinen früheren Export. Weiterhin maximal 1 MiB, acht Quellen, 16 Ereignisse und höchstens 1000000 Operationsbudget pro Quelle; keine Kompaktierung, keine automatische Aufnahme oder Regelpromotion. Nachweisarbeit bleibt separat von der eigentlichen Suche sichtbar.

## Replay-Versionen

| Modus | Container | Engine | Planer |
| --- | --- | --- | --- |
| feste Reihenfolge | `BRUCE_PORTFOLIO_BUNDLE_V5` | `bruce-portfolio-v5` | `four-family-v1` |
| datenabhängig | `BRUCE_PORTFOLIO_BUNDLE_V6` | `bruce-portfolio-v6` | `sample-shape-v2` |
| Erfahrung | `BRUCE_PORTFOLIO_BUNDLE_V7` | `bruce-portfolio-v7` | `experience-order-v2` |

Alle drei verwenden `domain-union-v2` und `shared-budget-v1`. V7 enthält seinen konkreten Experience-Snapshot. Z-Projektion wertet je Hypothese den tatsächlichen Nenner aus und hält Pole sowie nichtganzzahlige/zu große Werte als ausdrückliche Fehler fest.

Abnahme: CLI-Durchstich zunächst rot, dann vier Familien und Reihenfolgen vergleichen; globale Checkpoints und Operationsgrenzen für alle Planer prüfen; gemischte Stores/Widerruf/Selbstreferenz/Manipulation prüfen; Maximal-ID bis zur Auswahl replayen; alte Fixtures erhalten; unabhängiges Review, Debug/UBSan, Beispiele und Demo. Kein allgemeiner Beweis und kein vollständiger M2-/M4-Abschluss.
