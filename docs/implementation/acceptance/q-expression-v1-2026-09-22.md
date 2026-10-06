# Abnahme: Rationale Definitionsbereiche in der Ausdruckssuche – 2026-09-22

Stand: Core 0.27. Implementiert und aktuell verifiziert.

[Vertrag](../57-q-expression-contract.md) · [Benutzung](../58-q-expression-usage.md)

## Gelieferter Ablauf

Neue Lern-/Hold-out-Paare → bestehende begrenzte Ausdrucksgrammatik → Haupt- und Gegenrechnung → Normalisierung → Vergleich der Wertefunktion und des rationalen Definitionsbereichs → erste nachweislich passende Gruppe mit vollständig erhaltenen Ursprüngen → V7-Export, Replay und Import in Z-Auswahl.

`rational-q` ist optional und nur für die eigenständige Ausdruckssuche freigeschaltet. Historische Modi, Portfolio- und Experience-Regeln bleiben unverändert.

## Verifikation

- Debug-Build unter C++20 mit `-Wall -Wextra -Werror`: erfolgreich.
- Vollständige Debug-Suite: **52/52 bestanden**.
- Vollständige UBSan-Suite mit `-fsanitize=undefined -fno-sanitize-recover=all`: **52/52 bestanden**, keine Sanitizer-Meldung.
- Nach Ergänzung weiterer Fälle wurden beide neuen Testgruppen nochmals in Debug und UBSan geprüft. Die CLI-Datei enthält im finalen Stand sechs Testmethoden.
- Gesamtdemo einschließlich aller bisherigen Forschungs-/Experience-/Auswahlabläufe und neuer Q-Vergleiche: erfolgreich.
- Alle **24 historischen `.bruce`-Fixtures** in neuen Prozessen wiedergegeben und mit dem gespeicherten vollständigen JSON verglichen: unverändert.
- Unabhängiges Review: keine handlungsbedürftigen Befunde. Zusätzlich 24 Such-/Replayläufe mit rationalen Eingaben für x², 1/x, x+1/2 und null über alle drei Planer; zwölf Vergleiche mit konservativer Gruppierung bewahrten sämtliche Ursprungskennungen, Formeln und Statuswerte.

## Gezielte Prüfungen

1. Alle 31 nichtleeren Operatorauswahlen mit allen drei Planern; erwartete Befunde und vollständige Suche im kleinen Referenzraum.
2. Vollständiger Forschungsfall `1/(1+x²)` mit identischen Hypothesen über alle Planer, aktivem begrenztem Cache und V7-Replay.
3. Vollständiger Potenzfall x²: Multiplikations- und Potenzbäume in derselben Gruppe, exakte Werte-/Bereichsprüfung und erhaltene Quellen über alle Planer.
4. `1` und `x/x` bleiben verschiedene Gruppen. Auswahl bei gültiger Projektionsstelle und eingebettetes Auswahl-Replay funktionieren; die Polstelle 0 bleibt gesperrt. Die neue Herkunft ist unterscheidbar.
5. Gegenüber dem bisherigen Modus zusätzliche Zusammenführung bei nullstellenfreiem Restfaktor x²−2, bei identischen Kandidatenzahlen und Originaltreffern. Ein unbekannter Restfaktor x³−3 bleibt getrennt.
6. Für alle drei Planer: sämtliche kleineren positiven Operationsbudgets eines kleinen Gruppenfalls sowie alle Abbruch-Checkpoints, jeweils vollständiger Replayvergleich. Exakt ausreichendes Budget und Callback-Abbruch geprüft.
7. Auch bei unterbrochenem Gruppenvergleich entspricht die Zahl gespeicherter Ursprünge dem Quellenzähler; kein doppelter oder verlorener Ursprung. Vergleichskosten sind in den gesamten Operationskosten enthalten.
8. Ergebnis-, Spur- und Spurbytegrenzen bleiben wirksam. Ergebnisbudget zählt Originaltreffer, nicht Gruppen.
9. Neu berechnete Hashes verschleiern keine Änderung von Gleichheitspolicy, Cachepolicy, Bereichspolicy, Operatorreihenfolge oder Ergebniszählern: Replay weist sie zurück.
10. Portfolio weist `rational-q` weiterhin zurück. Neue Suchversionen ändern keine historischen Formate.

## Referenzmessungen der Demo

| Fall | Befund |
| --- | --- |
| Vollständige Suche für 1/(1+x²) | `VERIFIED_ON_SAMPLES`; 92460 logische Einheiten; nur ein Treffer, daher keine zusätzliche Paarvergleichsarbeit |
| Nullfunktion, Konstante 2, Operatoren -^/ | Konservativ 3 Gruppen, Q-Modus 2; 6844 zusätzliche Einheiten für Gruppendurchlauf und Bereichsprüfung; keine offenen Vergleiche |
| Nullfunktion, Konstante 3, Operatoren -^/ | Beide Modi 6 Gruppen; Q-Modus 15128 Vergleichseinheiten, 4 offene Bereichsvergleiche |

Die beiden Nullfunktionsfälle enden **in beiden Modi mit `INCOMPLETE` / `ResultBudgetExceeded`** bei 128 gespeicherten Originaltreffern. Die Ursprünge und Statuswerte sind identisch. Diese Messung belegt eine präzisere Gruppierung im Teilbefund, weder einen vollständigen Suchraumabschluss noch eine Freigabe für Auswahl. Alle Exporte einschließlich dieser Abbrüche sind exakt reproduzierbar.

Logische Vergleichsarbeit ist zusätzliche Arbeit und kein CPU-Beschleunigungsnachweis. Eine algebraisch bestätigte Formelgleichheit macht die Datenhypothese nicht zu einem allgemeinen Gesetz.

## Verbleibende Arbeit

Portfolio-/Experience-Einbindung, allgemeinere Nullstellenentscheidungen, effizientere Gruppenvergleiche und weitere Roadmap-Pakete bleiben offen. Die konservative Behandlung höherer Restgrade kann mathematisch gleiche Funktionen weiterhin getrennt lassen. Kein vollständiger M2-/Gesamtprojektabschluss.
