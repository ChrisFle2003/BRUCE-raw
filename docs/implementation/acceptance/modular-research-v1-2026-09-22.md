# Abnahme: Modulare Q-Gruppen in Forschung und Erfahrung – 2026-09-22

Stand: Core 0.30. Implementiert und aktuell verifiziert.

[Vertrag](../63-modular-research-contract.md) · [Benutzung](../64-modular-research-usage.md)

## Gelieferter Ablauf

Datenpaare → eigenständige Ausdruckssuche oder Fünferportfolio → neue Q-Gruppierung mit modularen Nullstellenausschlüssen → Originalerhalt, begrenzte Teilbefunde und sichtbare Zähler → versionierter Export/Replay → Z-Auswahl. Vollständige unabhängige Portfolioquellen können einen getrennten V9-Erfahrungskontext beliefern; Widerruf und eingebetteter historischer Speicherstand bleiben erhalten.

## Verifikation

| Prüfung | Nachweis |
| --- | --- |
| Debug-Build | C++20 und `-Wall -Wextra -Werror`; erfolgreich |
| Vollständige Debug-Suite | 57/57 bestanden |
| Vollständige UBSan-Suite | 57/57 bestanden; `-fsanitize=undefined -fno-sanitize-recover=all`, keine Sanitizer-Meldung |
| Gesamtdemo | Sämtliche bisherigen Abläufe und neue kubische Gruppierung, V9-Erfahrung, Widerruf, V28-Replay und Auswahl-Replay erfolgreich |
| Historische Exporte | Alle 24 vorhandenen `.bruce`-Fixtures liefern unverändertes vollständiges Replay-JSON |
| Historisches Standardverhalten | 90 zusätzliche Vergleiche gegen Core 0.29, alle drei Ausdrucksplaner, bisherige Gleichheitsmodi, fixe/adaptive Portfolios sowie knappe Arbeits-/Spurgrenzen: Ausgabe, Fehlerausgabe und Rückgabecode byteidentisch |
| Unabhängiges Code-Review | Keine handlungsbedürftigen Befunde; neue zwei Expression- und sechs Portfolio-Testmethoden zusätzlich unabhängig ausgeführt |

## Konkrete Befunde

Im kubischen Nullfunktionsbeispiel mit Konstante 3, Operatoren `-^/` und neun Baumknoten:

| Lauf mit Cache | Bisher `rational-q` | Neu `rational-q-modular` |
| --- | --- | --- |
| Ausdrucksgruppen | 6 | 5 |
| Portfolio-Gruppen | 6 | 5 |
| Gespeicherte Originaltreffer | 128 | 128 |
| Ausdrucksarbeit | 465169 | 461889 |
| Portfolioarbeit | 467870 | 463660 |
| Erfolgreiche modulare Seitenzertifikate | nicht verwendet | 4 |

Die Verringerung betrifft genau dieses begrenzte Beispiel. Zusätzliche Beweisarbeit kann durch spätere Vergleiche mit weniger getrennten Gruppen ausgeglichen werden; daraus folgt kein allgemeiner Geschwindigkeitsgewinn.

Beide Suchmodi erreichen weiterhin die Originalgrenze: Ausdruck `INCOMPLETE / ResultBudgetExceeded`, Portfolio `INCOMPLETE / ChildIncomplete`. Alle Originalausdrücke mit IDs und Status bleiben identisch erhalten. Ein unter diesen Originalen vorhandener Nullausdruck mit Ausschluss x³−3 wird im neuen Modus dem Vertreter ohne diese formale Ausschlussbedingung zugeordnet.

Der gesonderte vollständige Erfahrungslauf liefert `[0,0,0,0,1]` Stimmen und startet die Ausdruckssuche zuerst. Die vollständigen Gruppen entsprechen dem adaptiven Referenzlauf. Nach Widerruf entfällt der Einfluss. Replay bleibt auch nach Entfernen der Quelldateien möglich; Auswahl liefert bei 0 den Wert 0 und sperrt die Polstelle −1.

## Grenz- und Gegenprüfungen

- Tatsächliche kubische Zusammenführung und gleiche Originale in allen drei Ausdrucksplanern und beiden Suchformen; feste/adaptive Portfolio-Gruppen stimmen überein.
- Kleine vollständige Expression- und Portfolioaufgaben für beide Q-Politiken: jede kleinere positive Arbeitsgrenze und alle vorgesehenen Abbruch-Checkpoints, Portfolio-Kandidatenabbrüche, Quellen-/Spur-/Bytegrenzen mit Replay und gemeinsamer Abrechnung.
- Direkte Familienvereinigung einer Polynom-Nullfunktion mit einem kubischen Ausdruck: nachgewiesene Gleichheit für x³−2, keine falsche Vereinigung bei x³−1 oder beim offenen höhergradigen Produkt. Jede Operationsgrenze der Vereinigung erhält den unvollständigen Ursprung separat. Das prüft die Vereinigung unabhängig von der Ausdrucks-Enumeration.
- Alle 31 Operatorauswahlen, alle Ausdrucksplaner und feste/adaptive Portfolioplanung; literale Potenzen tatsächlich in den Originalen vorhanden.
- V9-Stimmen nur bei passendem Modus und Ausdrucksplaner; eigene Lernpaare ausgeschlossen, gemischter V8/V9-Speicher erhält alte Kennungen, doppelte Lernpaare abgelehnt, Widerruf wirksam.
- V28-Läufe als neue Erfahrungsquelle abgelehnt. Manipulierte Versionen, Politik, Cache, Gruppierungszähler und Inhalte auch mit neuem Dateihash abgelehnt.
- Auswahlprovenienz unterscheidet den neuen Modus; echte Polstellen und unvollständige Forschung bleiben gesperrt.

## Aussagegrenze

Die Ergebnisse bleiben begrenzte Forschung mit Sampleprüfung. Modulare Seitenzertifikate beweisen gegebenenfalls Nullstellenfreiheit eines konkreten Restpolynoms über Q; sie beweisen keine unbekannte Datenbeziehung. Fehlende Zertifikate bleiben offen. Die neue Integration ist kein vollständiger M2-/M4-/Gesamtprojektabschluss.
