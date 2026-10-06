# Abnahme: Portfolio-Gegenbeispiele und Architektur-Konsolidierung – 2026-09-22

Stand: Core 0.35. Implementiert und aktuell verifiziert.

[Vertrag](../73-witness-portfolio-contract.md) · [Benutzung](../74-witness-portfolio-usage.md) · [Architekturabgleich](../75-current-architecture-and-milestones.md)

## Gelieferter Ablauf

Fünferportfolio mit gemeinsamer begrenzter Zeugensuche → feste/adaptive/erfahrungsbasierte Reihenfolge → erhaltene Originale und nachgelagerte Familienvereinigung → V32/V33/V34-Export und Replay → unabhängige Quelle im V11-Erfahrungsspeicher → Widerruf → Z-Auswahl und Replay ohne ursprüngliche Quellen.

Zusätzlich: gemeinsame Fünferportfolio-Versionsauswahl für Ausgabe, Export und Replay; aktualisierte Architektur-/Phasenverweise, ADR-009 und konkrete offene M0–M2-Abnahmen. Die Konsolidierung führt noch keinen universellen Strategie-/Gate-Port ein.

## Verifikation

| Prüfung | Ergebnis |
| --- | --- |
| Debug-Build | C++20, `-Wall -Wextra -Werror`; erfolgreich |
| Vollständige Debug-Suite | 67/67 Testgruppen bestanden |
| Vollständige UBSan-Suite | 67/67 bestanden; `-fsanitize=undefined -fno-sanitize-recover=all`, keine Sanitizer-Meldung |
| Gesamtdemo | Alle bisherigen Abläufe und neuer Portfolio-/Experience-/Widerrufs-/Auswahl-/Replaypfad erfolgreich |
| Historische Fixtures | 24 vorhandene `.bruce`-Dateien unverändert replaybar |
| Unabhängiger Versionsvergleich | Sämtliche 24 bisherigen Fünferportfolio-Versionen V8–V31: JSON und Bundlebytes identisch zum vorherigen Binary, Replay identisch |
| Unabhängiges Review | Keine handlungsbedürftigen Befunde; neue Unit-Suite zusätzlich unabhängig ausgeführt |

## Mathematischer Referenzlauf

`portfolio-expression`, `cached-grammar`, feste Reihenfolge, `rational-q-witness`, Operatoren `-^/`, Konstante 3, neun Knoten, maximal 256 Originale, neue Nullfunktionsbeispiele:

- 561354 logische Gesamtoperationen.
- 256 Originaltreffer; vier abgeschlossene ausgewählte Gegenzeugen aus begrenzter Suche.
- Diese vier entstehen im Ausdruckskind. Die Familienvereinigung verwendet dieselbe Politik, benötigt in diesem Referenzfall jedoch keinen zusätzlichen begrenzten Zeugen.
- Status bleibt `INCOMPLETE / ChildIncomplete` wegen der Ergebnisgrenze des Ausdruckskinds. Keine vollständige Suche behauptet.
- Direkte Vereinigungstests mit höhergradigen Bereichsbedingungen prüfen zusätzlich tatsächlich in der Vereinigung gefundene Zeugen, modulare Abwesenheitsbeweise, begründet offene Fälle und jede Arbeitsgrenze vor Abschluss. Unfertige Zeugen bleiben unsichtbar; der unvollständige Ursprung bleibt separat erhalten.

## Ressourcen, Erfahrung und Replay

Alle drei Ausdrucksplaner und alle drei Reihenfolgen sind geprüft. Kleine vollständige Aufgaben durchlaufen jede positive Operationsgrenze, sämtliche Checkpoint-/Kandidatenabbrüche sowie Trace-/Bytegrenzen mit Kosten-, Herkunfts- und Replayprüfung. Die Grenze 256 gilt gemeinsam; die ersten vier Familien behalten zusätzlich jeweils höchstens 128 Ergebnisse. Standard bleibt 128.

Sieben Integrationstestmethoden prüfen vollständige Familienzusammenführung, alle 31 nichtleeren Operatorauswahlen, literale Potenzen, tatsächliche Gegenzeugen mit 256 Originalen, identische vollständige Befunde zwischen Reihenfolgen und unveränderte erste vier Familien bei den Standardreferenzen.

V11-Erfahrung ist vom alten Kontext und von anderen Ausdrucksplanern getrennt. Selbstbezug und doppelte Lernquellen sind ausgeschlossen. Gemischte Speicher behalten bestehende Datensätze; manipulierte Versionsdowngrades werden abgelehnt. Widerruf entfernt den späteren Planungseinfluss. V34-Läufe und unvollständige Läufe können keine neuen Erfahrungsquellen werden.

Die vollständige Quelle liefert `[0,0,0,0,1]` Stimmen. Die Auswahl bei 0 liefert 0, die Polstelle −1 bleibt gesperrt. Forschungs-/Auswahl-Replay funktionieren nach Löschen ursprünglicher Quellen, Regel- und Erfahrungsdateien. Manipulierte Metadaten, Cache-/Vergleichspolitik und Zähler werden auch nach Neuberechnung des Dateihashes abgelehnt.

Ein zunächst aus dem modularen Vorgängertest übernommener Manipulationstest suchte noch dessen alten Policytext; dadurch wurde gar keine Änderung erzeugt. Der Test wurde auf den neuen vollständigen Policybezeichner korrigiert. Keine Produktionskorrektur war dafür nötig.

## Verbleibende Arbeit

Die Architekturmatrix benennt gemeinsame Aufgaben-/Gate-/Strategieverträge, GridSpec/Cache/Replay, allgemeine Experience-Korrekturketten und Release-/Versionsverwaltung als offen. Der Bereichscache `equal-64` bleibt an seine alte Vergleichspolitik gebunden. Keine pauschale M0-/M1-/M2-/M4-/Gesamtprojektabnahme.
