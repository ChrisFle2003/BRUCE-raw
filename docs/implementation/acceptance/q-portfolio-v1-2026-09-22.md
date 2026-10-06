# Abnahme: Q-Gruppen im Portfolio und Erfahrungskontext – 2026-09-22

Stand: Core 0.28. Implementiert und aktuell verifiziert.

[Vertrag](../59-q-portfolio-contract.md) · [Benutzung](../60-q-portfolio-usage.md)

## Gelieferter Ablauf

Lern-/Hold-out-Paare → fünf begrenzte Formelfamilien in fester, adaptiver oder erfahrungsbasierter Suchreihenfolge → feste nachgelagerte Familienvereinigung mit rationalem Bereichsnachweis → erhaltene Originaltreffer und Gruppen → V23/V24/V25-Export, Replay und Z-Auswahl. Unabhängige V23/V24-Quellen können in V8-Erfahrungsspeichern die Reihenfolge späterer passender Aufgaben beeinflussen.

## Verifikation

| Prüfung | Nachweis |
| --- | --- |
| Debug-Build | C++20, bestehendes `-Wall -Wextra -Werror`; erfolgreich |
| Vollständige Debug-Suite | 54/54 bestanden |
| Vollständige UBSan-Suite | 54/54 bestanden; `-fsanitize=undefined -fno-sanitize-recover=all`, keine Sanitizer-Meldung |
| Ergänzte gezielte Fälle | Beide neuen Testgruppen erneut in Debug und UBSan; finale CLI-Datei mit sechs Testmethoden |
| Gesamtdemo | Alle bisherigen Abläufe und neuer Q-Portfolio-/Experience-/Auswahlablauf erfolgreich |
| Historische Exporte | Alle 24 vorhandenen `.bruce`-Fixtures liefern unverändertes vollständiges Replay-JSON |
| Unabhängiges Review | Keine handlungsbedürftigen Befunde; zusätzlich 140 randomisierte Budgetfälle mit Kosten-/Spurgrenzen und vollständigem Replay geprüft |

Die vollständigen Suites wurden vor den abschließenden Erweiterungen der neuen Testdatei ausgeführt; deren finaler Stand wurde anschließend gezielt unter beiden Builds geprüft. Produktionscode blieb dabei unverändert.

## Gezielte Abnahme

- Alle drei Ausdrucksplaner mit fester und adaptiver Reihenfolge: gleiche vollständige Gruppen, erste vier Teilverfahren unverändert, Ausdruckskind V7, striktes Portfolio-Replay.
- Alle 31 nichtleeren Operatorauswahlen über alle drei Ausdrucksplaner, jeweils feste und adaptive Planung; literale Potenzen bleiben tatsächlich in den Ursprüngen enthalten.
- Vollständig durchlaufender, familienübergreifender Nullfunktionsfall: quadratische Brüche mit nullstellenfreien Nennern werden mit der Polynom-/Potenzfamilie vereinigt. Alle Ursprünge bleiben erhalten, Repräsentanten bleiben über die Suchreihenfolgen gleich.
- Für feste, adaptive und erfahrungsbasierte Planung: alle kleineren positiven Operationsbudgets eines kleinen vollständigen Falls und jede globale Kandidaten-/Checkpoint-Abbruchposition geprüft; jede Ausgabe vollständig replaybar.
- Exakt ausreichendes Arbeitsbudget; Spurereignis- und Spurbytegrenzen einschließlich ausstehender UNION-Ereignisse geprüft. Summen von Teilrun-, Reihenfolge- und Vereinigungsoperationen ergeben die Gesamtarbeit. Erhaltene Originaltreffer und tatsächlich genutzte Spurbytes stimmen mit den Zählern überein.
- V8-Erfahrung für alle drei Ausdrucksplaner: passende Quelle wirkt, fremder Planer und bisheriger Gleichheitsmodus wirken nicht; eigene Lernpaare bleiben ausgeschlossen; Widerruf stellt die unbeeinflusste Reihenfolge wieder her. Alle fünf Familien bleiben enthalten.
- Gemischter V7/V8-Speicher: alter Datensatz und Kennung unverändert, beide Kontexte getrennt nutzbar, Deduplizierung gleicher Lernquellen auch über Planer hinweg. Neu gehashter Downgrade auf V7 wird abgewiesen.
- Erfahrungsbasierte V25-Läufe werden als neue Quellen zurückgewiesen.
- Import und eingebettetes Auswahl-Replay mit eigener Herkunft. Polstelle −1 weiterhin gesperrt; Replay nach Entfernen von Quelllauf, Erfahrungsspeicher und Ursprungsforschung geprüft.
- Manipulierte Vereinigungs-/Gleichheits-/Bereichs-/Cachepolicy und Ergebniszähler werden auch bei neu berechnetem Hash zurückgewiesen.
- Die bisherige negative Portfolio-Prüfung in den Q-Ausdruckstests wurde gezielt auf den weiterhin ungültigen Modus `syntax` umgestellt; `rational-q` ist jetzt ausdrücklich freigeschaltet.

Das unabhängige Review prüfte zusätzlich 140 randomisierte CLI-Budgetfälle über alle drei Ausdrucksplaner und feste/adaptive Reihenfolge. Additive Arbeitskosten, maximale Spurereignisse/-bytes und vollständige kanonische Replaygleichheit stimmten in allen Fällen.

## Gemessene Referenzfälle

### Vollständige Familienvereinigung

Eingabe: `examples/q-domain-zero-training.csv`, `examples/q-domain-zero-holdout.csv`; Parameter 0..1, Potenz-/Polynomgrenze 1, Ausdrucksgröße 3, Operatoren `+/`, Cacheplaner, feste Reihenfolge.

| Größe | Bisheriges `rational` | Neues `rational-q` |
| --- | ---: | ---: |
| Formelgruppen | 3 | 1 |
| Gespeicherte Originaltreffer | 7 | 7 |
| Logische Gesamtarbeit | 3147 | 3564 |
| Zusätzliche Gruppen-/Bereichsarbeit | – | 417 |
| Status | `AMBIGUOUS` | `VERIFIED_ON_SAMPLES` |

Beide Suchräume wurden vollständig abgearbeitet. Die zusätzliche Gleichheit betrifft insbesondere die Nullfunktion und die rational überall definierten Brüche `0/(x²+1)` und `0/(x²+x+1)`. Die Datenbeziehung bleibt auf die geprüften Paare beschränkt.

### Passende Q-Erfahrung

Die Ausdrucksquelle für x²/(x+1) ergibt V8-Stimmen `[0,0,0,0,1]`. Der passende Ziellauf führt die Ausdruckssuche zuerst aus und erhält dieselben vollständigen Gruppen wie der adaptive Referenzlauf. Gemessene Zielsuche: 84960 logische Einheiten. Zusätzliche Quellenprüfung: 88144 Einheiten, separat ausgewiesen. Widerruf, V25-Replay ohne externe Quellen und Auswahl-Replay ohne ursprüngliche Forschungsdatei sind Teil der Gesamtdemo.

Die Zahlen belegen keine niedrigere Gesamtlaufzeit. Q-Vergleiche kosten zusätzliche Arbeit; Erfahrung ist hier Planungseinfluss, kein allgemeines Lernen.

## Aussagegrenzen

Verglichen wird ausschließlich über Q. Höhere Restgrade können unentschieden bleiben; getrennte Gruppen beweisen daher nicht allgemein ihre Ungleichheit. Die feste Vereinigung macht gleiche vollständige Quellbefunde unabhängig von der Suchreihenfolge; bei Abbruch können andere Teilbefunde entstehen. Die nachgelagerte Vereinigung kann selbst am gemeinsamen Budget scheitern.

Allgemeinere Nullstellenentscheidungen, effizientere Vergleiche, externe Evidenz, weitergehende Experience-Korrekturketten und weitere Roadmap-Pakete bleiben offen. Kein vollständiger M2-/M4-/Gesamtprojektabschluss.
