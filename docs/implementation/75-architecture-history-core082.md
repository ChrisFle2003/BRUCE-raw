> Historischer Dokumentstand, am 26.09.2026 archiviert. Für aktuelle Aussagen gilt [75-current-architecture-and-milestones.md](75-current-architecture-and-milestones.md). Überholte Einschränkungen und Entwürfe sind hier bewusst unverändert erhalten.

# Tatsächliche Architektur und Meilensteinabgleich – Core 0.82

Stand: 25. September 2026. Diese Bestandsübersicht beschreibt den aktiven Ordner `Bruce_raw`. Andere BRUCE-/KEVIN-Arbeitsordner gelten nicht als Implementierungsnachweis. Die [Roadmap](04-roadmap.md) enthält das weitergehende Ziel; die [Abnahmematrix](136-m0-m2-acceptance-matrix.md) trennt vorhandene Teilverträge von noch offenen Gesamtmeilensteinen.

## Ausführbare Struktur

```text
CLI / strukturierte Dateien
  → ApplicationTask: acht typisierte Auftragssorten
  → gemeinsame Ausführung mit fachlichen Adaptern
  → Mathematik / Grid / Bruchvergleich / numerische Z-Auswahl
  → ApplicationRun: passender Befund mit Status, Aussagegrenze und Trace
  → versioniertes Bundle / gemeinsamer typisierter Replay

Optionaler geprüfter Sitzungscontroller
  CREATED → EXECUTING → SERIALIZING → REPLAYING → READY
                    jeder Arbeitsabschnitt → FAILED
  → optionales gemeinsames Prüfpunktbudget mit Phasenabrechnung
  → getrennte native Abbruch- und Prüfcallbacks

Geprüfte unabhängige Portfolioquellen
  → Experience-Snapshot, Widerruf und explizite Ersetzung
  → kontextgebundene Stimmen für die Reihenfolge eines neuen Portfolios

Grid-Stapel / Fortsetzung
  → feste exakte X- oder Y-Scheibe mit eingebetteter Herkunft
  → vorhandene Formelforschung → Z-Auswahl / Portfolio-Erfahrung
```

CMake baut die C++20-Bibliothek `bruce_core` und CLI `bruce`. GMP liefert exakte große Ganzzahlen und Brüche, OpenSSL die Digests. Die Referenzausführung ist sequenziell. Ein Digest ersetzt keine mathematische Gegenprüfung; Replay berechnet Inhalte erneut. READY bestätigt die erfolgreiche Sitzungsprüfung, keinen allgemeinen mathematischen Beweis.

Core 0.73 ergänzt `ApplicationAssessment`: Status, Stopgrund, Vollständigkeit und typisierte Aussagebereiche werden gemeinsam aus allen acht Ergebnisvarianten abgeleitet. Bestehende Inspektion und Sitzung verwenden diese Bewertung. `assess-run` bietet einen separaten versionierten, replaygeprüften CLI-Vertrag; die nativen Bundleformate bleiben erhalten. Details: [gemeinsame Ergebnisbewertung](153-result-assessment-usage.md).

| Bereich | Implementierter Bestand | Grenze |
| --- | --- | --- |
| `core`, `coordinate` | Grundtypen, lokale 8×8-Adressen, Cantor-Paarung und Pfadparser | Kein allgemeines BigInt-Adressbackend |
| `math` | Exakte Zahlen, fünf Formelfamilien, begrenzte Ausdrucksgrammatik, Normalformen, Bereichsvergleich und lokale Caches | Keine allgemeine Mathematikentscheidung |
| `application` | Acht Auftrags-/Ergebnisvarianten, gemeinsamer Einstieg und Replay, zentraler Exact-Validator, zehn eingebaute Strategierouten, Forschungs-Gates und geprüfte lineare Sitzung | Keine dynamischen Strategie-Plugins, allgemeine Gate-Zustandsmaschine oder Phase-DAG |
| `compatibility` | Numerische Snapshots, Masken, harte Ausschlüsse, Ranking, Gleichrangigkeit und geprüfte Forschungsimporte | Token-/Sprachrollen und externe Evidenz nicht integriert |
| `experience` | Quellenreplay, unveränderliche Exportsnapshots, Kontexttrennung, Stimmen, Deduplizierung, Widerruf und explizite Ersetzungsketten | Keine automatische Regelpromotion oder allgemeine Lernarchitektur |
| `evidence` | Begrenzte Bundles, Integritätsprüfung und anwendungsbezogene Replays | Kein universeller externer Evidenzdienst |
| `apps` | CLI und lokale Demonstrationsabläufe | Kein Cockpit |

Token-/Aufgabenverständnis, NanoCell-Runtime, Phase-DAG, Cockpit und GPU-Ausführung sind im aktiven Quellbaum noch nicht entsprechend integriert.

## Budgets, Steuerung und verbleibende atomare Arbeit

Die lokale Mathematik behält ihre fachlichen Operations-, Kandidaten-, Ergebnis- und Tracegrenzen. Grid-Forschung hat zusätzlich einen logischen Import-/Forschungsrahmen mit Quellenbytezulassung; gelernte Planung berücksichtigt darin Experiencebytes. Die Prüfsitzung kann außerdem Bundlebytes vor dem Replay begrenzen.

Core 0.65 ergänzt einen gemeinsamen [Prüfpunktzähler](135-session-work-budget-usage.md) über alle drei Arbeitsphasen. Ein Punkt ist eine Kontrollstelle, keine einheitliche Rechenoperation. Das Budget nutzt vorhandene native Such-/Abfragepunkte sowie passende Quellaudits, Serialisierungs- und Replaypunkte automatisch. Überschreitung endet mit Phase und Verbrauch in FAILED; native Teilbefunde und mathematische Bundles bleiben bei ausreichendem Budget unverändert.

| Abschnitt | Vorhandene innere Kontrollpunkte | Verbleibende Grenze |
| --- | --- | --- |
| Erstlauf | Native Such-/Abfragepunkte; Experience-, Grid-, Z-Forschungsquellen und Fortsetzungsimporte einschließlich Cache-Präfixaufbau | Einzelberechnungen, Quellhashes und Kopien bleiben teilweise atomar |
| SERIALIZING | Äußere SHA-256-Verarbeitung in Blöcken; Grid-Stapel/-Fortsetzung zwischen Datensätzen; seit 0.67 Fortsetzungs-Elternkopie in 64-KiB-Blöcken | Speicherreservierungen, andere Payloadfamilien, einzelne große Datensätze, weitere Kopien und Zusammenbau nicht blockweise instrumentiert |
| REPLAYING | Seit 0.81 auch Grid-Einzelreplay mit Digest- und Auswertungsgrenzen; Forschung/Portfolios, Erfahrungen, Grid-Stapel und Fortsetzungen, Grid-Forschung, Bruchvergleich und Z-Quellen | Atomare Unteroperationen einschließlich der eigentlichen Grid-Einzelauswertung; keine garantierte maximale Signallatenz |
| Außerhalb der Sitzung | Vorprüfung/Dateieinlesen und exklusiver Export haben eigene Fehler | Kein gemeinsames Zeit-/RAM-Limit für den Gesamtprozess |

Core 0.68 ergänzt ein separates [Bundlegrößenbudget](142-serialized-byte-budget-usage.md): Die erzeugte Bundlegröße wird nach SERIALIZING gemessen und vor REPLAYING zugelassen oder abgewiesen. Das verändert keine Bundles und ist keine Prozessspeichergrenze.

Core 0.69 ergänzt für `GridResearch` den [blockweisen Kopierpfad](144-grid-research-copy-usage.md). Die bestehende Serialisierungsbeobachtung erreicht damit auch große eingebettete Forschungsbundles; sie ändert weder Formatbytes noch die Replayprüfung.

Core 0.70 ergänzt diesen Pfad für `experience_snapshot` in `ExactResearch`. Bei aktiviertem gemeinsamen Sitzungsbudget werden auch diese Blöcke gezählt; direkte Serialisierungs-Callbacks bleiben bytegleich und replaykompatibel.

Core 0.71 ergänzt denselben Kopierpfad für `Compatibility`-Regeln, Kandidaten und Ursprungsbundles. Core 0.72 bindet zusätzlich `FractionComparison`-Aufgaben- und Ergebnispayloads an. Beide Erweiterungen ändern weder Bundleformat noch Replaybytes.

Die acht Auftragstypen sind angebunden. Ein universelles Kostenmodell für mathematische Operationen, Prozessspeicher und Zeit existiert weiterhin nicht. Signale und Sitzungsbudgets sind getrennt: zusätzliche Budgetpunkte erzeugen keine zusätzlichen Aufrufe bestehender Signalcallbacks.

## Meilensteine M0–M2

| Meilenstein | Nachgewiesener Teil | Noch offen |
| --- | --- | --- |
| M0 – Startverträge | Gemeinsame Aufgaben/Ergebnisse, Exact-Validierung, Strategiekatalog, reproduzierbare Bundles, lineare Phasen und strukturierte Fehler | Universelle Versionierungs-/Gateverträge für künftige Runtime-Arten; weitere Installationszielumgebungen, öffentliche Releaseverwaltung und Signaturverträge |
| M1 – Prüfbarer Kern und Auswahl | Reversible lokale Adressen, numerische Z-Auswahl, Ranggleichheit, Budgets, Abbrüche, Export und historische Replayformate | Vollständige Rollen-/Evidenzsemantik, allgemeine Gate-Geltungsbereiche und Artefaktverwaltung |
| M2 – Forschung und Grid | Fünf Formelfamilien, begrenzte Ausdruckssynthese, Gegenrechnung/Hold-out, Herkunft, GridSpec, Cache und feste X-/Y-Projektion | Dynamischer Strategie-Port, allgemeinere Probleme/Operatoren, mehrdimensionale Projektion, Expansion und BigInt im Grid/Z |

Grid-Fortsetzungen rekonstruieren ihre geprüfte Elternkette und den FIFO-Cache bis acht Generationen. Quellenprüfung und Cacheaufbau haben inzwischen Kontrollpunkte; allgemeine Forschungswiederaufnahme und persistente Cacheabbildungen fehlen weiterhin. Die lokale Byteabrechnung ist keine Prozess-RAM-Grenze.

**Bewertung:** M0–M2 besitzen tragfähige, ausführbare und einzeln geprüfte Teilverträge. Keiner dieser Gesamtmeilensteine wird allein durch die neue Budgetanbindung als abgeschlossen markiert.

## Weitere Meilensteine

M3 (mathematisches Aufgabenverständnis), M5 (NanoCell), M6 (Cockpit) und M7 (Phase-DAG) bleiben weitgehend Zielarchitektur. M4 besitzt begrenzte Erfahrungsplanung und explizite Korrekturketten, aber keine vollständige Lernarchitektur. Lokale Suchoptimierungen aus M8 sind vorhanden; parallele oder GPU-Ausführung folgt daraus nicht.

## Nächste Konsolidierungsschritte

1. Die [M0–M2-Abnahmematrix](136-m0-m2-acceptance-matrix.md) als Grundlage für konkrete Abschlussentscheidungen verwenden; Umfang und Restlücken explizit halten.
2. Die verbliebenen atomaren Arbeitsblöcke instrumentieren und separate Speicher-/Byteverträge definieren. Prüfpunktzahlen nicht nachträglich als universelle Rechenkosten ausgeben.
3. Die [lokale Installations-/Archivabnahme](138-installation-usage.md) auf isolierte Fremdrechner und öffentliche Releaseverträge erweitern. Core 0.66 prüft frischen Build, verschobene Installation und historische Wiederherstellung in der vorhandenen Linux-Umgebung.
4. Allgemeine Strategie-/Gateverträge und nutzerseitige Arbeitsabläufe auf dem konsolidierten Kern ausbauen. Der gemeinsame Ergebnisvertrag aus Core 0.73 ist ein implementierter Baustein; er erteilt keine automatischen Lern- oder Veröffentlichungsfreigaben. Weitere Leistungsoptimierungen sind zugunsten der Architekturarbeit zurückgestellt.

Der aktive Arbeitsordner ist kein gültiges Git-Repository. Es wurde keines initialisiert und kein Release öffentlich veröffentlicht. Core 0.81 wurde am Nutzerpfad `./build/bruce` auf Bruce-AI als Release-Build, Installation und Archiv geprüft. Apache-2.0, NOTICE mit Christopher Flegel als Rechteinhaber und Systembibliothekshinweise sind vorhanden. Eine Release-Signatur und weitere Zielumgebungen bleiben offen. Die Benchmark-Suite V1 liefert erste Messdaten; weitere Optimierung ist derzeit nachrangig.

## Herkunft der Architekturentscheidungen

Die [gemeinsame Portfolio-Versionsauswahl](../adr/009-five-family-wire-versions.md) und native historische Decoder bleiben erhalten. Maßgebliche Ausbauschritte: [Forschungsverträge](97-research-contract-usage.md), [Aufträge](99-application-task-usage.md), [Ergebnisinspektion](101-application-result-usage.md), [Sitzungsphasen](103-execution-phases-usage.md), [Fehler](105-execution-failure-usage.md), [Replaybytes](107-replay-byte-budget-usage.md), [Erfahrungsreplay](121-experience-replay-checkpoints-usage.md), [Digestpunkte](123-serialization-digest-checkpoints-usage.md), [Grid-Serialisierung](125-grid-serialization-records-usage.md), [native Erfahrung](127-execution-experience-checkpoints-usage.md), [Gridquellen](129-execution-grid-source-checkpoints-usage.md), [Z-Quellen](131-execution-compatibility-source-usage.md) und [Fortsetzungsimporte](133-execution-resume-import-usage.md). Historische Einschränkungen in diesen einzelnen Lieferdokumenten sind anhand dieser aktuellen Übersicht einzuordnen.

Core 0.74 ergänzt [typisierte Forschungszulassungsgründe](154-research-gate-reasons-usage.md) und den Regelkatalog `gate-contracts`. Die bestehenden Verbraucher verwenden weiter denselben Gateauswerter; eine allgemeine Ableitung aus allen Anwendungsergebnissen bleibt offen.

Core 0.75 ergänzt [gate-run](155-run-gates-usage.md): replaygeprüfte Auswahlzulassung und Erfahrungsquellenprüfung ohne Speichermutation; keine allgemeine Gatezustandsmaschine.

## Blueprint-Fortschreibung

Die [Konsolidierung nach Core 0.81](../architecture/03-blueprint-consolidation.md) beschreibt die tatsächlichen Modulabweichungen und konkrete nächste Abnahmen. Core 0.76–0.81 ergänzen typisierte Gateberichte, gemeinsame Replay-/Auditprüfpunkte, CLI-Prüfpunkt- und Eingabegrenzen, Signalsteuerung und Einzel-Grid-Replaysteuerung. Der nächste Schwerpunkt ist ein Anwendungseinstieg für explizite Erfahrungsaktionen, anschließend die Bereinigung der Modulabhängigkeiten.

Core 0.82 ergänzt [ExperienceTask](162-experience-application-usage.md): eigener Anwendungseinstieg für explizite Snapshotaktionen. Keine neue Regelaktivierung und kein geschützter menschlicher Freigabenachweis.
