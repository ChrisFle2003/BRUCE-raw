# Abnahme: Begrenzter Bereichscache – 2026-09-22

Stand: Core 0.31. Implementiert und aktuell verifiziert.

[Vertrag](../65-domain-cache-contract.md) · [Benutzung](../66-domain-cache-usage.md)

## Gelieferter Ablauf

Begrenzte Ausdruckssuche → Wertevergleich → modulare Bereichsprüfung oder Wiederverwendung einer exakt passenden bewiesenen Gleichheit → unveränderte Gruppen und vollständige Herkunft → V9-Export/Replay → Z-Auswahl. Der Cache ist optional, lauflokal und nur im eigenständigen Modus `rational-q-modular` zugelassen.

## Verifikation

| Prüfung | Ergebnis |
| --- | --- |
| Debug-Build | C++20 mit `-Wall -Wextra -Werror`; erfolgreich |
| Vollständige finale Debug-Suite | 59/59 bestanden |
| Vollständige UBSan-Suite | 59/59 bestanden; `-fsanitize=undefined -fno-sanitize-recover=all`, keine Sanitizer-Meldung |
| Gesamtdemo | Sämtliche bisherigen Abläufe und neuer Cache-/Auswahl-/Replaypfad erfolgreich |
| Historische Exporte | Alle 24 vorhandenen `.bruce`-Fixtures unverändert replaybar |
| Unabhängiges Review | Keine handlungsbedürftigen Befunde; Cache-Unit-Suite und ursprüngliche zwei Integrationstestmethoden unabhängig ausgeführt |
| Unabhängiger Altvergleich | Zwölf Core-0.30/Core-0.31-Ausgaben über drei Planer und vier bisherige Gleichheitsmodi byteidentisch; drei vollständige Cache-/Referenzläufe mit gleichen Hypothesen und Status |

Ein zunächst ungeeignetes zusätzliches Testbeispiel enthielt keine wiederholten Bereichsvergleiche und verletzte deshalb die Testannahme `hits > 0`. Es wurde durch den vollständigen vorhandenen Fall x²/(x+1) ersetzt. Die finale Debug-Gesamtsuite und UBSan-Gesamtsuite wurden mit den korrigierten drei Integrationstestmethoden erfolgreich ausgeführt. Dafür war keine Produktionskorrektur erforderlich.

## Gemessener Referenzlauf

Ausdruckssuche mit `cached-grammar`, Konstante 3, Operatoren `-^/`, neun Baumknoten und den Nullfunktionsbeispielen:

| Merkmal | Ohne Bereichscache | Mit `equal-64` |
| --- | --- | --- |
| Logische Gesamtarbeit | 461889 | 460519 |
| Formelgruppen | 5 | 5 |
| Originaltreffer | 128 | 128 |
| Status | INCOMPLETE | INCOMPLETE |
| Cachetreffer | – | 117 |
| Cacheverwaltung, bereits enthalten | – | 3957 |
| Cacheeinträge / Schlüsselbytes | – | 6 / 85 |
| Neu berechnete modulare Seitenzertifikate | 4 | 1 |

Die Einsparung beträgt 1370 logische Einheiten. Alle vollständigen Hypothesen inklusive IDs, Status, Normalformen und Originalausdrücken stimmen überein. Die gleiche Originalgrenze beendet beide Läufe; es wird keine vollständige Suche behauptet. Neue Beweise werden bei Cachetreffern nicht vorgetäuscht. Andere Aufgaben können durch Verwaltungskosten mehr Arbeit benötigen; keine allgemeine Laufzeit- oder RAM-Beschleunigungsbehauptung.

Der getrennte vollständige Fall x²/(x+1) bestätigt die Samplebeziehung und liefert bei der Auswahlstelle 0 den Wert 0. Die Polstelle −1 bleibt gesperrt. Nach Entfernen der ursprünglichen Forschungsdatei bleibt die Auswahl replaybar.

## Grenzprüfung

- Gleiches Cacheergebnis und unveränderte Ursprünge in allen drei Grammatikplanern.
- Erzwungene Platzkollision mit einem linearen Nenner und echter rationaler Definitionslücke: kein falscher Cachetreffer.
- Verdrängung eines vollständig bewiesenen Eintrags; keine Verdrängung durch DIFFERENT oder UNKNOWN.
- Größerer Schlüssel umgeht den Cache, statt die Mathematik abzubrechen; höchstens 64 gespeicherte Schlüssel und 262144 Schlüsselbytes.
- Jede Operationsgrenze vom Schlüsselaufbau über den kubischen Beweis bis zur Eintragung: kein halbfertiger Eintrag. Erneute Anfrage berechnet den Beweis vollständig.
- Kleine Forschungsaufgabe in allen Planern: jede kleinere positive Arbeitsgrenze, exakte Gesamtgrenze und alle Checkpointabbrüche mit vollständigem Replay und Budgetabrechnung.
- Ergebnis-, Spur- und Spurbytegrenzen sowie Auswahl und fehlende Ursprungsdatei.
- Manipulierte Cachepolitik, Zähler und Engineversion auch mit neuem Hash abgelehnt. Unzulässige Modi und Portfolioverwendung zurückgewiesen.

## Offene Arbeit

Portfolio und Erfahrungsplanung verwenden weiterhin ihre bisherigen Vergleiche ohne diesen Cache. Ihre versionierte Einbindung ist ein nächstes Paket. Weitere Suchoptimierungen, vollständigere Nullstellenverfahren, externe Evidenz und weitergehende Experience-Korrekturketten bleiben offen. Kein vollständiger M2-/Gesamtprojektabschluss.
