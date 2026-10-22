# Payload Run Inspector V1 — Design

**Status: VORSCHLAG — zur Prüfung, nicht implementiert.**

## Ziel und Arbeitsauftrag

Ein Benutzer soll einen vorhandenen lokalen V5-Payload-Lauf prüfen können,
ohne Worker zu starten, einen Adapter auszuführen, Daten erneut zu publizieren
oder Dateien zu reparieren. Die Prüfung umfasst den gespeicherten öffentlichen
Report, den privaten Run-Log, Request-Dateien und referenzierte lokale Outputs.
Eine ausdrücklich gewählte Zusatzprüfung untersucht erhaltene V1.5-Streams.

Diese Spec beschreibt einen neuen Baustein; die nachstehenden Anforderungen
sind keine Behauptung über bereits vorhandene Funktionen. Der gewünschte
Ausführungsmodus des Benutzers bleibt Subagent-driven Development.

## Ausgangslage und Belege

- [V5 CLI](../../../apps/dag_scheduler_v5_cli.hpp): `inspect_execute_report()`
  akzeptiert ausschließlich `dag-scheduler-v5-cli-v1`/`execute`.
- `single_report()`, `multi_report()` und `execute_payload()` in derselben Datei
  erzeugen `dag-scheduler-v5-payload-v13` beziehungsweise `...-v14`/`payload`.
  V1.4 fügt `external_inputs_digest`, `external_input_count` und
  `external_input_bytes` hinzu. V1.5 ändert diese öffentlichen Formate nicht.
- [Worker-Inspector](../../../src/worker/worker_run_inspector_v1.cpp) prüft
  Logzustände und Output-Dateien. Er lehnt einen abgeschlossenen Lauf mit
  `RESULT_RECEIVED READY` ohne `ARTIFACT_ACCEPTED` ab. Payload-Publikationsfehler
  können diesen Zustand regulär erzeugen: der Host setzt Receipt und History
  danach auf `FAILED`; das Worker-Ergebnis bleibt im Log `READY`.
- Payload-Preflight kann eine `attempt_id` reservieren, bevor `ATTEMPT_STARTED`
  geschrieben wird. Ein solcher fehlgeschlagener Versuch hat keinen neuen
  History-Eintrag und keine gestartete Worker-Instanz.
- [Blueprint](../../architecture/03-blueprint-consolidation.md), Abschnitt V1.5:
  Streams liegen in `payload-inputs/<attempt-id>/input-<index>/stream-payload/`;
  Worker erhalten die separate reguläre Datei `input.bin`.
- [V1.5 Spec](2026-10-15-artifact-payload-stream-v1-design.md) und
  [V1.5 Plan](../plans/2026-10-15-artifact-payload-stream-v1.md) begrenzen den
  Streamvertrag. Die abschließende Re-Review unter
  `.superpowers/sdd/2026-10-15-artifact-payload-stream-v1/final-rereview.md`
  belegt die Deadline-Korrektur; sie belegt keine neue Inspector-Funktion.

**Wesentliche Evidenzgrenze:** Es gibt keinen dauerhaft gespeicherten,
authentifizierten V1.5-Modusmarker und keinen vollständigen gespeicherten Index
der WorkerRequest-Payload-Eingaben. Der ursprüngliche DAG, Store-Provenienz und
der V1.4-Sidecar sind durch Digestfelder allein nicht rekonstruierbar. Der
Inspector darf daher weder historische Streamverwendung noch Vollständigkeit,
Reihenfolge oder ursprüngliche Herkunft aller Eingaben bestätigen.

## Gewählte Schnittstelle

```text
bruce dag-scheduler-v5 payload-inspect BUNDLE [--stream-v15]
```

`BUNDLE` ist der Report, nicht das `.run`-Verzeichnis. Exakt ein optionales,
wertloses `--stream-v15` nach BUNDLE; doppelte, unbekannte, falsch platzierte
Optionen und weitere Argumente liefern Exit 2. Keine Store-, Output-, Retry-,
Repair- oder Resume-Option. `inspect`, `execute`, `payload`, `publish` und
Worker-v1 behalten ihre bisherigen Verträge und Ausgabeformate.

Die neue Route akzeptiert ausschließlich kanonische V1.3-/V1.4-Payload-Reports
beider vorhandenen Formen (ein Node beziehungsweise Receipts für mehrere
Nodes). Ein Execute-Report ist hier ein Protokollfehler. Der alte Inspector
akzeptiert dadurch keine Payload-Reports.

`--stream-v15` ist eine **Prüfanforderung**, kein rückwirkender Modusnachweis.
Ohne Option werden private Payload-Eingaben nicht durchsucht oder bewertet.
Mit Option wird das vorhandene V1.5-Layout streng geprüft; reguläre V1.3-Dateien
an dessen Stelle sind kein Stream und führen zu `PROTOCOL_ERROR`.

## Architektur und Verantwortungen

Ein eigener hostseitiger `PayloadRunInspectorV1` liest Report und Run-Artefakte.
Er besitzt Payload-spezifische Zustandsregeln. Der vorhandene Execute-Inspector
bleibt unverändert. Sichere Leseprimitive und die bestehende Stream-Verifikation
können verwendet werden, ihre Execute-Zustandsmaschine darf nicht durch
Umbenennen des Reportvertrags wiederverwendet werden.

```text
payload-inspect
  -> sichere Report-/Run-Öffnung
  -> kanonische Report- und JSONL-Prüfung
  -> Payload-spezifischer Abgleich von Nodes, Attempts, Ergebnissen
  -> Request-/Output-Digestprüfung
  -> optional: private Stream-/Kompatibilitätsprüfung
  -> begrenzter, deterministischer Metadatenreport
```

Kein Aufruf von `start_attempt`, `execute_payload`, Store-Publikation,
Materialisierung oder Streamwriter. Kein Replay und kein externer Dienst.

## Report, Log und Zustände

Report- und Logparser erzwingen vorhandene kanonische Schlüsselreihenfolge,
exakte Schlüsselmenge, Typen, Zahlenbereiche, Identifier und Digests. Doppelte
und unbekannte Schlüssel, nachgestellte Daten, Trunkierung, ungültige UTF-8-
oder Escape-Darstellungen außerhalb des bestehenden Formats und JSON-Tiefe
über 64 sind Fehler. Bestehende Budgetmaxima, Retry 0..3, Workers 1..4 und
höchstens 15 Payload-Objekte pro Node werden nicht angehoben.

Der neue Parser prüft alle Logregeln des bisherigen Inspectors einschließlich
Run-ID, Sequenzen, Ereignisreihenfolge, eindeutigen Attempts, Retry-Familien,
Outputreferenzen und Digest/Größe. Folgende Payload-Regeln kommen ausdrücklich
hinzu; sie lockern den alten Inspector nicht:

| Situation | Erforderliche Nachweise | Bewertung |
|---|---|---|
| Akzeptierter Output | READY-Ergebnis, ARTIFACT_ACCEPTED, passende Receipt/History, lokale Outputbytes | regulär |
| Preflightfehler | reservierte nächste Attempt-ID gehört zum Node, kein Logattempt dieser ID, FAILED/FAILED, erlaubter Payload-Fehlercode, kein Output und kein neuer Historyeintrag | regulär |
| Publikationsfehler | existierender gestarteter Attempt, RESULT_RECEIVED READY mit überprüfbarem lokalem Output, kein ARTIFACT_ACCEPTED; letzte History/Receipt FAILED/FAILED mit Payload-Fehlercode, keine öffentlichen Outputmetadaten | regulär |
| Nicht gestarteter Node | BLOCKED/BLOCKED/BLOCKED oder FAILED/FAILED/START_BUDGET_EXCEEDED, keine Attempt-ID, keine History, kein Output | regulär |
| Ausführung fehlgeschlagen | vorhandener Worker-Endstatus und passende Receipt/History einschließlich Nutzung | regulär |
| READY ohne Acceptance und ohne belegten Hostfehler | widersprüchlich | PROTOCOL_ERROR |

Der erlaubte Preflight-Codekatalog ist exakt `ARTIFACT_REFERENCE_INVALID`,
`ARTIFACT_MANIFEST_INVALID`, `ARTIFACT_DIGEST_MISMATCH`,
`ARTIFACT_STORE_VIOLATION`, `INPUT_LIMIT_EXCEEDED`, `TIMEOUT`,
`EXTERNAL_MANIFEST_INVALID`, `EXTERNAL_OBJECT_INVALID`,
`EXTERNAL_INPUT_LIMIT_EXCEEDED`, `EXTERNAL_INPUT_TIMEOUT`.
Publikationsfehler benutzen nur die ersten sechs Codes.
Externe Codes sind nur bei V1.4 zulässig. Freie `EXTERNAL_*`-Präfixakzeptanz
ist unzulässig. Preflight nach früheren gescheiterten Retry-Attempts erhält
deren verifizierte History; nur die neu reservierte ID fehlt im Log.

Bei Publikationsfehlern kann die abschließende Receipt-Nutzung null sein,
während das Worker-Log die vorherige Nutzung enthält; dies ist eine ausdrücklich
modellierte Hostentscheidung und keine allgemeine Erlaubnis für Abweichungen.
Der Inspector bestätigt die interne Konsistenz dieser Erklärung, nicht den
externen Grund des Storefehlers. V1.4-Count/Byte-Felder werden auf Typ, Grenze,
Überlauf und vorhandene Konsistenzbeziehungen geprüft. Der Sidecar-Digest ist
syntaktisch prüfbar, ohne Sidecar nicht inhaltlich verifizierbar.

Für jeden gespeicherten Request wird der Digest geprüft; jeder im Log genannte
Output wird auch ohne Acceptance auf Größe/Digest geprüft. Alle gestarteten
Logattempts müssen genau einem Report-Node zugeordnet sein. Der finale Runstatus
muss bei `RUN_FINISHED` mit den Receipts übereinstimmen. Ohne `RUN_FINISHED`
dürfen nur strukturell konsistente, vollständig lesbare Daten `INCOMPLETE`
ergeben; abgeschnittenes JSONL wird nicht als harmloser Zwischenstand gewertet.
Ein fehlender Bundle-Report ist `PROTOCOL_ERROR`, keine Rekonstruktion aus Logs.

## Private Streamprüfung

Die ausdrückliche Option öffnet ausschließlich `payload-inputs` unter dem
geprüften Runroot. Ein fehlendes oder leeres Verzeichnis ergibt null geprüfte
Streams und keinen Vollständigkeitsnachweis. Vorhandene Einträge müssen zum
festen Layout passen, auf bekannte gestartete Attempt-IDs verweisen und pro
Attempt lückenlose `input-0` bis `input-N` mit N < 15 bilden. Reservierte,
ungestartete Preflight-IDs dürfen keine erhaltenen Unterbäume besitzen.

Jedes Inputverzeichnis enthält exakt `stream-payload` und `input.bin`.
Temporäre Quellen, Stagingreste, fremde Einträge und gemischte Legacy-Dateien
werden abgelehnt und bleiben unangetastet. Pfadnamen bestimmen nur die erlaubte
Struktur. **Sie belegen niemals Streamgültigkeit:** der Inspector liest
kanonischen Header/Seal, konstruiert daraus `ArtifactPayloadStreamV15`, verlangt
`stream_id=payload`, `chunk_bytes=65536`, korrekte Anzahl/Größe und authentifiziert
die tatsächlichen Chunks über `verify_payload_stream_v15`. Anschließend prüft
er die reguläre Kompatibilitätsdatei auf dieselbe Größe und denselben SHA-256.

Unvollständige Streamstrukturen sind `PROTOCOL_ERROR`, auch bei unvollständigem
Run. Ein kompletter, konsistenter Stream ist nur ein heute überprüfter lokaler
Stream; ein nachträglich eingefügter konsistenter Stream ist ohne historisches
Inputmanifest nicht als solcher erkennbar. Das vollständige Entfernen eines
Inputbaums oder des letzten Inputs ist ebenfalls nicht zuverlässig nachweisbar.
Der Bericht darf diese Evidenzlücke nicht mit `VALID` verdecken.

## Sicherheits-, Ressourcen- und Mutationsgrenze

- Descriptor-relative `O_NOFOLLOW|O_CLOEXEC`-Öffnung durch alle Pfadkomponenten;
  keine Symlink-, Hardlink-, FIFO-, Device- oder Socket-Payloads.
- Private Verzeichnisse `0700`, Stream-/Kompatibilitätsdateien `0600`, aktueller
  Eigentümer und einzelne Dateilinks. Report/Log/Request/Output behalten ihre
  bestehenden Private-File-Regeln.
- Report/Log und Dateizahlen bleiben durch `bruce::max_file_bytes` und die
  Report-/Attempt-/Inputgrenzen beschränkt; kein rekursiver Scan außerhalb des
  genannten Layouts. Vor Allokation und Summierung Grenzen/Überlauf prüfen.
- Chunks und reguläre Payloaddateien mit festem höchstens 65536-Byte-Puffer
  lesen; keine payloadgroße Allokation. Inputgröße höchstens 1048576 pro Objekt
  und insgesamt höchstens das Run-Inputbudget pro Attempt.
- Neue Inspector-Leseoperationen haben eine feste monotone Prüffrist von
  30000 ms ab Einstieg; jeder neue Open/Read/Hash-Abschnitt prüft sie. Ablauf
  liefert `PROTOCOL_ERROR`. Keine neue CLI-Budgetoption. Bereits laufende
  Kerneloperationen lassen sich dadurch nicht unterbrechen.
- Geöffnete Dateien vor/nach Lesen auf Identität, Größe, Mode, Links, mtime/ctime
  prüfen; Verzeichniszuordnung und Mitgliedschaft vor/nach Prüfung vergleichen;
  Report/Log am Ende erneut gegen die gelesenen Bytes prüfen. Erkannte Mutation
  führt zu `PROTOCOL_ERROR`, ohne automatische Wiederholung.
- Keine Schreiboperation, kein chmod, kein fsync, keine Locks/Cache/Tempdateien
  im Run oder Store. Fehler-Cleanup schließt ausschließlich eigene Deskriptoren
  und gibt Speicher frei. Zugriff kann systembedingt atime verändern; kein
  unveränderlicher Dateisystemsnapshot und kein Schutz gegen beliebige spätere
  Schreibzugriffe desselben Benutzers wird behauptet.

## Öffentlicher Ausgabe- und Exitvertrag

Erfolgreiche Prüfung schreibt genau eine kompakte JSON-Zeile auf stdout,
stderr bleibt leer. Feste Schlüsselreihenfolge:

```json
{"contract":"dag-scheduler-v5-payload-inspect-v1","action":"payload-inspect","status":"VALID","run_finished":true,"run_status":"READY","source_contract":"dag-scheduler-v5-payload-v13","run_id":"example","last_seq":7,"node_count":1,"attempt_count":1,"accepted_artifact_count":1,"stream_check":"NOT_REQUESTED","verified_stream_count":0,"verified_stream_bytes":0,"input_completeness":"NOT_PROVABLE","external_provenance":"NOT_CHECKED"}
```

`stream_check` ist `NOT_REQUESTED` oder `VERIFIED_PRESENT`. Letzteres bedeutet
auch bei Anzahl null ausschließlich Prüfung der vorhandenen Streamstruktur.
Öffentliche Digests, private Pfade, Streamnamen, Header-/Seal-Inhalte,
Payloadbytes/Hex, Manifest-IDs, PIDs und OS-Diagnosen werden hier nicht benötigt
und nicht ausgegeben. Feste Zähler werden overflow-sicher berechnet.

| Zustand | stdout | stderr | Exit |
|---|---|---|---:|
| vollständiger konsistenter Lauf, auch FAILED | Report status VALID | leer | 0 |
| konsistenter Lauf ohne RUN_FINISHED | Report status INCOMPLETE, run_status INCOMPLETE | leer | 1 |
| Syntax/Integrität/Dateisicherheit/Mutation/Prüffrist verletzt | leer | `{"error":"PROTOCOL_ERROR"}` plus LF | 2 |

`VALID` bedeutet Konsistenz der genannten gespeicherten Belege, nicht erfolgreicher
Workflow, Store-Verfügbarkeit, DAG-Replay oder Vollständigkeit historischer Inputs.

## Abnahme und Nichtziele

Reale V1.3-/V1.4-Läufe, beide Reportformen, Streamoption, Fehler vor Workerstart,
Publikationsfehler, Retry, BLOCKED und fehlendes RUN_FINISHED werden geprüft.
Manipulation jeder Nachweisgruppe, path safety, große/tiefe Daten, Mutationen,
Prüffrist und Leckfreiheit erhalten permanente Tests. Vor/nach jeder Inspektion
werden Inhalt, Mitgliedschaft, Mode, Links und mtime verglichen; atime wird
nicht als Schreibnachweis verwendet. Alte Execute-/Inspect-/Publish- und
Payload-Kompatibilität wird mit bestehenden Tests und Ausgabevergleich belegt.

Keine Produktionsänderung ist Teil dieses Planungsauftrags. Nichtziele bleiben
Store-Resolve/Download, persistierter Inputindex, Recovery, Resume, Repair,
Streamproduktion, Workerframeänderung, Netzwerk/GPU/Plugins und allgemeine
Runtime-Orchestrierung. Ein künftiger authentifizierter Inputindex benötigt
einen eigenen Vertrag und darf nicht stillschweigend in diese Prüfung geraten.
