# Abnahme: Rationale Nullstellenzeugen – 2026-09-22

Stand: Core 0.33. Implementiert und aktuell verifiziert.

[Vertrag](../69-rational-witness-contract.md) · [Benutzung](../70-rational-witness-usage.md)

## Gelieferter Ablauf

Vorgegebene Polynombrüche → exakte Normalformen mit erhaltenen Nennerausschlüssen → bisherige modulare Abwesenheitsprüfung → begrenzte rationale Zeugensuche → unabhängige Gegenprüfung beider ursprünglicher Bereichsbedingungen → sichtbarer Beweis oder offener Befund → V3-Export und vollständiger Replay.

Die Option `--domain-policy modular-witness` gilt zunächst ausschließlich für den eigenständigen Vergleich. Der Standard und bisherige Forschungspolitiken bleiben unverändert.

## Verifikation

| Prüfung | Ergebnis |
| --- | --- |
| Debug-Build | C++20, `-Wall -Wextra -Werror`; erfolgreich |
| Vollständige Debug-Suite | 63/63 bestanden |
| Vollständige UBSan-Suite | 63/63 bestanden; `-fsanitize=undefined -fno-sanitize-recover=all`, keine Sanitizer-Meldung |
| Gesamtdemo | Alle bisherigen Abläufe sowie neue ganzzahlige/gebrochene Zeugen und offener Außenfall erfolgreich |
| Historische Exporte | 24 vorhandene `.bruce`-Fixtures unverändert replaybar |
| Vergleich mit Core 0.32 | 60 bisherige quadratische/modulare Vergleiche samt kleinen Arbeitsbudgets byteidentisch |
| Unabhängiges Review | Keine handlungsbedürftigen Befunde; zusätzlich 120 zufällige Altvergleiche byteidentisch und 39 Budgetgrenzen mit Replay geprüft |

## Mathematische Fälle

- `(x³−8)/(x³−8)` gegenüber 1: bisher modular `UNDECIDED`, neu `PROVEN_DIFFERENT`, Zeuge 2.
- `(8x³−1)/(8x³−1)` gegenüber 1: Zeuge 1/2. Negative, nullwertige und rechtsseitige Zeugen sind ebenfalls geprüft.
- Grenzwerte 16 und 1/16 werden gefunden. Tatsächliche Lücken bei 17 und 1/17 bleiben offen.
- Konstruierte Polynome `(b*x−a)*(x²+1)` mit rationaler Skalierung: ausgegebene Zeugen unabhängig mit Python `Fraction` eingesetzt und mit a/b verglichen.
- Gemeinsame Nullstellen werden entfernt: `x*(x³−8)` und x unterscheiden sich bei 2, nicht bei der gemeinsamen Lücke 0. Skalierte identische Nennerbereiche bleiben gleich.
- `x³−2` wird weiterhin modular als rational nullstellenfrei bewiesen; dafür ist keine Kandidatensuche nötig.
- `(x²−2)*(x²−3)*(x²−6)` hat keine rationale Nullstelle, aber die verwendeten modularen Prüfungen finden kein Abwesenheitszertifikat. Die endliche Suche bleibt korrekt `UNDECIDED`.
- Grad 64 mit `x⁶⁴−1`: konkreter Zeuge 1 innerhalb der bestehenden Gradgrenze.

## Unterbrechung und Evidenz

Für die vollständigen Referenzläufe mit Zeugen 2 und 1/2 wurden jede kleinere positive Arbeitsgrenze, jede Abbruchposition, die exakte Abschlussgrenze sowie Callback-Abbrüche geprüft. Alle Teilbefunde rechnen exakt ab und sind replaybar. Unfertige Bereichsentscheidungen veröffentlichen weder einen Zeugen noch Teilzähler. Zusätzlich wurden endliche erfolglose Suche und Bitgrenzen geprüft.

Manipulation von Zeuge, Kandidatenzähler, Politik und Versionsheader wird auch bei neu berechnetem Dateihash abgelehnt. Bestehende Tests für ungültige Eingaben und Schutz vorhandener Ausgabedateien laufen weiterhin mit.

Ein zusätzliches Bitgrenzentestbeispiel `x³−125` war zunächst ungeeignet: Bis zum gefundenen Zeugen 5 passen alle Zwischenwerte in acht Bits. Das Beispiel wurde durch `x³−1/8` ersetzt; dort überschreitet die Auswertung während der vorgeschriebenen Kandidatenreihenfolge nachweislich die Grenze. Keine Produktionskorrektur war dafür erforderlich.

## Offene Arbeit

Keine vollständige rationale Nullstellenentscheidung und keine automatische Integration in Ausdruckssuche, Portfolio, Erfahrung oder Bereichscache. Diese Einbindung erfordert eigene Versionen. Weitere Nullstellenverfahren, Suchoptimierung, externe Evidenz und Experience-Korrekturketten bleiben offen. Kein vollständiger M2-/Gesamtprojektabschluss.
