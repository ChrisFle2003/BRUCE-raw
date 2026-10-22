# Tatsächliche Architektur und Meilensteine – Core 0.93

Stand: 16.10.2026. Maßgeblich sind der aktive lokale Quellbaum und der Nutzerpfad auf Bruce-AI. [Konsolidierte Modulübersicht](../architecture/03-blueprint-consolidation.md), [historische Übersicht Core 0.82](75-architecture-history-core082.md).

| Bereich | Implementierter Bestand | Weiterhin offen |
| --- | --- | --- |
| Mathematik | Exakte große Ganzzahlen/Brüche, begrenzte Formelfamilien und Ausdrucksgrammatiken, Normalformen und rationale Definitionsbereichsvergleiche | Allgemeines mathematisches Problemlösen und freie Synthese |
| Grid | Lokale Adressen, feste Blattregeln `x^y` und `100*(2^stage-1)`, einzelne Auswertungen, Stapel, begrenzte Fortsetzungen, Cache und feste X-/Y-Projektionen | Allgemeine Expansion, mehrdimensionale Projektion, BigInt im Grid/Z |
| Forschung | Eingebaute Strategierouten, Portfolios, Hold-out, Trace und Gegenrechnung | Dynamische Strategieports, universelle Beweise aus Stichproben |
| Gemeinsame Anwendung | Neun Laufvarianten einschließlich Grid Stage Sweep V1, typisierte Bewertung, Gates und linearer geprüfter Sitzungscontroller | Vollständige gemeinsame Verwaltung aller neueren Aktionen und Phase-DAG |
| Experience | Vier Anwendungsaktionen, Quellenreplay, Stimmenrouting, Deduplizierung, Widerruf und Ersetzung | Allgemeines Lernen und vollständig unabhängige Persistenzbibliothek |
| Regeln | Einzeldateivorschlag/-export und aktiver lokaler numerischer Speicher mit Bestätigung, Basisprüfung und Journal | Globale Durchsetzung aller Regelwege; Journalmigration/Rotation und Ablehnungsprotokoll |
| Übergaben | Forschung → Erfahrung sowie Forschung → Z-Auswahl, gebundene Inhalte und erneute Prüfung | Allgemeine Übergabeketten und universelle gemeinsame Arbeitsbudgets |
| Worker-/Runtime-Host | Additive Common Runtime Host V2-Fassade für lokale Worker-V1-Versuche; JSONL-Laufspur, Host-Abbruch, explizite Retries und metadata-only Ergebnisprojektion sind im V5-Einzelknotenpfad (`workers=1`) und im gewöhnlichen Mehrknotenpfad sowie lokalen Payload V13/V14 belegt | Automatische Retries, Stream-/Flow-/Handoff-Hostmigration, externe Payloadspeicher und ein allgemeiner Runtime-Controller |
| Betrieb | CLI, historische Replayfixtures, Installation und Paketprüfung | Cockpit, Sprache, NanoCell, GPU, weitere Zielplattformabnahmen |

## M0–M2 im lokalen Profil

**IMPLEMENTIERT UND BELEGT:** Scaled Exponential Grid
Rule V1 ergänzt `grid` und `grid-batch` um die geschlossene Option
`--rule leaf-local-power|scaled-exponential-v1`; Standard bleibt `x^y`.
Die zweite feste Regel berechnet `100*(2^stage-1)` ohne Stage-Option mit
Blatt-`y` als Stufe 0..7. Scaled Exponential Grid Stage V1 ergänzt ausschließlich
für diese Regel optional `--stage 0..57` bei `grid` und `grid-batch`;
ein Batch nutzt eine gemeinsame Stage. Nichtnumerische oder außerhalb des
Bereichs liegende Stufen ergeben `InvalidGridStage`, eine Stage mit Blattregel
ergibt `StageRequiresScaledExponentialRule`. Pfade bleiben lokale Adressen
mit Koordinaten 0..7. Exact ist
auf UInt64 und `max_bits` begrenzt, Metric liefert die exakte Bitlänge mit
internem 64-Bit-Budget, Modular den sicher berechneten Rest. Der Stop-Sentinel
bleibt unter beiden Regeln an die Adresse `(0,0)` gebunden. Cache, kanonisches
Batch-/Einzelreplay und Resume tragen die Regel und optionale Stage. Explizite
Stufen verwenden `grid-spec-v2`, Grid V2, unbegrenzten Batch V3, begrenzten
Batch V4 und Resume V2 mit kanonischen Stage-Feldern. Die Batch-Headerwahl
bezeichnet die konfigurierte Ausführungsgrenze; begrenzte Batches verwenden
auch bei `COMPLETED` V4, ohne explizite Stage entsprechend V2 statt V1.
Resume erbt seine Stage
ausschließlich aus replayter Elternevidenz und prüft die Übereinstimmung;
eine Stage-Option zum Überschreiben existiert dort nicht. Ohne Stage-Option
bleiben Einzel-V1, Batch-V1/V2 und Resume-V1 einschließlich Cache, JSON und
Trace bytekompatibel und historische skalierte Bundles replaybar. Stage 57
liefert 14411518807585587100, benötigt 64 Bits und wird in Exact mit 63 Bits
abgewiesen; Metric und Modular behalten ihre bestehenden Grenzen.
Die Skala 100 ist abstrakt, kein physikalischer Blasensimulator;
Koordinatenexpansion, freie Formeln/Stage-Ausdrücke, NanoCell-, Worker-, GPU-
und Netzwerkfunktionen werden nicht ergänzt. Belege:
[CLI-Vertrag](../cli/commands.md#grid--einen-pfad-auswerten),
[Grid-Verträge](../../tests/unit/grid_tests.cpp),
[Cache-/Batch-Verträge](../../tests/unit/grid_cache_tests.cpp) und
[CLI-Workflows](../../tests/integration/test_cli.py).

**IMPLEMENTIERT UND BELEGT:** Der eigenständige `grid-stage-sweep` führt die
feste skalierte Regel über ein inklusives zusammenhängendes Intervall `0..57`
aus. Lokale Adressen bleiben unverändert; mit festem `zero_zero=one` wird
auch `(0,0)` pro angeforderter Stage ausgewertet. Exact bewahrt bei seinem
ersten `IntegerBitLimit` das erfolgreiche Präfix als `INCOMPLETE`; bei
`0..57` mit 63 Bits sind dies die 57 Einträge 0..56 mit Abbruchstage 57.
Metric/Modular behalten ihre bestehenden Grid-Grenzen. Kompakte Records
tragen Stage, Index, Ergebnis, Cache-Digest/-Ereignis und Status; ausschließlich
`--debug` ergänzt die vollständigen Record-Traces, ohne Werte, Reihenfolge,
Cacheidentität oder Cacheabrechnung zu verändern. Ein frischer, bytebegrenzter
FIFO-GridCache gilt nur für diesen Lauf bzw. dessen Replay.

Der neue `BRUCE_GRID_STAGE_SWEEP_BUNDLE_V1` bindet Parameter, gemeinsamen
Spec, Records, Statistik und Endstatus. Der zentrale Anwendungsweg führt
Sweep aus, rendert/exportiert ihn, replayt den vollständigen kanonischen
Payload und stellt Bewertung (`grid-evaluation`) und Inspektion bereit.
Die bestehenden acht Varianten behalten ihre Indizes. Repräsentative alte
Grid-/Batch-/Resume-Ausgaben und Bundles wurden vor/nach CLI-Integration
bytegleich verglichen; daraus folgt keine erschöpfende historische
Kompatibilitätsprüfung. Sweep V1 ergänzt keine freien Regeln/Formeln,
Stage-Listen, Wiederholungen, Resume- oder Cachepersistenz und keine
physikalische, NanoCell-, Worker-, GPU- oder Netzwerkausführung.
Belege: [Sweep-Verträge](../../tests/unit/grid_stage_sweep_tests.cpp),
[CLI-Workflows](../../tests/integration/test_cli.py),
[Bedienung](../cli/commands.md#grid-stage-sweep--begrenzte-stufenreihe-mit-diagnose)
und [Bundlevertrag](../cli/formats.md#grid-stage-sweep-v1-evidenz).
Die Sweep-spezifischen Verträge stammen nicht aus dem unveränderten
CLI-Beispielgenerator. Die Gesamtsuite bleibt wegen der separat
protokollierten Sandbox-/Cockpit-Socketgrenze **IMPLEMENTIERT, ABER NICHT
AKTUELL VERIFIZIERT**; die begrenzten Sweep-/Grid-/CLI-/Anwendungsprüfungen
sind davon getrennt belegt.

Das [Abnahmeprofil](174-local-cli-acceptance-profile.md) benennt den überprüfbaren Umfang der vorhandenen Software. Die [Matrix](136-m0-m2-acceptance-matrix.md) verbindet jeden Bereich mit konkreten Tests und offenen Zielanforderungen. Das ursprüngliche M0–M2-Ziel wird nicht nachträglich auf den Implementierungsstand verkleinert.

Gemeinsame Prüfpunktbudgets und blockweise Kopier-/Hashpfade sind vorhanden. Sie garantieren weder ein Prozessspeicherlimit noch einheitliche mathematische Operationskosten oder maximale Signallatenz. Die Übergabe- und Verwaltungswege sind nicht pauschal an denselben Controller angeschlossen. Der Common Runtime Host V2 bindet den schmalen V5-Einzelknotenpfad und den gewöhnlichen V5-Mehrknotenpfad (einschließlich expliziter Retries; gewöhnliche Einzelknoten mit `--workers 1`) sowie die lokale Payload-V13/V14-Route (ein/mehrere Knoten, `--workers 1..4`, lokale externe Eingaben) an die gemeinsame Worker-Grenze; die übrigen V5-Pfade behalten ihre eigenen Implementierungen.

## Höhere Meilensteine

M3 (Aufgaben-/Sprachverständnis), M5 (NanoCell), M6 (Cockpit) und M7 (Phase-DAG) bleiben überwiegend Zielarchitektur. M4 besitzt begrenzte Erfahrungsplanung und explizite Korrekturen, keine allgemeine Lernarchitektur. Lokale Optimierungen aus M8 sind vorhanden; parallele/GPU-Ausführung folgt daraus nicht.

**IMPLEMENTIERT UND BELEGT:** Stage 3A ergänzt den Common Runtime Host V2
als additive Ein-Worker-Schicht für den beschriebenen V5-Einzelknotenpfad.
Stage 3B erweitert denselben Host für gewöhnliche V5-Mehrknotenläufe mit
`--retry 0` und ohne Payload-, Stream-, Flow- oder Handoff-Erweiterung. Der
Batch-Host teilt sich einen JSONL-Lauf, begrenzt aktive Worker über
`--workers`, steuert Cancel zentral und reapt alle Worker und Watchdogs vor
`RUN_FINISHED`; der bestehende Scheduler behält Dependency-Gating,
Receipt-Reihenfolge, Startbudget und `BLOCKED`-Fortpflanzung. Worker V1 Frames,
die öffentliche V5-Syntax und der metadata-only Bericht bleiben unverändert.
Die Nachweise umfassen Host-V2- und Batch-Verträge, V5-CLI-, Abbruch-,
Inspector-, Payload-, Flow- und Handoff-Workflows sowie die reproduzierbaren
Stage-3A-, Stage-3B- und Stage-3C-Integrationen.

**IMPLEMENTIERT UND BELEGT:** Stage 3C bindet explizite Retries (`--retry 1..3`)
für gewöhnliche lokale Einzelknotenläufe mit `--workers 1` und Mehrknotenläufe
mit `--workers 1..4` an den Batch-Host. Nur `PROCESS_CRASHED`, `TIMEOUT` und
`PROTOCOL_ERROR` sind wiederholbar; höchstens `N+1` Attempts behalten eigene
IDs, Verzeichnisse und Historieneinträge. Retries verbrauchen keinen weiteren
Node-Startplatz. `record_attempt_retry_v2` trennt historische Fehler von der
finalen Node-Entscheidung, sodass ein späterer Erfolg READY ergeben kann.
Cancel entscheidet auch bereits projizierte, noch nicht angenommene Ergebnisse
und den letzten Attempt zwischen Retries; IDs und Usage bleiben erhalten.
Der Inspector unterstützt dafür eine einmalige Post-Result-Cancelentscheidung
sowie `RESULT_REJECTED/OUTPUT_LIMIT_EXCEEDED` ausschließlich zur Ablehnung
eines kanonischen READY wegen des Receipt-Budgets. Das ergänzt den privaten
JSONL-Lifecycle; Worker-V1-Frames und öffentliche V5-Syntax/-Bericht bleiben
unverändert. Belege: [Stage-3C-Integration](../../tests/integration/test_common_runtime_host_v2_stage3c.py),
[Batch-Verträge](../../tests/unit/common_runtime_host_v2_batch_tests.cpp),
[Inspector-Verträge](../../tests/unit/worker_run_inspector_v1_tests.cpp).
Automatische Retries,
externe Payloadspeicher, freie Plugins,
GPU-/Netzwerkausführung, allgemeines Beweisen und übergreifende
Runtime-Orchestrierung bleiben **DOKUMENTIERT / KONZEPT**. :codex-annotation{index="1"}

**IMPLEMENTIERT UND BELEGT:** Stage 3D führt lokale Payload-V13/V14-Läufe
über denselben Batch-Host; öffentliche CLI/Berichte, Worker-V1-Frames und
Artifact Store V1 bleiben kompatibel. Der Scheduler behält Provenienz,
Materialisierung, Receipt-Reihenfolge und Veröffentlichung; der Host führt
Worker-Lifecycle, Cancel und Reaping. Die absolute Attempt-Deadline beginnt
vor der Materialisierung und wird bis zur Annahme geprüft. Synchrone
Store-Aufrufe werden nicht hart unterbrochen. Vorbereitungsfehler können ohne
erfundenen Attempt mit FAILED enden. Sechs enge `RESULT_REJECTED`-Gründe
bilden Receipt-, Timeout-, Referenz-, Manifest-, Digest- und Storefehler ab;
TIMEOUT erlaubt nur mit explizitem Retrybudget einen neuen Attempt. Cancel
schließt die Attempt-Serie, ungültige Kontrollmarker bleiben Protokollfehler.
Nach Veröffentlichung abgelehnte oder abgebrochene unveränderliche
Store-Einträge werden nicht automatisch zurückgerollt. Stream V15, Flow V2
und Handoff V1 behalten ihre bisherigen Pfade. Belege:
[Stage-3D-Integration](../../tests/integration/test_common_runtime_host_v2_stage3d.py),
[Batch-Host](../../src/application/common_runtime_host_v2_batch.cpp),
[Payload-Inspector](../../src/artifact/payload_run_inspector_v1.cpp) und
[Stage-3D-Vertrag](../superpowers/specs/2026-10-16-common-runtime-host-v2-stage3d-payload-host-design.md).

Der aktuelle [Prüfnachweis](acceptance/local-cli-core093-v1-2026-09-26.md) trennt neu ausgeführte Prüfungen von nicht abgedeckten Eigenschaften. Versionsnummer und Anzahl der Tests sind keine prozentuale Fertigstellungsangabe.
