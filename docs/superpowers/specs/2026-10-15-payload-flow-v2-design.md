# Payload Flow V2 — Design

**Status: IMPLEMENTIERT UND BELEGT — additive Opt-in-Integration mit
kanonischem Sidecar, Host-Lineage und read-only Flow-V2-Inspector;
fokussierte Abnahme am 2026-10-15.**

## Implementierungs- und Prüfbelege

- Kernmodell und kanonischer Codec:
  [Header](../../../include/bruce/artifact/payload_flow_v2.hpp) und
  [Implementierung](../../../src/artifact/payload_flow_v2.cpp).
  Die gemeinsame Manifest-ID-Ableitung steht in
  [v5_manifest_id_v1.hpp](../../../include/bruce/artifact/v5_manifest_id_v1.hpp)
  und [v5_manifest_id_v1.cpp](../../../src/artifact/v5_manifest_id_v1.cpp).
- Hostaufzeichnung, atomare Sidecar-Adoption und CLI-Routen:
  [V5 CLI](../../../apps/dag_scheduler_v5_cli.hpp).
- Read-only Inspector:
  [Header](../../../include/bruce/artifact/payload_flow_inspector_v2.hpp) und
  [Implementierung](../../../src/artifact/payload_flow_inspector_v2.cpp).
- Verträge und reale Workflows:
  [Codec-Tests](../../../tests/unit/payload_flow_v2_tests.cpp),
  [CLI-Tests](../../../tests/unit/dag_scheduler_v5_payload_cli_tests.cpp),
  [Inspector-Tests](../../../tests/unit/payload_flow_inspector_v2_tests.cpp),
  [Payload-Workflows](../../../tests/integration/test_dag_scheduler_v5_payload.py)
  und [Inspector-Workflows](../../../tests/integration/test_dag_scheduler_v5_payload_flow_inspector.py).
  Quellen und Testziele sind in [CMake](../../../CMakeLists.txt) registriert.
- Der abschließende Laufbericht (archivaler lokaler, nicht versionierter Pfad:
  `runs/bruce-payload-flow-v2-2026-10-15.md`)
  belegt den nativen Build, **5/5 fokussierte CTests**, **29/29 CLI-Dokumentationstests**,
  lokale Links und die CLI-Generatorprüfung in einer temporären Kopie.
  Die finale Inspector-Re-Review (archivaler lokaler, nicht versionierter Pfad:
  `runs/payload-flow-v2-task3-review.md`)
  ist PASS und belegt zusätzlich **13/13 direkte Inspector-Workflows**.

Dies ist keine Behauptung einer vollständig grünen Gesamtsuite: Der
Laufbericht hält **192/193** bestandene zentrale CTest-Ziele fest;
`cockpit_inspector_workflows` scheiterte an einer lokalen
`PermissionError`-Socketbindung. Der Host-Dependencies-Wrapper war wegen des
malformierten `PkgConfig::GMPXX`-Pfads nicht als zusätzlicher Abnahmepfad nutzbar.
Git-Diff-/Statusaussagen waren wegen fehlender Git-Metadaten nicht möglich.
Maßgeblich für diese Abnahme sind der native `/tmp/bruce-v14-core-build` und
die verlinkten fokussierten Prüfungen.

## Ziel und belegte Ausgangslage

V2 macht die bereits vorhandene direkte Payloadübergabe dauerhaft nachvollziehbar:
welcher akzeptierte Producer-Attempt lieferte welches Objekt an welchen Input
welcher Consumer-Attempt dieses Input konsumierte. Ein eigener read-only Inspector prüft diese Bindungen
gegen DAG-Metadaten, Legacy-Report, Workerlog und erhaltene lokale Payloaddateien.

**IMPLEMENTIERT UND BELEGT:** Der aktuelle Quellcode enthält
bereits echten Payloadfluss. `prepare_payload_inputs()` und
`publish_payload_output()` in [V5 CLI](../../../apps/dag_scheduler_v5_cli.hpp)
verbinden direkte Vorgänger über verifizierte Storeobjekte mit Workerinputs.
`test_stream_two_node_success_and_legacy_report_bytes` in
[Workflowtests](../../../tests/integration/test_dag_scheduler_v5_payload.py)
prüft die tatsächlichen Consumerbytes `payload + b"!"`. Phase-DAG V2/V3 und
DAG-Scheduler V2/V3/V4 besitzen außerdem ihren eigenen Bytefluss. V2 baut diesen
Flow nicht erneut und bedeutet nicht Worker-v2 oder DAG-Scheduler-v2.

**IMPLEMENTIERT UND BELEGT:** Der direkte In-Memory-Payloadfluss des
DAG-Schedulers V2 ist in
`include/bruce/dag_scheduler/dag_scheduler_v2.hpp`,
`src/dag_scheduler/dag_scheduler_v2.cpp` und den zugehörigen Unit-/CLI-Tests
vorhanden. Er bleibt ein separater, lokaler synchroner Vertrag. Diese Phase
führt keine neue V2-Scheduler-API ein, sondern bindet den vorhandenen
hostgesteuerten V1.3-Storefluss mit einer persistierten Lineage-Evidenz an.

Das historische [V1.3-Design](2026-10-15-artifact-payload-flow-v1-design.md)
bezeichnete seine Funktion damals als nicht implementiert. Der heutige Code und
die reproduzierbaren Payload-Workflows sind für die aktuelle Einordnung
maßgeblich. Der bestehende
[Payload-Inspector V1](2026-10-15-payload-run-inspector-v1-design.md) kann ohne
Inputindex weder Vollständigkeit noch ursprüngliche Reihenfolge bestätigen.
Genau diese lokale Evidenzlücke adressiert V2.

## Architekturentscheidung für diese Phase

Diese Phase implementiert keinen neuen Byte-Transport. Der direkte Payloadfluss
des DAG-Schedulers V2 und der portable Phase-DAG-V3-Payloadvertrag bleiben
unverändert. Der in dieser Phase implementierte Baustein ist ausschließlich
die persistierte `PayloadFlowSnapshotV2`-Evidenz für den bereits vorhandenen V5-Worker-/Storefluss:

```text
bestehender Producer-/Consumer-Payloadfluss
        |
        v
kanonische Producer-Input-Bindungen
        |
        v
atomarer flow-v2-Sidecar
        |
        v
read-only Lineage-Inspector
```

Der Erfolg dieser Phase ist erreicht, wenn ein realer Zwei-Knoten-Lauf seine
direkten Inputbindungen, Attempt-Reihenfolge, Manifest-IDs, Digests und Größen
in einem bounded lesbaren Sidecar ablegt und der Inspector denselben Lauf mit
Exit 0 als `LOCAL_CONSISTENCY` bestätigt. Ohne Sidecar bleibt der bestehende
V1.3-Run unverändert nutzbar; ohne Original-Spec oder externe Signatur wird
keine historische Authentizität behauptet.

## Schnittstellen und Kompatibilität

```text
bruce dag-scheduler-v5 payload SPEC --output BUNDLE --store STORE --flow-v2 [--stream-v15] [bestehende Budget-/Retry-/Workers-Optionen]
bruce dag-scheduler-v5 payload-inspect BUNDLE --flow-v2
```

- `--flow-v2` ist wertlos, einmalig und nur in diesen beiden Routen erlaubt.
- V2 umfasst zunächst direkte Kanten desselben Runs. `--external-inputs` mit
  `--flow-v2` wird vor Storezugriff/Dateianlage abgewiesen, auch für leere Sidecars.
- Bei `payload` bleibt `--stream-v15` optional. Der V2-Sidecar speichert den Modus.
  Bei `payload-inspect --flow-v2` wird der gespeicherte Modus zwingend geprüft;
  eine zusätzliche `--stream-v15`-Option ist hier ungültig.
- Die Konfliktprüfung ist eine reine Argumentprüfung. Nach dem syntaktischen
  CLI-Parse und **vor jedem Datei-Read oder jeder Datei-/Runroot-Anlage** — also
  vor `read_external_inputs_v14()`, `parse_spec()`, Store-Öffnung und
  `execute()` — wird `--external-inputs` zusammen mit `--flow-v2` abgewiesen.
- Ohne `--flow-v2` bleiben alle bestehenden Routen, Dateien und Reportbytes gleich.
- Worker-v1-Frame, WorkerRequest, Supervisor, V1.3/V1.4 und Stream-V1.5 werden
  nicht erweitert. Der Worker sieht weiter reguläre private Eingabedateien.
- Phase-DAG, Scheduler V2/V3/V4, Storeobjekt-/Manifestformate bleiben unverändert.

## Hostverantwortung und Ablauf

Bruce validiert zuerst die bestehende V3-Spec und Callback-Arity. Für jeden
Consumer-Attempt bildet er in der bestehenden `dependencies`-Reihenfolge exakt
eine Bindung pro direktem Vorgänger. Die Bindung stammt aus dem akzeptierten
Producerzustand und dem geprüften READY-Manifest, niemals aus freien Nutzerpfaden.
`object_index` ist in dieser Version immer 0.

```text
Producerresultat prüfen -> Storeobjekt/READY-Manifest publizieren
-> Producer akzeptieren -> direkte Consumer werden bereit
-> neue Consumer-Attempt-ID -> Ref/Manifest/Run/Node/Attempt prüfen
-> Inputs materialisieren, optional V1.5-Stream plus input.bin
-> vollständige geordnete Bindungen und private Dateien verifizieren
-> ATTEMPT_STARTED -> unveränderter Worker-v1
-> Ergebnis prüfen/publizieren -> nächste Kante
-> Legacy-BUNDLE abschließen -> V2-Sidecar atomar veröffentlichen
```

Ein Producer wird nur nach erfolgreicher Publikation akzeptiert. Ein Consumer
startet erst nach vollständigem Preflight aller direkten Inputs. Fehlgeschlagene
Preflightversuche besitzen keine angenommenen Inputbindungen; ihre neue
reservierte Attempt-ID darf neben älteren gestarteten Retry-Attempts existieren.
Vor dem Workerstart entsteht aus Dependency-Reihenfolge, Producer-Node/-Attempt,
Manifest, Objektindex, Digest und Größe ein unveränderlicher Lineage-Satz. Er
wird erst dann als gestarteter Attempt übernommen, wenn `ATTEMPT_STARTED`
erfolgreich im Log steht, und danach nie durch den aktuellen Zustand oder einen
späteren Retry überschrieben. Jeder tatsächlich gestartete Attempt, auch ein
später fehlgeschlagener Retry, erhält so einen eigenen vollständigen
Bindungssatz. Nur der akzeptierte
Producer-Attempt kann Quelle einer Kante sein. Retry bleibt ausschließlich über
`--retry N` erlaubt, mit bestehenden retryfähigen Fehlerklassen und neuer ID.

## Persistierter V2-Vertrag

Die bestehenden Dateien `BUNDLE` und `BUNDLE.run/` bleiben im V1.3-Format.
Zusätzlich entsteht ausschließlich im V2-Modus `BUNDLE.flow-v2.json` als private
reguläre Datei mit Mode 0600. Der Sidecar wird nach Fertigstellung des Legacy-
Reports atomar ohne Überschreiben veröffentlicht. Existiert eines der drei
Ausgabeziele bereits, wird vor dem Run abgebrochen. Die drei Ziele sind keine
Dateisystemtransaktion; ein Crash kann Legacy-Artefakte ohne V2-Sidecar hinterlassen.
Es gibt keine Wiederaufnahme oder automatische Reparatur.

Der Sidecar enthält eine kompakte kanonische JSON-Zeile mit abschließendem LF.
Exakte Schlüsselfolge:

```text
contract="bruce-payload-flow-v2"
run_id
spec_digest
report_digest                 SHA-256 der exakten Legacy-BUNDLE-Bytes
stream_v15                    boolean
nodes[]                       ursprüngliche Spec-Node-Reihenfolge
  node_id, callback_id, request_digest, dependencies[]
attempts[]                    ATTEMPT_STARTED-Reihenfolge im Workerlog
  consumer_node_id, consumer_attempt_id, inputs[]
    input_index               0..N-1; Request ist nicht enthalten
    producer_node_id
    producer_attempt_id
    manifest_id
    object_index              immer 0
    sha256
    size_bytes
```

Alle Knoten einschließlich BLOCKED-Nodes sind enthalten. Alle tatsächlich
begonnenen Attempts einschließlich Rootnodes mit `inputs:[]` sind enthalten.
Reservierte, nicht gestartete Preflight-IDs sind nicht in `attempts` enthalten.
`run_id` gilt für Consumer und Producer; Cross-Run-Inputs sind ausgeschlossen.
Die unmittelbare Kantenbindung ergibt sich aus `nodes.dependencies`, Index und
Producer-ID; die Spec-Digestbindung allein ersetzt diese Angaben nicht.

`spec_digest` ist exakt `sha256(parsed.raw_bytes)`, also der bereits vom
V5-Parser verwendete Digest der eingelesenen Specbytes. Der Inspector prüft
damit ausschließlich die **interne Konsistenz** der gespeicherten Report-,
Node- und Lineage-Metadaten. Ohne erneute Original-Spec oder externe Signatur
behauptet der Wert weder historische Originalität noch Authentizität. Eine neue
kanonische Spec-Serialisierung wird in V2 nicht eingeführt.

Die Knoten-/Callback-/Attempt-/Manifest-/Digestgrammatiken entsprechen exakt den
bestehenden validierten Typen. Unbekannte/doppelte Schlüssel, falsche Reihenfolge,
abweichende Zahlendarstellungen, Trunkierung, trailing bytes und JSON-Tiefe über
64 werden abgewiesen. Der Sidecar ist höchstens `bruce::max_file_bytes` groß;
Zahlen/Container werden vor Allokation und Summierung begrenzt. Pro Attempt sind
höchstens 15 Payloadinputs erlaubt; die Zahl von Nodes/Attempts wird zusätzlich
an die validierte Spec und Report-/Logmenge gebunden. Keine Pfade, Payloadbytes,
Requestbytes, streaminternen Namen, PIDs, errno-Texte oder Secrets werden gespeichert.

Die initiale Ausgabe von `payload --flow-v2` bleibt der unveränderte V1.3-Report.
Bruce hält diese Bytes jedoch bis nach erfolgreicher Sidecar-Adoption in einem
Puffer zurück. Bei Encode-, Größen-, Schreib- oder Adoptionsfehler bleibt
stdout leer, stderr enthält exakt `{"error":"FLOW_RECORD_INVALID"}\n`, und der
Prozess endet mit Exit 2; bereits publizierte Legacy-Artefakte bleiben zur
Diagnose erhalten. Ein fachlich gescheiterter Run mit vollständigem Sidecar
gibt den unveränderten Legacy-Report aus und behält Exit 1.

## Read-only Inspector

`payload-inspect BUNDLE --flow-v2` öffnet Sidecar, Legacy-BUNDLE und Runroot sicher.
Der Flow-Inspector verwendet dafür eine integrierte gemeinsame
`PayloadFlowSnapshotV2`-Reader-/Deadline-Sicht: Sidecar, Report, Log und alle
zugeordneten Dateien werden unter derselben bounded Readerinstanz gelesen und
am Ende erneut auf Identität und Metadaten geprüft. Er ruft
`inspect_payload_run_v1()` nicht als separaten ersten Vollscan auf; gemeinsame
interne Prüfhelfer dürfen wiederverwendet werden. Damit gilt eine einheitliche
Konsistenzgrenze statt zweier zeitlich getrennter Lesesichten.
Zusätzlich muss er:

1. Sidecar-`report_digest`, `spec_digest`, `run_id` gegen gelesenen Report/Log prüfen.
2. Node-/Callback-/Request-Metadaten vollständig mit Receipts/Requestdateien
   abgleichen; DAG auf eindeutige Nodes, bekannte direkte Dependencies und
   Zyklen prüfen. Abhängigkeiten müssen der gespeicherten Reihenfolge entsprechen.
3. Jeden gestarteten Logattempt genau einmal finden; keine fremden, doppelten
   oder fehlenden Attempts akzeptieren. Reservierte Preflight-IDs ausnehmen.
4. Pro Attempt exakt alle direkten Inputs mit lückenlosen Indizes verlangen.
   Manifest-ID muss ausschließlich der gemeinsamen versionierten
   `v5_manifest_id_v1(run_id,node,attempt)`-Hilfe entsprechen. Producer muss im
   Legacy-Report/Log akzeptiert und vor dem Consumerstart akzeptiert worden
   sein; Digest/Größe müssen übereinstimmen.
5. Jeden lokalen Consumerinput und Produceroutput digest-/größenprüfen. Im
   regulären Legacy-Layout heißt die Datei exakt
   `BUNDLE.run/payload-inputs/n<node-ordinal>-<attempt-id>-<input-index>.bin`;
   die Node-Ordinal ist die Position der Node in der validierten Spec, Rootnodes
   besitzen keine Inputdateien. Alle Dateien gestarteter Fehlversuche und
   Retries bleiben eigene Belege; die erwartete globale Menge des flachen
   `payload-inputs/`-Verzeichnisses wird vollständig gegen Sidecar und Log
   geprüft. Fremde oder zusätzliche Einträge sind Fehler.
6. Im Streamlayout liegt pro Attempt und Input exakt
   `BUNDLE.run/payload-inputs/<attempt-id>/input-<index>/input.bin` sowie
   `stream-payload/header.jsonl`, numerische Chunks
   `chunk-00000000000000000000.bin` aufwärts und `seal.json`. Das Inputverzeichnis
   enthält exakt `input.bin` und `stream-payload`; `source.tmp`, Stagingreste,
   gemischte Legacy-Dateien und Extraeinträge sind Fehler. Leere Payloads haben
   keine Chunkdatei. Beide Layouts werden nicht gemischt.
7. Im Streammodus sämtliche Header/Seal/Chunks und die reguläre `input.bin`
   prüfen; andernfalls die regulären V1.3-Eingabedateien prüfen. Es werden keine
   Sidecarpfade als Dateiquelle verwendet.
8. Report, Sidecar, Log und geprüfte Dateizuordnung abschließend erneut auf
   Änderung prüfen. Erkannte Mutation ergibt einen Fehler ohne Retry.

Der Inspector öffnet keinen Artifact Store. Er bestätigt lokale Bindung und
Vollständigkeit relativ zum gespeicherten Graphen, nicht heutige Storeexistenz,
originale Nutzer-Spec oder historische Unveränderlichkeit. Gleichzeitige
konsistente Manipulation aller Dateien durch denselben Benutzer ist ohne externe
Signatur/Vertrauensanker nicht erkennbar. `VALID` ist keine Authentizitätsaussage.

Exakte öffentliche Ausgabe (Schlüsselreihenfolge wie gezeigt):

```json
{"contract":"dag-scheduler-v5-payload-flow-inspect-v2","action":"payload-inspect","status":"VALID","run_status":"READY","run_id":"example","node_count":2,"attempt_count":2,"binding_count":1,"verified_input_bytes":3,"stream_v15":false,"input_completeness":"VERIFIED","provenance_scope":"LOCAL_CONSISTENCY"}
```

`VALID`/Exit 0 gilt auch für konsistente fachlich FAILED-Runs; `run_status`
unterscheidet den fachlichen Ausgang. Ein konsistenter vorhandener, aber nicht
abgeschlossener Run ergibt `INCOMPLETE`/Exit 1 und
`input_completeness="NOT_PROVABLE"`. Fehlende erforderliche Dateien, kaputtes
JSON/Bindings oder Mutation liefern ausschließlich
`{"error":"PROTOCOL_ERROR"}` plus LF und Exit 2. stderr bleibt leer.

## Ressourcen, Sicherheit und Fehler

Bestehende Defaults/Maxima bleiben: Inputbytes 65536/1048576, Outputbytes
65536/5242880, Wallzeit 30000/600000 ms, Operationen 1000000/10000000,
Childspeicher 268435456/2147483648 Bytes; Workers 1..4 und Retry 0..3.
Request plus direkte Inputs zählt logisch einmal; Metadaten und Hostkopien
werden nicht als Adapteroperation oder doppelte Inputbytes gezählt.

Die bestehende Attemptdeadline umfasst Referenzprüfung, Materialisierung,
Bindungsprüfung, Worker und Outputadoption. Der Inspector hat eine einzige
30000-ms-Deadline ab Eintritt; vor jedem neuen Read/Open/Hash-Abschnitt prüfen.
Keine Aussage über Unterbrechbarkeit bereits blockierender Kerneloperationen.
Feste Lesepuffer höchstens 65536 Bytes; keine vollständige Payloadallokation.

Alle neuen Dateizugriffe descriptor-relativ, `O_NOFOLLOW|O_CLOEXEC`, aktueller
Eigentümer, reguläre Single-Hardlink-Dateien, private Verzeichnisse 0700 und
Sidecar/Inputs 0600. Identität, Größe, Modus, Links und mtime/ctime vor/nach Reads
prüfen. Der Inspector schreibt/löscht/repariert nichts; systembedingtes atime
ist möglich. Keine Rekursion außerhalb des fest definierten privaten Layouts.

Bindungsfehler im Preflight benutzen `ARTIFACT_REFERENCE_INVALID`, Byteschäden
`ARTIFACT_DIGEST_MISMATCH`, Ressourcen-/Timeoutfehler vorhandene V1.3-Codes.
Kein Integritätsretry, keine Teilannahme, Nachfolger werden transitiv BLOCKED.
Begonnene Attempts behalten ihre Inputs als Prüfbeleg; gescheiterter Preflight
räumt ausschließlich seine eigenen neu angelegten Inputs auf.

## Abnahme und Nicht-Ziele

Abnahme verlangt echte Consumerbytes für Kette, Fan-in und Fan-out, ordnungsfeste
Bindings, leere Payloads, Rootnodes, Workerparallelität, expliziten Retry,
Preflight-/Publikationsfehler, entfernte/zusätzliche/vertauschte Inputs,
Digest-/Lineage-/Manifestkonflikte, Mutation, Deadline und sichere Dateitypen.
Read-only-Prüfung muss anhand unveränderter Dateiinhalte/Verzeichnismengen belegt
werden. V1.3/V1.4-Reportbytefixtures, V1.5-Workflows, alter Inspector und
Worker-v1/Supervisor bleiben Regressionen.

Keine freien Programme/Plugins, GPU/Netzwerk, externen Payloadspeicher,
Recovery, GC, Replay, Repair, übergreifende Runtime, freie Sprache oder
allgemeines Beweisen. Diese Funktionen bleiben **DOKUMENTIERT / KONZEPT**.
