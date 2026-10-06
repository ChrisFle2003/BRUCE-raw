# Tatsächliche Architektur und Meilensteine – Core 0.93

Stand: 26.09.2026. Maßgeblich sind der aktive lokale Quellbaum und der Nutzerpfad auf Bruce-AI. [Konsolidierte Modulübersicht](../architecture/03-blueprint-consolidation.md), [historische Übersicht Core 0.82](75-architecture-history-core082.md).

| Bereich | Implementierter Bestand | Weiterhin offen |
| --- | --- | --- |
| Mathematik | Exakte große Ganzzahlen/Brüche, begrenzte Formelfamilien und Ausdrucksgrammatiken, Normalformen und rationale Definitionsbereichsvergleiche | Allgemeines mathematisches Problemlösen und freie Synthese |
| Grid | Lokale Adressen, einzelne Auswertungen, Stapel, begrenzte Fortsetzungen, Cache und feste X-/Y-Projektionen | Allgemeine Expansion, mehrdimensionale Projektion, BigInt im Grid/Z |
| Forschung | Eingebaute Strategierouten, Portfolios, Hold-out, Trace und Gegenrechnung | Dynamische Strategieports, universelle Beweise aus Stichproben |
| Gemeinsame Anwendung | Acht Laufvarianten, typisierte Bewertung, Gates und linearer geprüfter Sitzungscontroller | Vollständige gemeinsame Verwaltung aller neueren Aktionen und Phase-DAG |
| Experience | Vier Anwendungsaktionen, Quellenreplay, Stimmenrouting, Deduplizierung, Widerruf und Ersetzung | Allgemeines Lernen und vollständig unabhängige Persistenzbibliothek |
| Regeln | Einzeldateivorschlag/-export und aktiver lokaler numerischer Speicher mit Bestätigung, Basisprüfung und Journal | Globale Durchsetzung aller Regelwege; Journalmigration/Rotation und Ablehnungsprotokoll |
| Übergaben | Forschung → Erfahrung sowie Forschung → Z-Auswahl, gebundene Inhalte und erneute Prüfung | Allgemeine Übergabeketten und universelle gemeinsame Arbeitsbudgets |
| Betrieb | CLI, historische Replayfixtures, Installation und Paketprüfung | Cockpit, Sprache, NanoCell, GPU, weitere Zielplattformabnahmen |

## M0–M2 im lokalen Profil

Das [Abnahmeprofil](174-local-cli-acceptance-profile.md) benennt den überprüfbaren Umfang der vorhandenen Software. Die [Matrix](136-m0-m2-acceptance-matrix.md) verbindet jeden Bereich mit konkreten Tests und offenen Zielanforderungen. Das ursprüngliche M0–M2-Ziel wird nicht nachträglich auf den Implementierungsstand verkleinert.

Gemeinsame Prüfpunktbudgets und blockweise Kopier-/Hashpfade sind vorhanden. Sie garantieren weder ein Prozessspeicherlimit noch einheitliche mathematische Operationskosten oder maximale Signallatenz. Die Übergabe- und Verwaltungswege sind nicht pauschal an denselben Controller angeschlossen.

## Höhere Meilensteine

M3 (Aufgaben-/Sprachverständnis), M5 (NanoCell), M6 (Cockpit) und M7 (Phase-DAG) bleiben überwiegend Zielarchitektur. M4 besitzt begrenzte Erfahrungsplanung und explizite Korrekturen, keine allgemeine Lernarchitektur. Lokale Optimierungen aus M8 sind vorhanden; parallele/GPU-Ausführung folgt daraus nicht.

Der aktuelle [Prüfnachweis](acceptance/local-cli-core093-v1-2026-09-26.md) trennt neu ausgeführte Prüfungen von nicht abgedeckten Eigenschaften. Versionsnummer und Anzahl der Tests sind keine prozentuale Fertigstellungsangabe.
