# Abnahme: Polynombrüche und rationale Definitionsbereiche – 2026-09-22

Stand: Core 0.26. Implementiert und aktuell verifiziert.

## Gelieferter Durchstich

Vier explizite Koeffizientenlisten → exakte Bruchnormalisierung mit erhaltenen Ausschlüssen → GGT der Ausschlusspolynome → rationale Nullstellenprüfung für Restgrade bis 2 → getrennte Werte-/Bereichsbefunde → Gesamtstatus und gegebenenfalls gegengeprüfte Ausschlussstelle → Export und vollständiges Replay.

[Vertrag](../55-fraction-comparison-contract.md) · [Anleitung](../56-fraction-comparison-usage.md)

## Verifikation

| Prüfung | Nachweis |
| --- | --- |
| Debug-Build | C++20, bestehende Warnungen mit `-Wall -Wextra -Werror`; erfolgreich |
| Vollständige Testsuite | `ctest --test-dir build --output-on-failure -j 4`: 50/50 bestanden |
| Undefiniertes Laufzeitverhalten | Vollständige Suite mit `-fsanitize=undefined -fno-sanitize-recover=all`: 50/50 bestanden, keine Sanitizer-Meldung |
| Durchgängige Demo | `python3 scripts/demo.py`: einschließlich der bisherigen Forschungs-/Experience-/Auswahlabläufe und vier neuer Vergleichs-/Replayfälle bestanden |
| Historische Exporte | Alle 24 vorhandenen `.bruce`-Fixtures in neuen Prozessen wiedergegeben und mit ihrem gespeicherten vollständigen JSON-Ergebnis verglichen: identisch |
| Unabhängiges Code-Review | Keine handlungsbedürftigen Befunde; zusätzliche 250 Fälle mit bekannten rationalen Nullstellenmengen geprüft |

Die neue CLI-Testdatei enthält fünf Testgruppen. Eine Gruppe wurde nach der vollständigen Debug-Suite um die unabhängige Funktions-/Definitionsbereichsreferenz erweitert und anschließend separat erfolgreich ausgeführt; die vollständige UBSan-Suite umfasst diesen erweiterten Stand.

### Neue gezielte Nachweise

- Gleichheit auf Q trotz formal verschiedener Ausschlusspolynome: `x²+1`, `x²−2`, gemeinsame Faktoren und rationale Skalierungen.
- Echte Lücken bleiben erhalten: `x/x`, wiederholte Nullstellen, entfernbare Lücken von `(x²−1)/(x−1)`, Nullfunktion mit Ausschlüssen.
- Leere Definitionsbereiche: beide leer gleich, nur eine Seite leer verschieden.
- Höhere Restgrade bleiben offen. Algebraisch verschiedene Wertefunktionen und nachgewiesene unterschiedliche Lücken können den Gesamtvergleich dennoch entscheiden.
- 80 deterministisch erzeugte quadratische Fälle gegen eine separate Python-Ganzzahl-/Bruchreferenz; ausgegebene Wurzeln erfüllen das ursprüngliche Polynom exakt.
- 100 weitere deterministisch erzeugte Funktionspaare: unabhängige Kreuzmultiplikation, exakte rationale Nullstellenmengen und Prüfung jeder ausgegebenen Ausschlussstelle.
- Review: 250 weitere Fälle mit rationalen Linearfaktoren, nullstellenfreien quadratischen Faktoren, Wiederholungen und rationalen Skalierungen. 106 bewiesen verschieden, 23 bewiesen gleich, 121 mit der begrenzten Methode offen; keine widersprüchliche Behauptung, alle Zeugen auf genau einer Seite ausgeschlossen.
- C++-Vertragstests: alle kleineren positiven Arbeitsbudgets und sämtliche Abbruchpositionen für fünf repräsentative Fälle, jeweils vollständiger Replay-Vergleich; exakt ausreichendes Budget; Callback-Abbruch einschließlich gespeicherter Position.
- Eingabegrenzen: leere/fehlerhafte Listen, ungültige rationale Literale, über 65 Koeffizienten, Bits-/Arbeitsgrenzen, unbekannte/mehrfache Optionen; zulässiger Grad 64.
- Zwischenwert-Bitgrenze führt zu `INCOMPLETE` und reproduzierbarem Abbruch.
- Export wird nicht überschrieben. Manipulierte Policy, Status und Operationszahl werden auch nach neu berechnetem Hash abgewiesen; angehängte Daten ebenfalls.

## Gemessene Beispiele

Die erweiterte Gesamtdemo vergleicht jeweils `p(x)/p(x)` mit `1` auf Q:

| p(x) | Status | Logische Einheiten |
| --- | --- | ---: |
| x²+1 | `PROVEN_EQUIVALENT` | 255 |
| x²−2 | `PROVEN_EQUIVALENT` | 257 |
| x | `PROVEN_DIFFERENT`, gegenprüfte Stelle 0 | 187 |
| x³−2 | `UNDECIDED` | 278 |

Alle vier vollständigen Befunde wurden in einem neuen Prozess identisch reproduziert. Die Arbeitseinheiten sind keine CPU-Zeitmessung und kein Beschleunigungsvergleich.

## Aussagegrenzen

Der algebraische Nachweis gilt für die beiden vorgegebenen partiellen Funktionen über Q. Er erhebt keine Stichprobenhypothese zum allgemeinen Gesetz und trifft keine Gleichheitsaussage über R. Höhergradige Restfaktoren werden nicht allgemein faktorisiert. Ausdruckssuche, Portfolio, Experience, Z-Import und alle bestehenden Forschungsformate verwenden unverändert ihre bisherigen Regeln. Ihre Einbindung sowie weitere Roadmap-Pakete bleiben offen; kein vollständiger M2-/Gesamtprojektabschluss.
