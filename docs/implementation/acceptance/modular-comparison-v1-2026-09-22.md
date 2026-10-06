# Abnahme: Modulare Nullstellenausschlüsse – 2026-09-22

Stand: Core 0.29. Implementiert und aktuell verifiziert.

[Vertrag](../61-modular-comparison-contract.md) · [Benutzung](../62-modular-comparison-usage.md)

## Gelieferter Ablauf

Zwei vorgegebene Polynombrüche → exakte Normalisierung mit ursprünglichen Nennerausschlüssen → gemeinsame Faktoren entfernen → bisherige Nullstellenentscheidung plus optionale Primzahlzertifikate → Gesamtbefund über Q → V2-Export und vollständiger Replay.

## Verifikation

| Prüfung | Ergebnis |
| --- | --- |
| Debug-Build | C++20, `-Wall -Wextra -Werror`; erfolgreich |
| Vollständige Debug-Suite | 55/55 bestanden |
| Vollständige UBSan-Suite | 55/55 bestanden; `-fsanitize=undefined -fno-sanitize-recover=all`, keine Sanitizer-Meldung |
| Finale modulare Integrationstests | Vier Testmethoden in Debug und UBSan bestanden, einschließlich beider Zertifikatsseiten und Header-Manipulation |
| Gesamtdemo | Alle bisherigen Abläufe plus neuer V2-Vergleich/Replayschritt erfolgreich |
| Historische Exporte | Alle 24 `.bruce`-Fixtures liefern unverändertes vollständiges Replay-JSON |
| Unabhängiges Review | Keine handlungsbedürftigen Befunde; 150 zusätzliche zufällige Vergleiche gegen Core 0.28 liefern byteidentische Standardausgaben, Fehler und Rückgabecodes |

Die Debug-Gesamtsuite lief vor den letzten zusätzlichen Assertions in der neuen CLI-Testdatei; deren finaler Stand wurde danach unter beiden Builds gezielt geprüft. Produktionscode blieb unverändert.

## Gezielte Abnahme

- x³−2: `UNDECIDED` im bisherigen Modus, `PROVEN_EQUIVALENT` mit Primzahl 7 im neuen Modus. Vollständige Restwerttabelle `[5,6,6,4,6,4,4]`; Kosten 278 → 421 logische Einheiten. Das ist zusätzliche Beweisarbeit, kein Geschwindigkeitsversprechen.
- Symmetrische Seiten, zwei höhere Restpolynome, gemeinsame Faktoren und leere Definitionsbereiche.
- Rationale Koeffizienten, negative Skalierung, nachlaufende Nullen, x⁶⁴+1 und Bitgrenze.
- 80 zufällige Polynome: ausgegebene Zertifikate durch unabhängige direkte Potenzauswertung in Python gegengeprüft.
- 80 konstruierte Polynome mit bekannten rationalen Nullstellen: kein falscher Gleichheitsbeweis.
- Nenner, die alle elf Primzahlen enthalten: unzulässige Restklassenübertragungen werden übersprungen; rationale Definitionslücke bleibt offen.
- P=(x²−2)(x²−3)(x²−6): keine rationale Nullstelle, aber kein Zertifikat durch dieses begrenzte Verfahren; der Vergleich bleibt `UNDECIDED`.
- Für fünf Referenznenner und beide Politiken: jedes kleinere positive Operationsbudget, jeder mögliche Abbruchpunkt und exakte Gesamtgrenze geprüft, einschließlich vollständigem Replay.
- Manipulierte Politik, Primzahl, Restwerttabelle und Versionsheader werden auch bei passendem neuem Hash abgelehnt.

## Aussagegrenze und nächste Arbeit

Der Beweis betrifft die vorgegebenen Polynombrüche als partielle Funktionen über Q. Er beweist keine Datenhypothese und keinen reellen Definitionsbereich. Höhere rationale Nullstellen werden mit diesem Zusatzverfahren nicht allgemein gefunden; ein fehlendes Zertifikat bleibt offen.

Ausdruckssynthese, Portfolio und Erfahrung sind noch nicht an die neue modulare Politik angeschlossen. Deren versionierte Integration ist ein nächstes mögliches Paket. Kein vollständiger M2-/Gesamtprojektabschluss.
