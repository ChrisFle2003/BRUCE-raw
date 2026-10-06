# Abnahme: Bereichscache im Portfolio – 2026-09-22

Stand: Core 0.32. Implementiert und aktuell verifiziert.

[Vertrag](../67-domain-cache-portfolio-contract.md) · [Benutzung](../68-domain-cache-portfolio-usage.md)

## Gelieferter Ablauf

Fünferportfolio → Ausdruckssuche mit eigenem Bereichscache → Familienvereinigung mit getrenntem Bereichscache → V29/V30/V31-Export und Replay → unabhängige Erfahrungsquelle im V10-Speicher → kontextgebundene Reihenfolge und Widerruf → Z-Auswahl und Replay ohne ursprüngliche Quelldateien.

Beide Caches sind optional, lauflokal und auf jeweils 64 Einträge begrenzt. Nur vollständig bewiesene Bereichsgleichheiten werden gespeichert. Alle Originaltreffer und Definitionslücken bleiben erhalten. Schlüsselaufbau, Vergleich und Verwaltung belasten das gemeinsame Arbeitsbudget. Der Standard ohne Bereichscache bleibt unverändert.

## Verifikation

| Prüfung | Ergebnis |
| --- | --- |
| Debug-Build | C++20 mit `-Wall -Wextra -Werror`; erfolgreich |
| Vollständige Debug-Suite | 61/61 bestanden |
| Vollständige UBSan-Suite | 61/61 bestanden; `-fsanitize=undefined -fno-sanitize-recover=all`, keine Sanitizer-Meldung |
| Gesamtdemo | Sämtliche bisherigen Abläufe und neuer Portfolio-/Erfahrungs-/Widerrufs-/Auswahl-/Replaypfad erfolgreich |
| Historische Exporte | Alle 24 vorhandenen `.bruce`-Fixtures unverändert replaybar |
| Vergleich mit Core 0.31 | 54 bisherige Portfoliofälle über drei Ausdrucksplaner, zwei Reihenfolgen, drei Gleichheitsmodi und drei Arbeitsbudgets byteidentisch |
| Unabhängiges Review | Keine handlungsbedürftigen Befunde; zusätzlich 120 zufällige Arbeitsbudgets auf Abrechnung, Budgetgrenze und Replay geprüft |

## Gemessene Referenzläufe

Beide Aufgaben verwenden `portfolio-expression`, `cached-grammar` und `rational-q-modular` mit den Nullfunktionsbeispielen. Die Gesamtdemo enthält die ausführbaren Aufrufe.

| Merkmal | Kleiner vollständiger Lauf | Kubischer Teilbefund |
| --- | --- | --- |
| Operatoren / Konstante / maximale Knoten | `+/` / 0 bis 1 / 3 | `-^/` / 3 / 9 |
| Arbeit ohne Bereichscache | 3564 | 463660 |
| Arbeit mit `equal-64` | 3620 | 462532 |
| Unterschied | 56 Einheiten mehr | 1128 Einheiten weniger |
| Gruppen / Originaltreffer, jeweils unverändert | 1 / 7 | 5 / 128 |
| Status | VERIFIED_ON_SAMPLES | INCOMPLETE / ChildIncomplete |
| Cachetreffer gesamt / davon Vereinigung | 2 / 1 | 117 / 0 |
| Cacheverwaltung, bereits in Gesamtarbeit enthalten | 126 | 4199 |
| Summe abschließender Einträge / Schlüsselbytes | 4 / 40 | 6 / 85 |

Der kubische Lauf erreicht unverändert die Originalgrenze des Ausdruckskinds. Er ist keine vollständige Suche. Die Speicherzähler summieren zwei getrennte abschließende Phasenstände; sie messen keinen gleichzeitig belegten RAM. Die kleine Aufgabe zeigt ausdrücklich, dass Cacheverwaltung mehr Arbeit verursachen kann. Keine allgemeine Geschwindigkeitsbehauptung.

## Grenz- und Integrationsprüfung

- Alle drei Ausdrucksplaner sowie feste, adaptive und erfahrungsbasierte Reihenfolge: konsistente gemeinsame Abrechnung und identisches Replay vollständiger sowie abgebrochener Läufe.
- Jede kleinere positive Arbeitsgrenze des kleinen Referenzfalls, die exakte Gesamtgrenze, Checkpointabbrüche sowie Kandidaten-, Ergebnis-, Spurereignis- und Spurbytegrenzen.
- Abbruch an jedem Arbeitsschritt eines echten Cachetreffers in der Familienvereinigung: keine vorzeitige Zusammenführung; der neue Ursprung bleibt als unvollständiger eigener Befund erhalten.
- Alle 31 nichtleeren Operatorkombinationen über drei Ausdrucksplaner mit fester und adaptiver Reihenfolge; unveränderte vollständige Gruppen und Replay.
- Getrennte Cachezustände und korrekt summierte Zähler; die ersten vier Familien bleiben vom Ausdruckscache unabhängig. Bestehende Kollisions-, Verdrängungs- und Schlüsselgrößentests laufen weiterhin mit.
- V10-Erfahrung mit vollständiger unabhängiger Quelle: `[0,0,0,0,1]` Stimmen, Ausdruckssuche zuerst, gleiche vollständige Befunde wie die adaptive Referenz. Selbsteinfluss, doppelte Lernquellen und erfahrungsbasierte Quellen werden ausgeschlossen.
- Alte und neue Erfahrungskontexte sowie unterschiedliche Ausdrucksplaner bleiben getrennt. Gemischte V9-/V10-Quellen behalten vorhandene Datensatz-IDs; ein Widerruf entfernt den späteren Einfluss.
- Manipulierte Cachepolitik, Geltungsbereich, Zähler, Gleichheitsmodus und Erfahrungsversion werden auch nach neu berechnetem Hash abgelehnt.
- Vollständige Auswahl bei 0 liefert 0; die Polstelle −1 bleibt gesperrt. Forschungs- und Auswahl-Replay funktionieren nach Entfernen ursprünglicher Quellen, Speicher und Regeldateien.

## Offene Arbeit

Weitere Suchoptimierungen, vollständigere Nullstellenverfahren, externe Evidenz und weitergehende Experience-Korrekturketten bleiben offen. Sampleprüfungen beweisen keine allgemeine Datenbeziehung. Kein vollständiger M2-/M4-/Gesamtprojektabschluss.
