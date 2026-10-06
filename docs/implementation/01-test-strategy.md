# Teststrategie und Abnahme

## Grundsatz

Ein bestandener Test beweist nur seinen getesteten Vertrag. Kein Testlauf allein beweist mathematische Wahrheit oder Gesamtfehlerfreiheit.

## Testebenen

| Ebene | Zweck | Beispiele |
| --- | --- | --- |
| Unit | einzelne Werteobjekte und Regeln | Z-Paarung, Score, Tokenisierung |
| Property | Invarianten ueber viele Werte | `unpair(pair(x,y)) == (x,y)` |
| Integration | Modulgrenzen | Snapshot -> Auswahl -> Trace |
| Replay | historische Reproduktion | alte Regeln/Token wiederholen |
| End-to-End | sichtbarer Nutzerpfad | Input -> Erklaerung -> Replay |

## Pflichtmatrix

| ID | Test | Erwartung |
| --- | --- | --- |
| COORD-01 | Cantor-Bijektivitaet | Rueckrechnung liefert exakt x/y |
| COORD-02 | lokales 8x8 | `8*y+x` bleibt eindeutig |
| COORD-03 | Wert-/Adresskollision | `2^6` und `4^3` behalten getrennte Adressen |
| MATH-01 | `0^0`-Policy | explizit `ValueOne` oder `ReservedStop` |
| COMP-01 | deterministischer Score | gleicher Snapshot, gleiches Ergebnis |
| COMP-02 | Tie-Break | dokumentierte Reihenfolge entscheidet stabil |
| COMP-03 | Mehrdeutigkeit | vollstaendiger Gleichstand bleibt sichtbar |
| TOKEN-01 | Tabellen-Version | alter Trace nutzt alte Token-ID-Semantik |
| EXP-01 | append-only | alte Erfahrung bleibt bytegleich erhalten |
| EXP-02 | Korrektur | neue Erfahrung referenziert alte |
| EXP-03 | Selbstbestaetigung | kein LearnGate ohne unabhängige Evidenz |
| EVID-01 | manipulierte Evidenz | Digestfehler wird abgelehnt |
| CACHE-01 | Verdraengung | Cacheverlust aendert Ergebnis nicht |
| BUDGET-01 | Kandidatenlimit | kontrollierter StopReason und Trace |
| REL-01 | Gegenbeispiel | alte Relation wird sichtbar entkraeftet |
| HOLDOUT-01 | mathematische Hypothese | Hold-out ist nicht Teil der Stuetzdaten |
| E2E-01 | Gesamtpfad | Input -> Token -> z -> Filter -> Evidenz -> Erklaerung -> Replay |

## Vorgeschlagene Befehle nach Implementierungsstart

```bash
cmake -G Ninja -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Status

**Ziel-Testmatrix mit erstem ausführbarem Harness.** CTest führt C++-Vertragsprüfungen und Python-CLI-Integrationstests aus. Die Zielmatrix ist nicht vollständig implementiert; der aktuelle Umfang steht im [Core-V1-Benutzungsstand](06-core-v1-usage.md).
