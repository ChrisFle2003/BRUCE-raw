# Befehlsreferenz – Core 0.102.0

[Zum Handbuch](README.md) · [Dateiformate](formats.md) · [Ergebnisse und Steuerung](results-and-control.md)

## Hilfe, Version und Vertragskataloge

| Aufruf | Wirkung |
| --- | --- |
| `bruce`, `bruce help`, `bruce --help` | Textuelle Gesamthilfe |
| `bruce --version` | `{"contract":"bruce-version-v1","version":"0.102.0"}`; keine weiteren Argumente |
| `bruce contracts` | Eingebaute Forschungsstrategien mit Standardplaner und zulässigen Planern; keine weiteren Argumente |
| `bruce gate-contracts` | Versionierter Forschungs-Gatevertrag mit Ablehnungsgründen und Priorität; keine Argumente, keine Prüfung eines konkreten Laufs |
| `bruce task-contracts` | Acht Anwendungstypen und Architekturgrenzen; keine weiteren Argumente |

`contracts` ist kein vollständiges JSON-Schema für Ergebnisse und enthält nicht alle numerischen Grenzwerte. `task-contracts` nennt `research-int64`, `research-exact`, `grid`, `grid-batch`, `grid-resume`, `grid-research`, `fraction-comparison`, `compatibility`. Es weist ausdrücklich auf fehlende dynamische Ausführungsplugins und uneinheitliche mathematische Budgeteinheiten hin. Die folgenden Daten-Mods sind ein separater Routingpfad und ergänzen diesen Anwendungskatalog nicht.

## `dag-scheduler-v5` – experimenteller Worker-DAG-Lauf

**IMPLEMENTIERT UND BELEGT:** V5 ist ein experimenteller, lokal vom Bruce-
Prozess überwachter Worker-Endpunkt. Er liest dieselbe kanonische
[V3-Spec-Datei](formats.md#dag-scheduler-v3-spec-datei), verwendet weiterhin
nur `literal-v1`, `append-v1` und `join-v1` und startet pro Versuch einen
eingebauten lokalen Child-Prozess:

```text
bruce dag-scheduler-v5 execute SPEC --output BUNDLE
  [--retry N]
  [--max-wall-ms N]
  [--max-input-bytes N]
  [--max-output-bytes N]
  [--max-operations N]
  [--max-memory-bytes N]
  [--workers N]
  [--handoff-v1]
```

`SPEC` wird durch den V3-Parser geprüft. Knoten, Abhängigkeiten, Callback-
Arity, Request-Digests und die V3-Dateigrenzen bleiben deshalb gültig. Ein
Nachfolger wird erst bereit, wenn alle direkten Vorgänger einen akzeptierten
`READY`-Receipt besitzen. `--workers 1` ist der deterministische Solo-Modus;
größere Werte erlauben bis zu vier unabhängige, bereits bereite Knoten
gleichzeitig. Die Receipt-Reihenfolge im Bericht bleibt Spec-Reihenfolge,
unabhängig von der tatsächlichen Abschlussreihenfolge.

Das aus der V3-Spec übernommene `max-started-nodes` reserviert die ersten
Positionen der kanonischen geplanten Reihenfolge. Wiederholungen desselben
Knotens verbrauchen diese Startposition nicht erneut. Wird eine Position wegen
erschöpftem Startbudget abgewiesen, werden ihre abhängigen Knoten blockiert;
die Position wird nicht an einen späteren unabhängigen Knoten umverteilt.

### Optionen, Budgets und Wiederholung

Die Werte gelten je Worker-Versuch und werden im Report unter `budget`
wiederholt. `--retry N` zählt zusätzliche Versuche und ist die einzige
Möglichkeit, Wiederholung zu aktivieren; ohne Option gibt es genau einen
Versuch. `--retry 0` ist explizit zulässig und verhält sich wie die Voreinstellung.

| Option | Standard | Maximum | Bedeutung |
| --- | ---: | ---: | --- |
| `--retry` | `0` | `3` | zusätzliche Versuche |
| `--max-wall-ms` | `30000` | `600000` | harte Laufzeitgrenze je Versuch |
| `--max-input-bytes` | `65536` | `1048576` | harte Eingabebytegrenze je Versuch |
| `--max-output-bytes` | `65536` | `5242880` | harte Ausgabebytegrenze je Versuch |
| `--max-operations` | `1000000` | `10000000` | harter Operationszähler je Versuch |
| `--max-memory-bytes` | `268435456` | `2147483648` | Child-Adressraumgrenze je Versuch |
| `--workers` | `1` | `4` | gleichzeitig laufende unabhängige Versuche |

Wiederholbar sind nur `PROCESS_CRASHED`, `TIMEOUT` und ein als vorübergehend
behandelter `PROTOCOL_ERROR`. `PROCESS_START_FAILED`, ungültige Requests,
Adapterfehler, Digestfehler, Budgetüberschreitungen sowie fachliche
`FAILED`-, `BLOCKED`- und `CANCELLED`-Ergebnisse werden nicht wiederholt.
Jeder Versuch besitzt eine neue `attempt_id` und einen eigenen privaten
Ausgabebereich. Die Versuchshistorie mit Status, Fehlercode, Digest, Größe und
Nutzung bleibt als Metadaten im Report erhalten.

Bruce setzt für jeden Child-Prozess alle fünf Budgets durch. Der Linux-
Adressraum wird zusätzlich per Prozesslimit begrenzt; Peak-Speicher erscheint
als Diagnosewert. Die Supervisor-Grace ist fest auf `1000 ms` gesetzt und
keine CLI-Option. Nach dieser Grace kann Bruce die vollständige
Prozessgruppe beenden. Der Host-Abbruch ist als separater, expliziter Befehl
belegt: `bruce dag-scheduler-v5 cancel BUNDLE`. Er erzeugt atomar unter
`BUNDLE.run/control/cancel.request` die exakten Bytes
`BRUCE_WORKER_CANCEL_V1\n`; der Marker ist besitzerbeschränkt (`0600`) und
wird vor der Rückmeldung synchronisiert. Die Route ist idempotent und gibt
`REQUESTED`, `ALREADY_REQUESTED` oder `ALREADY_FINISHED` zurück. Sie nimmt
keine privaten Run-Root-Pfade, Optionen oder PIDs entgegen und reicht keine
Signale als öffentliche Schnittstelle durch.

Der V5-Host prüft den Marker vor jedem neuen Dispatch und während der
Überwachung laufender Attempts. Nach der Beobachtung werden keine neuen
Knoten oder Retries gestartet, alle aktiven Handles werden in derselben
Scheduler-Iteration über die bestehende `request_cancel`-API angefordert,
und nicht gestartete Knoten erhalten `CANCELLED` ohne Attempt- oder
Artefaktmetadaten. Die feste Supervisor-Grace von `1000 ms`, die
TERM-to-KILL-Eskalation nach 50 ms und die Prozessgruppenbereinigung bleiben
die einzige Durchsetzungslogik. Eine Cancellation-Race wird parent-seitig
linearisiert: Ein bereits vor der Markerbeobachtung akzeptierter Receipt
bleibt akzeptiert; ein danach beobachtetes, noch nicht übernommenes Ergebnis
wird als `CANCELLED` übernommen.

Der eingebaute Child-Dienst liest den initialen `REQUEST`-Frame und bedient
`STATUS_QUERY` beziehungsweise `CANCEL` einmal vor und einmal nach dem
synchronen Aufruf des festen Adapters. Während des synchronen Adapteraufrufs
gibt es keinen zusätzlichen Zwischenpunkt für fachliche Cancellation. Eine
empfangene Anfrage liefert `STATUS`/`ACK`; ein bestätigtes `CANCEL` führt vor
dem Resultat zu `CANCELLED`. Timeout- und Gruppenbeendigung bleiben unter der
Kontrolle des Linux-Supervisors.

### Bericht, Run-Log und Grenzen

Ein erfolgreicher Lauf endet mit Exit 0. Ein ausgeführter Lauf mit einem
terminalen Worker- oder DAG-Fehler veröffentlicht weiterhin einen
Metadatenbericht und endet mit Exit 1. Falsche CLI-Argumente oder ungültige
Optionen enden mit Exit 2. Der öffentliche Bericht trägt den Vertrag
`dag-scheduler-v5-cli-v1`, `spec_digest`, DAG- und Knotenmetadaten, Status,
Fehler-/Resultcode, Budget, Nutzung, Versuchshistorie und ausschließlich
opake akzeptierte Referenzen wie `artifact/a`. Er enthält keine Request- oder
Payloadbytes und keine internen `attempts/...`-Pfade. Ein hostseitig
abgebrochener Lauf verwendet den Status `CANCELLED`, den Fehlercode
`CANCELLED` und Exit 1. Nicht gestartete Receipts haben keine Attempt-ID;
schon gestartete, später abgebrochene Attempts behalten ID, Nutzung und
Historie. Referenz und Digest eines nicht angenommenen Ergebnisses bleiben
leer. Das bestehende öffentliche Berichtsschema bleibt erhalten.

Bruce schreibt zusätzlich im privaten Laufbaum
`BUNDLE.run/control/run-log.jsonl`. Die Datei ist append-only, sequenziell
validiert und enthält nur kontrollierte Metadaten. Im normalen Ablauf treten
`RUN_CREATED`, `ATTEMPT_STARTED`, `STATUS`, `CANCEL_REQUESTED`,
`PROCESS_EXITED`, `RESULT_RECEIVED`, bei akzeptierten Artefakten
`ARTIFACT_ACCEPTED` und am Ende `RUN_FINISHED` auf. Nach beobachtetem Cancel
tragen die anschließend projizierten `PROCESS_EXITED` und `RESULT_RECEIVED`
den Status `CANCELLED`. War das Ergebnis bereits projiziert, darf genau ein
nachfolgendes `CANCEL_REQUESTED/CANCELLED` dessen noch ausstehende Entscheidung
abschließen; es werden keine zweiten Ergebnisereignisse geschrieben. Ein
solcher Attempt erzeugt kein `ARTIFACT_ACCEPTED`.

Der gewöhnliche lokale Host-V2-Pfad schreibt außerdem
`RESULT_REJECTED/OUTPUT_LIMIT_EXCEEDED`, wenn der Scheduler ein kanonisches
READY erst wegen des gemeinsamen Receipt-Budgets ablehnt. Die lokale
Payload-V13/V14-Route unterstützt zusätzlich `TIMEOUT`,
`ARTIFACT_REFERENCE_INVALID`, `ARTIFACT_MANIFEST_INVALID`,
`ARTIFACT_DIGEST_MISMATCH` und `ARTIFACT_STORE_VIOLATION`. Dieses Ereignis
enthält keine Artefakt-/Usage-Felder und ist nur vor Annahme, Cancel oder einer
vorherigen Ablehnung erlaubt. Der Inspector behandelt den Attempt danach als
FAILED und erhält die bereits gemessene Usage. TIMEOUT bleibt bis zur
expliziten Retryentscheidung offen; die übrigen Ablehnungsgründe sind terminal.
Ein anschließender Cancel schließt die Attempt-Serie und verhindert einen
Neustart. Der Worker schreibt dieses Log nicht. Interne relative
Attempt-Referenzen bleiben auf den privaten Run-Baum beschränkt; ungefilterte
Payloadbytes, absolute Pfade, Kommandozeilen und Stderr gehören nicht in den
öffentlichen Bericht oder das Log.

Belege: [V5-C++-Verträge](../../tests/unit/dag_scheduler_v5_cli_tests.cpp),
[V5-Prozessworkflows](../../tests/integration/test_dag_scheduler_v5_cli.py),
[V5-Abbruch-Workflows](../../tests/integration/test_dag_scheduler_v5_abort.py),
[Abort-Marker-Kern](../../include/bruce/worker/worker_abort_control_v1.hpp),
[Worker-Supervisor-Vertrag](../../tests/unit/worker_supervisor_v1_tests.cpp).
Die bisherigen V2-, V3- und V4-Routen bleiben unverändert.

### V5-Worker-Abbruch V1 – explizite Host-Steuerung

**IMPLEMENTIERT UND BELEGT:** Die öffentliche Route lautet exakt:

```text
bruce dag-scheduler-v5 cancel BUNDLE
```

`BUNDLE` bezeichnet die veröffentlichte V5-Execute-Datei. Bruce bindet den
privaten Run-Root ausschließlich als `BUNDLE.run/`; ein `.run`-Ziel,
zusätzliche Argumente, Optionen, leere oder mit `-` beginnende Pfade werden
als Syntaxfehler mit Exit 2 abgewiesen. Fehlt der Run-Root, ist er unsicher,
oder ist der Marker manipuliert, gibt die Route ausschließlich
`{"error":"CANCEL_TARGET_UNAVAILABLE"}` mit Exit 1 aus. Ein fertiger Lauf
hat Vorrang vor einem vorhandenen Marker und liefert `ALREADY_FINISHED`.

Die erfolgreiche, metadata-only Antwort ist kanonisch:

```json
{"contract":"dag-scheduler-v5-cancel-v1","action":"cancel","status":"REQUESTED"}
```

Nach der ersten Anfrage liefert eine Wiederholung `ALREADY_REQUESTED`; sie
legt keinen zweiten Marker an. Der Marker enthält keine Payloadbytes, Pfade,
PIDs, Kommandozeilen oder Tokens. Der Host pollt ihn read-only über die
descriptor-relative, symlinkgeschützte V1-Schnittstelle und bleibt für
Worker-Abbruch und Reaping verantwortlich.

Ein aktiver Attempt erhält genau ein `CANCEL_REQUESTED`. Bruce adressiert
alle gleichzeitig aktiven Handles in derselben Host-Iteration, leert die
Ready-Menge, unterdrückt Nachfolgerstarts sowie `--retry`-Wiederholungen und
markiert nicht gestartete Knoten als `CANCELLED`. Die terminale Run-Log-Zeile
ist `RUN_FINISHED` mit `status:"CANCELLED"`; `inspect BUNDLE` akzeptiert den
vollständig geloggten Lauf als `VALID` mit `run_status:"CANCELLED"`, während
`publish` abgebrochene Läufe weiterhin ablehnt.

Die Cancel-Route fordert keinen direkten Prozess oder PID an. Sie nutzt
ausschließlich die bestehende Worker-Supervisor-Grenze
`worker_v1::request_cancel`; deren feste 1000-ms-Grace, 50-ms-Eskalation und
Prozessgruppen-Reaping bleiben unverändert. Der Vertrag beendet lokale,
Bruce-eigene Worker-Processes; freie Code-Plugins, GPU-/Netzwerkausführung,
externe Payloadspeicher, freie Sprache, allgemeines Beweisen und eine
übergreifende Runtime-Orchestrierung sind weiterhin
**DOKUMENTIERT / KONZEPT**.

### V5-Run-Inspector

**IMPLEMENTIERT UND BELEGT:** Der V5-Endpunkt besitzt zusätzlich die exakt
begrenzte read-only Route:

```text
bruce dag-scheduler-v5 inspect BUNDLE
```

`BUNDLE` muss die veröffentlichte V5-Execute-Datei sein. Der Inspector bindet
den Lauf ausschließlich an die daneben liegende Datei `BUNDLE.run/`; ein
alternativer Run-Root, `BUNDLE.run` als Eingabe, Optionen oder zusätzliche
Argumente sind nicht zulässig. `./bundle` und `sub/../bundle` werden vor der
Bindung lexikalisch normalisiert. Symlinks, Root-Escapes und unsichere private
Dateien werden abgewiesen.

Der Inspector startet keinen Worker, ruft keinen Adapter auf, setzt keinen
Lauf fort, repariert oder trunciert kein Log und schreibt keine Datei. Er liest
den kanonischen Execute-Bericht und `control/run-log.jsonl`, prüft die
per-Attempt-Zustände, Sequenzen, Größen, SHA-256-Digests und Artefakte relativ
zum privaten Run-Root. Artefakt- und Payloadbytes, absolute Pfade,
`attempts/...`-Referenzen, Kommandozeilen und Fehlermeldungen des
Betriebssystems erscheinen nicht im öffentlichen Bericht.

Der JSON-Bericht trägt den Vertrag `dag-scheduler-v5-inspect-v1` und enthält
nur `status`, `run_finished`, `run_status`, `run_id`, `last_seq` sowie pro
Attempt `attempt_id`, Statusfolge, Akzeptanz, Ereignisanzahl, Digest,
`size_bytes`, begrenzte Nutzung und `artifact_verified`. Die Attempts bleiben
in ihrer ersten beobachteten Reihenfolge. Ein vollständiger, strukturell
gültiger Lauf mit `RUN_FINISHED` endet mit Exit 0 und `VALID`. Ein gültiger
Verlauf ohne abschließendes `RUN_FINISHED` endet mit Exit 1 und
`INCOMPLETE`; er wird nicht fortgesetzt. Korrupte, abgeschnittene,
manipulierte oder unsichere BUNDLE-/Run-Daten ergeben leeres stdout, exakt
`{"error":"PROTOCOL_ERROR"}` auf stderr und Exit 2. Syntaxfehler behalten
die feste V5-Usage-Ausgabe.

Die JSON-Auswertung ist auf 64 Verschachtelungsebenen begrenzt. Reportwerte
werden an den beobachteten Run gebunden; dazu gehören Retry-, Workerzahl- und
Budgetobergrenzen, Request- und Payload-Digests, Status-/Resultcodes,
Attempt-Historie und Usage. Der Inspector ist damit eine lokale Diagnose- und
Integritätsprüfung, kein Replay, kein Artifact Store und kein allgemeiner
Recovery-Executor.

Belege: [Inspector-Kern](../../include/bruce/worker/worker_run_inspector_v1.hpp),
[Inspector-Implementierung](../../src/worker/worker_run_inspector_v1.cpp),
[V5-CLI](../../apps/dag_scheduler_v5_cli.hpp),
[C++-Verträge](../../tests/unit/dag_scheduler_v5_cli_tests.cpp),
[Inspector-Workflows](../../tests/integration/test_dag_scheduler_v5_inspector.py).

### V5-Execute-Handoff-Lineage V1

**IMPLEMENTIERT UND BELEGT:** Für die bestehende Execute-Route ist die
lokale Producer-/Consumer-Übergabe ausdrücklich zuschaltbar:

```text
bruce dag-scheduler-v5 execute SPEC --output BUNDLE --handoff-v1
bruce dag-scheduler-v5 inspect BUNDLE --handoff-v1
```

`--handoff-v1` darf beim Execute genau einmal nach `--output BUNDLE` unter den
optionalen V5-Argumenten stehen. Beim Inspect muss es genau einmal unmittelbar
nach `BUNDLE` stehen. Andere Routen akzeptieren die Option nicht. Ohne sie
bleiben Execute-Bericht und bestehende V5-Inspect-Route bytegleich. Der
separate `payload`-Pfad nutzt für persistierte Eingabekanten bereits
[Payload Flow V2](#v5-payload-flow-v2--persistierte-kanten-lineage).

Nach dem unveränderten Execute-Bericht legt Bruce exklusiv und atomar die
private Datei `BUNDLE.handoff-v1.json` an. Sie bindet die exakten Berichts-
und Specbytes per SHA-256 sowie jeden tatsächlich gestarteten Versuch an
seine direkten, akzeptierten Producer-Inputs. Root-Versuche haben `inputs:[]`;
auch fehlgeschlagene Versuche und Retries erhalten eigene Einträge. Eine
reservierte, nie gestartete Preflight-ID bleibt ausgenommen. Nach beobachtetem
Host-Abbruch entsteht kein weiterer Attempt-Eintrag. Die vollständige
[Feldreihenfolge und Größenbindung](formats.md#v5-execute-handoff-lineage-v1)
sind im Formatvertrag beschrieben. Das Worker-Frame ändert sich nicht.

Der read-only Inspector prüft Sidecar, Execute-Bericht, Run-Log, Requests,
private Eingabedateien und akzeptierte Producer-Artefakte auf lokale
Konsistenz. Er startet keinen Worker, schreibt oder repariert nichts und
liefert nur Zähler und Statusmetadaten. Ein vollständiger konsistenter Lauf
liefert `VALID`/Exit 0, auch bei `run_status=FAILED`, `BLOCKED` oder
`CANCELLED`; ein konsistenter Verlauf ohne `RUN_FINISHED` liefert
`INCOMPLETE`/Exit 1. Falsche Syntax, unsichere oder widersprüchliche Daten
liefern leeres stdout, exakt `{"error":"PROTOCOL_ERROR"}` plus LF auf
stderr und Exit 2. Schlägt beim Execute das Kodieren oder die exklusive
Übernahme des Sidecars nach dem Legacy-Bericht fehl, lautet der Fehler
`{"error":"HANDOFF_RECORD_INVALID"}` plus LF, Exit 2 bei leerem stdout;
bereits erzeugte Legacy-Dateien bleiben zur Diagnose erhalten.

Die Seite ist owner-only `0600`, private Verzeichnisse `0700`. Begrenzte,
descriptor-relative `O_NOFOLLOW|O_CLOEXEC`-Zugriffe prüfen Eigentümer,
Einzellink und Dateiidentität vor und nach Reads. Die Aussage ist lokale
Konsistenz zum Prüfzeitpunkt, keine Authentizität oder historische Herkunft.
Freie Sprache, allgemeines Beweisen, GPU-/Netzwerkausführung, freie Plugins,
externe Stores, Recovery/Resume und breite Runtime-Orchestrierung bleiben
**DOKUMENTIERT / KONZEPT**.

Belege: [Codec](../../src/artifact/execute_handoff_lineage_v1.cpp),
[Inspector](../../src/artifact/execute_handoff_inspector_v1.cpp),
[CLI-Verträge](../../tests/unit/dag_scheduler_v5_cli_tests.cpp) und
[reale Workflows](../../tests/integration/test_dag_scheduler_v5_handoff.py).

### V5-Artifact-Publish

**IMPLEMENTIERT UND BELEGT:** Die ausdrücklich separate Publish-Route importiert
einen bereits abgeschlossenen und vom Inspector validierten V5-Lauf in einen
lokalen Artifact Store:

```text
bruce dag-scheduler-v5 publish BUNDLE --store STORE
```

Die Route ist `Inspector-first`: BUNDLE und der ausschließlich daran gebundene
Seitensatz `BUNDLE.run/` werden vor dem ersten Store-Schreibvorgang vollständig
geprüft. Zulässig sind nur Läufe mit `RUN_FINISHED`, `VALID`/`READY` und genau
einem akzeptierten `ARTIFACT_ACCEPTED` je V5-Spec-Knoten. Die Knoten werden in
der deterministischen `inputs/`-/Spec-Reihenfolge importiert. Unvollständige,
fehlgeschlagene, blockierte, abgebrochene, manipulierte oder unsichere Läufe
werden als Protokollfehler abgewiesen.

Der Publish-Bericht ist metadata-only und besitzt den Vertrag
`dag-scheduler-v5-publish-v1`. Die Feldreihenfolge ist fest:

```json
{"contract":"dag-scheduler-v5-publish-v1","action":"publish",
 "status":"PUBLISHED","run_id":"run-id","manifests":[
 {"node_id":"a","attempt_id":"a-1","reference":"artifact/a",
  "manifest_id":"v5-<sha256>","sha256":"<64-hex>","size_bytes":12}]}
```

Auch die sechs Felder jedes `manifests`-Eintrags bleiben in dieser Reihenfolge:
`node_id`, `attempt_id`, `reference`, `manifest_id`, `sha256`, `size_bytes`.
Der Bericht enthält keine Payloadbytes, privaten Pfade, Run-Log-Inhalte,
Kommandozeilen oder Betriebssystemfehler.

Für jeden akzeptierten Knoten wird ein kanonisches
`BRUCE_ARTIFACT_MANIFEST_V1` veröffentlicht. Es bindet `run_id`, `node_id`,
`attempt_id`, `status=READY`, den adapterabhängigen `result_code` und genau
ein Objekt mit Digest, Größe und `origin=artifact/<node>` an. Die Manifest-ID
ist deterministisch aus
`BRUCE_DAG_SCHEDULER_V5_PUBLISH_V1`, `run_id`, `node_id` und `attempt_id`
gebildet (`v5-` plus SHA-256 der kanonischen Bindungszeilen). Gleiche
Wiederholung ist idempotent; abweichende oder beschädigte bestehende Objekte
oder Manifeste werden nicht überschrieben und ergeben `PROTOCOL_ERROR`.

| Fall | stdout | stderr | Exit |
| --- | --- | --- | ---: |
| vollständiger validierter Lauf, alle Objekte und Manifeste veröffentlicht | Publish-Bericht mit LF | leer | 0 |
| unvollständiger/fehlgeschlagener/manipulierter Lauf, unsicherer Pfad, falscher Store oder Konflikt | leer | `{"error":"PROTOCOL_ERROR"}` plus LF | 2 |
| falsche Unterroute oder zusätzliche Argumente | leer | `{"error":"PROTOCOL_ERROR"}` plus LF | 2 |

Die Route verändert weder BUNDLE noch BUNDLE.run, startet keinen Worker, ruft
keinen Adapter auf und führt keinen Replay aus. Store-Schreibvorgänge sind
V1.1-seitig nicht transaktional: Nach einem späteren Fehler können bereits
immutable publizierte, noch nicht referenzierte Objekte liegen bleiben; diese
bewusste Teilpublikation ist die V1.1-Grenze. Es
gibt dafür keine Löschung, kein Rollback und keine GC-Route.

Belege: [Publish-Implementierung](../../apps/dag_scheduler_v5_cli.hpp),
[Publish-C++-Verträge](../../tests/unit/dag_scheduler_v5_cli_tests.cpp),
[Publish-Workflows](../../tests/integration/test_dag_scheduler_v5_publish.py),
[Artifact Store API](../../include/bruce/artifact/artifact_store_v1.hpp) und
[Publish-Spezifikation](../../docs/superpowers/specs/2026-10-15-worker-artifact-publish-v1-design.md).

Freie Sprache, ein allgemeiner Beweiser, GPU- oder Netzwerkausführung,
dynamische Code-Plugins, externe Payloadspeicher und eine allgemeine
übergreifende Runtime-Orchestrierung sind weiterhin **DOKUMENTIERT / KONZEPT**.
Der V5-Worker ist ein fester, vertrauenswürdiger Bruce-Child und kein freier
Programmstarter.

### V5-Payload-Flow V1.3

**IMPLEMENTIERT UND BELEGT:** Der explizite Payloadmodus verbindet direkte
Vorgänger eines V3-DAGs mit dem bestehenden V5-Worker über den lokalen
Artifact Store:

```text
bruce dag-scheduler-v5 payload SPEC --output BUNDLE --store STORE
  [--retry N] [--max-wall-ms N] [--max-input-bytes N]
  [--max-output-bytes N] [--max-operations N]
  [--max-memory-bytes N] [--workers N]
```

Der Modus ist eine eigene Route. `execute`, `inspect` und `publish` behalten
ihre bestehenden Parser, Berichte und Grenzen. `SPEC` verwendet weiterhin den
kanonischen V3-DAG-Vertrag. Bruce validiert den vollständigen DAG vor dem
ersten Store-Zugriff, bestimmt ausschließlich direkte Vorgänger und erzeugt
deren Eingaben erst nach einem akzeptierten `READY`-Output.

Seit Stage 3D verwendet die lokale V1.3-/V1.4-Route für einen oder mehrere
Knoten den gemeinsamen Batch-Host mit `--workers 1..4` und `--retry 0..3`.
DAG, Materialisierung, Store-Veröffentlichung und Bericht bleiben Aufgaben
des Schedulers. Die Attempt-Deadline beginnt vor der Eingabevorbereitung und
wird vor Dispatch, während der Workerüberwachung sowie vor/nach Veröffentlichung
geprüft; synchrone Store-Aufrufe werden nicht einzeln hart unterbrochen.
Ein Fehler vor dem ersten Worker kann FAILED mit leerer Attempt-Historie
ergeben. Erst die Host-Annahme schaltet veröffentlichte Manifestreferenzen
und Nachfolger frei; nach Veröffentlichung abgelehnte/abgebrochene immutable
Store-Einträge bleiben ohne automatische Rücksetzung erhalten. Stream V1.5,
Flow V2 und Handoff V1 behalten ihre bisherigen Ausführungspfade.
Beleg: [Stage-3D-Workflows](../../tests/integration/test_common_runtime_host_v2_stage3d.py).

Die interne, versionierte Auswahlreferenz `ArtifactInputRefV13` enthält exakt
`manifest_id`, `object_index`, `sha256` und `size_bytes`. Nutzer liefern diese
Referenzen nicht frei ein: Bruce bildet sie aus der direkten DAG-Kante und dem
deterministisch veröffentlichten Vorgängermanifest. Nur kanonische Manifeste
mit `status=READY` und passender `run_id`, Vorgänger-ID, `attempt_id`, Digest-
und Größenbindung werden akzeptiert. `origin` ist Provenienzmetadatum und
niemals ein Pfad oder Downloadziel.

Bruce bleibt die einzige Store- und DAG-Autorität. Er prüft das Objekt
descriptor-relativ mit `O_NOFOLLOW`, streamt es bounded in eine neue owner-only private
Attempt-Eingabe und prüft die Zieldatei danach erneut mit Digest und Größe.
Der Worker sieht nur die relative Bruce-Referenz unter seinem privaten
Run-Root sowie erwarteten Digest, Größe und bestehende Limits. Store-Root,
Manifestpfade, Payloadbytes, Netzwerk, neue Prozesse und Plugins erreichen
den Worker nicht. Die Materialisierung mehrerer direkter Inputs erfolgt in
Spec-Reihenfolge; bei einem Fehler werden bereits angelegte private
Eingabedateien des Attempts entfernt.

Die V1.3-Grenzen sind:

| Option/Grenze | Standard | Maximum | Bedeutung |
| --- | ---: | ---: | --- |
| `--retry` | `0` | `3` | zusätzliche Worker-Versuche, nur explizit aktiviert |
| `--max-wall-ms` | `30000` | `600000` | End-to-End-Deadline je Attempt, einschließlich Resolve, Materialisierung, Worker und Publikation |
| `--max-input-bytes` | `65536` | `1048576` | Request plus direkte Vorgängerobjekte |
| `--max-output-bytes` | `65536` | `5242880` | geprüfter Workeroutput |
| `--max-operations` | `1000000` | `10000000` | fachliche Workeroperationen |
| `--max-memory-bytes` | `268435456` | `2147483648` | bestehende Linux-Child-Grenze |
| `--workers` | `1` | `4` | unabhängige READY-Kandidaten parallel |
| direkte Payloadobjekte | `0` | `15` | ein Platz des unveränderten 16er-Workerlimits gehört dem Request; insgesamt sind 16 Gesamtinputs zulässig |

Ein akzeptierter Output wird sofort als unveränderliches Objekt und
`READY`-Manifest publiziert; erst danach werden direkte Nachfolger freigegeben.
Ein Referenz-, Manifest-, Store-, Integritäts-, Materialisierungs- oder
Inputlimitfehler setzt den Node auf `FAILED` und abhängige Nodes rekursiv auf
`BLOCKED`. Nur ein akzeptierter READY-Attempt entsperrt Nachfolger. Die
Publikation und Zustandsübernahme bleiben bei `--workers N` durch Bruce
deterministisch.

Der metadata-only Bericht verwendet den Vertrag
`dag-scheduler-v5-payload-v13` und `action="payload"`. Er bindet
`spec_digest`, `dag_id`, Retry-/Workerwerte, die fünf Budgets, Status und je
Receipt `node_id`, `callback_id`, `request_digest`, `attempt_id`, Status,
Fehler-/Resultcode, öffentliche Referenz oder `null`, Payload-Digest,
Payloadgröße, Usage und Attempt-Historie. Interne Manifest-IDs,
`ArtifactInputRefV13`, private Pfade, Payloadbytes, PIDs, `errno`-Texte und
Kommandozeilen werden nicht ausgegeben. Ein erfolgreicher Lauf endet mit
Exit 0; ein ausgeführter Lauf mit `FAILED` oder `BLOCKED` liefert den
Metadatenbericht mit Exit 1; Syntax-, Store- und Protokollfehler liefern leeres
stdout, einen kompakten Fehlerbericht und Exit 2.

`--retry N` ist die einzige Wiederholungsoption. Prozessabsturz, Timeout und
der bestehende temporäre Worker-Protokollfehler können wiederholt werden; jeder
Versuch erhält eine neue `attempt_id`. Referenz-, Manifest-, Integritäts-,
Berechtigungs- und Inputlimitfehler werden nicht automatisch wiederholt.

Belege: [V1.3-CLI](../../apps/dag_scheduler_v5_cli.hpp),
[Payload-Kern](../../include/bruce/artifact/artifact_payload_v1.hpp),
[Payload-Workflowtests](../../tests/integration/test_dag_scheduler_v5_payload.py),
[CLI-Verträge](../../tests/unit/dag_scheduler_v5_payload_cli_tests.cpp),
und der [Payload-Workflow](../../tests/integration/test_dag_scheduler_v5_payload.py).

Freie oder ungeprüfte Manifest-Referenzen außerhalb des kontrollierten
V1.4-Sidecars, externe oder entfernte Stores, Netzwerk-/GPU-Ausführung, freie
Code-Plugins, freie Sprache, allgemeines Beweisen und eine
übergreifende Runtime-Orchestrierung gehören weiterhin zu
**DOKUMENTIERT / KONZEPT**. V1.3 ist ein kontrollierter lokaler Host-/Store-/
Worker-Pfad und erweitert die bestehenden V2-, V3-, V4-, V5- und Resolve-
Verträge nicht.

### V5-Payload-Flow V1.4 – externe lokale Eingaben

**IMPLEMENTIERT UND BELEGT:** V1.4 ergänzt ausschließlich die Payload-Route um
eine explizite Sidecar-Datei mit bereits veröffentlichten lokalen Artifact-
Referenzen:

```text
bruce dag-scheduler-v5 payload SPEC --output BUNDLE --store STORE \
  --external-inputs INPUTS
```

Alle V1.3-Optionen bleiben verfügbar und unverändert. Ohne
`--external-inputs` bleibt der V1.3-Vertrag byte- und verhaltenskompatibel;
die Route meldet dann `dag-scheduler-v5-payload-v13`. Mit der Option wird der
Reportvertrag `dag-scheduler-v5-payload-v14` verwendet.

`INPUTS` ist eine Bruce-eigene, owner-only JSON-Datei. Sie wird exakt und
ohne Whitespace eingelesen; unbekannte oder doppelte Felder, zusätzliche
Escape-Sequenzen, Symlinks, Mehrfachlinks und fremde Dateirechte werden
abgelehnt. Das kanonische Format lautet:

```json
{"contract":"bruce-dag-scheduler-v5-external-inputs-v14","bindings":[{"node_id":"merge","inputs":[{"manifest_id":"v5-source-001","object_index":0,"sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef","size_bytes":42,"source_run_id":"run-source","source_node_id":"source","source_attempt_id":"attempt-1"}]}]}
```

Jede Eingabe bindet exakt `manifest_id`, `object_index`, `sha256`,
`size_bytes`, `source_run_id`, `source_node_id` und `source_attempt_id`. Die
Bindung ist an einen vorhandenen DAG-Node gebunden. Bruce öffnet ausschließlich
den ausdrücklich gewählten lokalen Store, verlangt ein kanonisches `READY`-
Manifest und prüft Provenienz, Objektindex, Digest und Größe. Die Sidecar-Datei
und das referenzierte Objekt müssen im selben Store-Kontext auflösbar sein;
kein Pfad, keine URL und kein Netzwerkziel wird an den Worker weitergegeben.

Direkte In-Run-Abhängigkeiten und Sidecar-Eingaben werden gemeinsam gezählt:
Ein Node darf höchstens 15 Payloadobjekte enthalten, weil der unveränderte
Workervertrag 16 Gesamtinputs erlaubt und der Request selbst einen Platz
belegt. Das Sidecar zählt außerdem höchstens 15 Eingaben pro Bindung; die
kombinierte Grenze wird gegen die direkten DAG-Abhängigkeiten geprüft.

Bruce materialisiert die geprüften Objekte bounded in owner-only private
Attempt-Dateien und löscht alle bereits erzeugten Teildateien, wenn ein
späteres Objekt scheitert. Der Worker erhält nur die privaten relativen
Referenzen, Digest, Größe und die bestehenden Budgets. Pro V1.4-Report kommen
`external_inputs_digest` auf Laufebene sowie `external_input_count` und
`external_input_bytes` je Receipt hinzu. Manifest-IDs, Sidecar-Payloadbytes,
private Pfade, PIDs und Betriebssystemdiagnosen bleiben verborgen.

Syntax-, Dateisicherheits-, unbekannte Node-, doppelte Bindungs- und
Arityfehler werden vor dem Lauf mit leerem stdout, kompaktem Fehler-JSON und
Exit 2 abgewiesen. Fehler beim Auflösen eines fehlenden/nicht bereiten
Manifests, bei Objektintegrität, Storezugriff, Inputlimit oder Timeout erzeugen
im gestarteten Lauf einen `FAILED`-Receipt, blockieren abhängige Nodes und
enden mit Exit 1. Abhängige Nodes erhalten dabei den Status `BLOCKED`. Diese
Fehler werden nicht implizit wiederholt; nur das
explizite `--retry N` darf die bestehenden retrybaren Workerfehler wiederholen.
Die stabilen Ergebnisnamen dafür sind `EXTERNAL_INPUTS_INVALID`,
`EXTERNAL_INPUT_PERMISSION`, `EXTERNAL_INPUT_BINDING_INVALID`,
`EXTERNAL_MANIFEST_INVALID`, `EXTERNAL_OBJECT_INVALID`,
`EXTERNAL_INPUT_LIMIT_EXCEEDED` und `EXTERNAL_INPUT_TIMEOUT`.

V1.4 erweitert weder `execute`, `inspect`, `publish`, Resolve noch V2/V3/V4.
Freie externe Stores, Netzwerk-/GPU-Ausführung, freie Code-Plugins, freie
Sprache, allgemeines Beweisen und eine übergreifende Runtime-Orchestrierung
bleiben **DOKUMENTIERT / KONZEPT**.

Belege: [V1.4-CLI](../../apps/dag_scheduler_v5_cli.hpp),
[Sidecar-Vertrag](../../include/bruce/artifact/artifact_payload_external_v1.hpp),
[Sidecar-Parser](../../src/artifact/artifact_payload_external_v1.cpp),
[V1.4-Workflowtests](../../tests/integration/test_dag_scheduler_v5_payload.py)
und [V1.4-Spezifikation](../../docs/superpowers/specs/2026-10-15-artifact-payload-v1-external-inputs-design.md).

### V5-Payload-Stream V1.5 – explizite private Chunk-Eingaben

**IMPLEMENTIERT UND BELEGT:** ausschließlich für den expliziten Payloadpfad:

```text
bruce dag-scheduler-v5 payload SPEC --output BUNDLE --store STORE --stream-v15
bruce dag-scheduler-v5 payload SPEC --output BUNDLE --store STORE --external-inputs INPUTS --stream-v15
```

Für die Streamproduktion ist `--stream-v15` payload-only, wertlos (kein Optionswert) und genau einmal
zulässig. Doppelte Optionen sowie `execute`, `inspect` und `publish` mit dieser
Option werden abgewiesen. Die separate Route `payload-inspect` nutzt denselben
Optionsnamen ausschließlich als Prüfanforderung (siehe unten). Ohne die Option bleibt der bisherige V1.3-/V1.4-Pfad
unverändert; die V1.3-/V1.4-Berichte enthalten keine Streamfelder und behalten
`dag-scheduler-v5-payload-v13` beziehungsweise `dag-scheduler-v5-payload-v14`.

Bruce erzeugt pro Attempt und Input einen privaten owner-only Bereich
`BUNDLE.run/payload-inputs/<attempt-id>/input-<index>/`. Unter `stream-payload/`
liegen ausschließlich `header.jsonl`, numerisch geordnete Chunkdateien ab
`chunk-00000000000000000000.bin` und `seal.json`. Die feste Chunkgröße beträgt 65.536 Bytes (65,536 bytes), nur der
letzte Chunk darf kürzer sein; leere Payloads haben keine Chunkdatei. Bruce
prüft SHA-256-Digest, exakte Größe und Chunkanzahl, kanonischen Header und Seal,
Dateityp, Eigentümer, Rechte und einzelne Hardlinks mit `O_NOFOLLOW`. Alle
Verzeichnisse sind owner-only `0700`, Dateien owner-only `0600`.

Die Streamübernahme ist exklusiv und atomar nach `fsync` und vollständiger
Prüfung. Byte-Limit und monotone End-to-End-Deadline gelten auch für Streambau,
Verifikation und Kompatibilitätskopie; vor jedem Quellread wird die verbleibende
Bytefreigabe begrenzt. Ein blockierender Kernel-Dateizugriff selbst ist nicht
unterbrechbar. Nach authentifizierter Chunklektüre erzeugt Bruce bounded eine
reguläre Kompatibilitätsdatei `input.bin` und prüft deren Digest und exakte Größe
nochmals. **WorkerRequest bindet diese reguläre Datei**, nicht das
Streamverzeichnis: Der unveränderte Supervisor verlangt reguläre ArtifactRefs.
Das worker-v1 Frame bleibt unverändert; eine Zero-Copy-Worker-Schnittstelle
wird nicht behauptet. Die vorhandenen Input-, Worker- und Adapterlimits bleiben.

Fehler vor dem Start entfernen den gesamten aktuellen Attempt-Inputbereich,
auch bereits adoptierte Streams, partielle Kompatibilitätsdateien und temporäre
Quellen; es gibt kein `ATTEMPT_STARTED` und keine Store-Publikation. Auch die
letzte Deadlineprüfung nach erfolgreicher Vorbereitung räumt auf. Retries haben
isolierte Attemptbereiche. Erfolgreiche private Streams und `input.bin` bleiben
lokale vertrauliche Laufartefakte; temporäre Quellen und Stagingreste verschwinden.
Öffentliche Berichte und Run-Log sind metadata-only: keine Payloadbytes oder
Hexdarstellungen, privaten Pfade, Streamnamen, Header- oder Sealinhalte.
Owner-only Speicher ist die Vertrauensgrenze; spätere Manipulation durch einen
Prozess desselben Benutzers ist nicht kernel-seitig ausgeschlossen.

Externe/Netzwerk-Stores, GPU, freie Plugins, freie Sprache, allgemeines Beweisen
und große Runtime-Orchestrierung bleiben **DOKUMENTIERT / KONZEPT**.
Belege: [Streamkern](../../src/artifact/artifact_payload_stream_v1.cpp),
[Streamverträge](../../tests/unit/artifact_payload_stream_v1_tests.cpp),
[CLI-Verträge](../../tests/unit/dag_scheduler_v5_payload_cli_tests.cpp) und
[reale Payload-Workflows](../../tests/integration/test_dag_scheduler_v5_payload.py).

### V5-Payload-Run-Inspector V1 – vorhandene Läufe prüfen

**IMPLEMENTIERT UND BELEGT:** Die neue read-only Route prüft gespeicherte
V1.3-/V1.4-Payload-Läufe, einschließlich Solo-/Multi-Reports und Hostfehlern:

```text
bruce dag-scheduler-v5 payload-inspect BUNDLE [--stream-v15]
```

`BUNDLE` bezeichnet die Reportdatei; `BUNDLE.run/` muss daneben erhalten sein.
Die wertlose Option darf genau einmal nach BUNDLE stehen. Fehlende, zusätzliche,
doppelte oder falsch platzierte Argumente sind `PROTOCOL_ERROR`. Die Route
benötigt keinen Store. Sie startet keinen Worker und keinen Adapter: kein Replay,
kein Repair, keine Publikation und kein Schreiben oder Entfernen von Dateien.
Dateizugriff kann systembedingt `atime` ändern; Inhalt, Mode und mtime bleiben
unverändert. Bestehende V1.3-/V1.4-Writer und Berichte bleiben unverändert.
Execute-Reports werden hier abgewiesen; `inspect` behält seinen Execute-Vertrag.

| Prüfergebnis | Bedeutung | Exit |
| --- | --- | --- |
| `VALID` | Vollständiger, lokal konsistenter Lauf; auch bei `run_status=FAILED` | Exit 0 |
| `INCOMPLETE` | Konsistente, vollständige JSONL-Zeilen ohne `RUN_FINISHED`; `run_status=INCOMPLETE` | Exit 1 |
| `PROTOCOL_ERROR` | Ungültige Argumente, Trunkierung, Integritäts-/Dateisicherheitsfehler, Mutation oder Prüffristablauf | Exit 2 |

`VALID` bedeutet keine erfolgreiche fachliche Ausführung. Für deren Ergebnis
ist `run_status` maßgeblich. Auch ein belegter Preflightfehler mit reservierter,
ungestarteter Attempt-ID oder ein Host-Publikationsfehler nach READY kann lokal
konsistent sein. Der Inspector gleicht Report, Log und History ab und prüft die
Request- und lokalen Outputdigests, auch bei nicht akzeptiertem Workeroutput.

`--stream-v15` ist eine **Prüfanforderung**, kein rückwirkender Modusnachweis.
Ohne Option lautet `stream_check=NOT_REQUESTED`; private Eingaben werden nicht
untersucht. Mit Option lautet es `VERIFIED_PRESENT`: vorhandene Streams samt
`input.bin` werden streng geprüft, auch wenn null Streams erhalten sind.
Legacy-Eingabedateien anstelle des Streamlayouts, Lücken oder Stagingreste sind
Fehler und werden nicht bereinigt. Ein vollständig entfernter Streambaum kann
null verifizierte Streams ergeben. Es gibt **keine historische Vollständigkeit**:
`input_completeness=NOT_PROVABLE` und `external_provenance=NOT_CHECKED` stehen
immer im Ergebnis. Digestfelder ersetzen keinen gespeicherten ursprünglichen
Inputindex, Sidecar oder Herkunftsnachweis. Auch nachträglich eingefügte,
in sich konsistente Streams beweisen keine ursprüngliche Verwendung.

Die Prüfung ist auf **30.000 ms** ab Einstieg begrenzt; ein bereits laufender
Kernelzugriff kann nicht unterbrochen werden. JSON-Tiefe höchstens 64, Retry
0..3, Workers 1..4, höchstens 15 Inputs je Attempt, höchstens 1.048.576 Bytes
je Input und in Summe höchstens das Run-Inputbudget. Bounded Lesepuffer sind
höchstens 65.536 Bytes groß. Descriptor-relative `O_NOFOLLOW`-Zugriffe und
Vorher-/Nachher-Abgleich erkennen unsichere Dateien und beobachtete Mutation;
das ist kein unveränderlicher Dateisystemsnapshot.

Ein normaler lokaler Ablauf mit einer vorhandenen V3-Spec und einem bereits
initialisierten lokalen Artifact Store `local-store` ist (Storeinitialisierung
siehe `artifact-store-v1 import FILE --store STORE`):

```sh
bruce dag-scheduler-v5 payload graph.spec --output payload.bundle --store local-store --stream-v15
bruce dag-scheduler-v5 payload-inspect payload.bundle
bruce dag-scheduler-v5 payload-inspect payload.bundle --stream-v15
```

Bei einem fachlich gescheiterten ersten Befehl trotzdem den vorhandenen Bundle
prüfen; Inspektions-Exit 0 kann dann `run_status=FAILED` melden. Ein Fehler der
Prüfung liefert ausschließlich `{"error":"PROTOCOL_ERROR"}` plus LF auf stderr,
Exit 2 und leeres stdout. Sonst erscheint genau eine metadata-only JSON-Zeile
auf stdout und kein stderr. Payloadbytes/Hex, Digests, private Pfade,
Streamnamen, Manifest-IDs und OS-Diagnosen werden nicht ausgegeben.
Das genaue Format steht im [Formatvertrag](formats.md#payload-run-inspector-v1).
Belege: [Kern](../../src/artifact/payload_run_inspector_v1.cpp),
[Streamprüfung](../../src/artifact/payload_run_stream_inspector_v1.cpp) und
[reale Workflows](../../tests/integration/test_dag_scheduler_v5_payload_inspector.py).
Recovery, Resume, ein authentifizierter historischer Inputindex und übergreifende
Runtime-Orchestrierung bleiben **DOKUMENTIERT / KONZEPT**.

### V5-Payload-Flow V2 – persistierte Kanten-Lineage

**IMPLEMENTIERT UND BELEGT:** Der opt-in-Modus ergänzt den bestehenden
V1.3-Payloadlauf um ein lokales, kanonisches Sidecar mit der tatsächlichen
Producer-/Consumer-Lineage:

```text
bruce dag-scheduler-v5 payload SPEC --output BUNDLE --store STORE --flow-v2
bruce dag-scheduler-v5 payload SPEC --output BUNDLE --store STORE --flow-v2 --stream-v15
bruce dag-scheduler-v5 payload-inspect BUNDLE --flow-v2
```

`--flow-v2` ist genau einmal erlaubt und nur in diesen beiden Routen gültig.
`--external-inputs` wird zusammen mit Flow V2 sofort nach dem CLI-Parse mit
`FLOW_V2_EXTERNAL_INPUTS_UNSUPPORTED` abgewiesen, bevor Spec, Store, Sidecar
oder Runroot gelesen beziehungsweise angelegt werden. Ohne `--flow-v2` bleiben
V1.3, V1.4, V1.5 sowie alle bestehenden Reportbytes unverändert.

Nach einem abgeschlossenen Legacy-Report wird zusätzlich
`BUNDLE.flow-v2.json` mit Mode 0600 atomar angelegt. Die feste Schlüsselreihenfolge
lautet `contract`, `run_id`, `spec_digest`, `report_digest`, `stream_v15`,
`nodes`, `attempts`; die Lineage-Einträge enthalten nur Node-, Attempt-,
Manifest-, Digest-, Größen- und Indexmetadaten. Payloadbytes, Pfade, PIDs,
Kommandozeilen und Betriebssystemdiagnosen werden nicht gespeichert. Root-
Attempts haben `inputs:[]`; `ATTEMPT_STARTED` bestimmt die veröffentlichte Reihenfolge; Consumerinputs folgen exakt der Spec-
Dependency-Reihenfolge. Gestartete Fehlversuche und Retries bleiben als eigene
Attempts erhalten.

Der Flow-Inspector ist read-only und liefert ausschließlich metadata-only Metadaten; er öffnet keinen Store und startet keinen Worker:

```json
{"contract":"dag-scheduler-v5-payload-flow-inspect-v2","action":"payload-inspect","status":"VALID","run_status":"READY","run_id":"example","node_count":2,"attempt_count":2,"binding_count":1,"verified_input_bytes":1,"stream_v15":false,"input_completeness":"VERIFIED","provenance_scope":"LOCAL_CONSISTENCY"}
```

`VALID` endet mit Exit 0, ein konsistenter noch nicht beendeter Lauf mit
`INCOMPLETE` und Exit 1. JSON-, Digest-, Lineage-, Dateisicherheits- oder
Mutationsfehler liefern ausschließlich `{"error":"PROTOCOL_ERROR"}` plus LF
auf stderr und Exit 2. Sidecar-Encode-, Größen-, Schreib- oder
Adoptionsfehler beim Payloadlauf liefern `{"error":"FLOW_RECORD_INVALID"}`
plus LF und Exit 2; stdout bleibt leer. Der Inspector prüft Report, Log,
Requests, Outputs und beide privaten V1.3-/V1.5-Inputlayouts gegen das
Sidecar. Die Spezifikation wird nicht erneut aus einem externen Store geladen;
`provenance_scope=LOCAL_CONSISTENCY` ist keine Authentizitätsaussage.

Belege: [Flow-V2-Modell](../../include/bruce/artifact/payload_flow_v2.hpp),
[Sidecar-Writer](../../src/artifact/payload_flow_v2.cpp),
[Flow-Inspector](../../src/artifact/payload_flow_inspector_v2.cpp),
[Payload-Workflows](../../tests/integration/test_dag_scheduler_v5_payload.py),
[Inspector-Workflows](../../tests/integration/test_dag_scheduler_v5_payload_flow_inspector.py).
Freie externe Stores, Netzwerk-/GPU-Ausführung, freie Code-Plugins, freie
Sprache, allgemeines Beweisen und große Runtime-Orchestrierung bleiben
**DOKUMENTIERT / KONZEPT**.

## `artifact-store-v1` – lokaler unveränderlicher Artifact Store

**IMPLEMENTIERT UND BELEGT:** Der Artifact Store V1 ist ein separater lokaler
Linux-Speicherpfad für immutable, content-addressed Objekte. Die öffentliche
CLI-Oberfläche umfasst vier bewusst getrennte Aufrufe:

```text
bruce artifact-store-v1 import FILE --store STORE
bruce artifact-store-v1 inspect DIGEST --store STORE
bruce artifact-store-v1 verify DIGEST --store STORE
bruce artifact-store-v1 resolve MANIFEST_ID --store STORE
```

`import` liest genau eine reguläre Quelldatei und publiziert deren SHA-256-
adressiertes Objekt. `inspect` und `verify` lesen ausschließlich das bereits
unter `DIGEST` adressierte Objekt. `resolve` liest ein bereits publiziertes,
kanonisches Manifest und verifiziert jedes darin referenzierte Objekt vollständig.
Die drei Objektaufrufe liefern bei Erfolg einen
metadata-only Metadatenbericht mit dem Vertrag `artifact-store-v1`, den Feldern `action`,
`status`, `sha256` und `size_bytes`; Payloadbytes, Quellpfade und Storepfade
werden nie ausgegeben. `inspect` prüft die Digest-/Größenbindung vollständig;
`verify` führt dieselbe Integritätsprüfung ohne Payload-Ausgabe aus.

### Kontrollierte Manifestauflösung V1.2

**IMPLEMENTIERT UND BELEGT:** Die Resolve-Route hat exakt diese Syntax und
akzeptiert ausschließlich die Option `--store`:

```text
bruce artifact-store-v1 resolve MANIFEST_ID --store STORE
```

`MANIFEST_ID` ist ein einzelner opaker Token nach der bestehenden ASCII-
Grammatik. Er wird nicht als Dateipfad interpretiert; die Route öffnet nur das
descriptor-relative Manifest `manifests/<MANIFEST_ID>.manifest`. Datei-
Substitutionen, absolute oder pfadartige IDs, URLs, Provider- und Download-
Optionen sowie Materialisierungs- oder V5-Optionen werden abgewiesen, bevor
der Store geöffnet wird.

Nach erfolgreicher kanonischer Manifestprüfung werden alle referenzierten
Objekte in der gespeicherten Reihenfolge vollständig auf exakten Digest und
exakte Bytegröße geprüft. Erst danach entsteht genau ein metadata-only Bericht
mit abschließendem LF:

```json
{"contract":"artifact-store-resolve-v1","action":"resolve","status":"RESOLVED","manifest_id":"v5-abc123","run_id":"run-123","node_id":"a","attempt_id":"a-1","manifest_status":"READY","result_code":"OK","objects":[{"sha256":"<64-hex>","size_bytes":12,"origin":"artifact/a"}]}
```

Die feste Feldreihenfolge lautet `contract`, `action`, `status`,
`manifest_id`, `run_id`, `node_id`, `attempt_id`, `manifest_status`,
`result_code`, `objects`. Objektfelder folgen je Eintrag als `sha256`,
`size_bytes`, `origin`; die Manifestreihenfolge bleibt erhalten.
`manifest_status` ist der gespeicherte Manifeststatus, während `status` den
Resolve-Erfolg `RESOLVED` beschreibt. Ein leerer `origin` bleibt `""` und ein
Manifest ohne Objekte liefert exakt `"objects":[]`; `media_type` wird nicht
öffentlich ausgegeben.

Resolve ist read-only. Es materialisiert keine Eingabe, gibt keine Payloadbytes
oder private Pfade aus, schreibt keinen Cache oder Index, startet keinen
Prozess und verwendet keinen Netzwerk- oder externen Providerpfad. Bei jeder
Syntax-, Store-, Manifest-, Rechte-, Größen- oder Integritätsverletzung wird
der vollständige Bericht verworfen und exakt diese Zeile auf stderr mit Exit 2
geschrieben; stdout bleibt leer:

```json
{"contract":"artifact-store-resolve-v1","error":"PROTOCOL_ERROR"}
```

Die bestehenden `import`, `inspect` und `verify`-Verträge bleiben unverändert.
Die Route führt kein neues Zeit-, CPU-, Speicher- oder I/O-Diagnosebudget ein;
die vorhandene bounded 32-MiB-Grenze bleibt maßgeblich.

Der Objektgrenzwert ist **32 MiB = 33.554.432 Bytes** und wird vor einer
unbegrenzten Kopie beziehungsweise Allokation geprüft. Das Store-Layout ist
`STORE/objects/aa/<sha256>.blob`, ergänzt um `STORE/store.lock` und den
Manifestbereich `STORE/manifests/`. Eine doppelte Publikation desselben
Digest-/Größenpaares ist idempotent und überschreibt das vorhandene Objekt
nicht. Die Veröffentlichung verwendet private Dateien, Streaming-SHA-256,
`fsync` und exklusive atomare Publikation.

### Fehler- und Sicherheitsmatrix

| Fall | stdout | stderr | Exit |
| --- | --- | --- | ---: |
| gültiger Import, Inspect, Verify oder Resolve | kanonischer Metadatenbericht mit LF | leer | Exit 0 |
| ungültige Syntax, Digest, Datei, Store, Rechte, Symlink, Hardlink, Größe, Digest oder Korruption | leer | `{"contract":"artifact-store-v1","error":"PROTOCOL_ERROR"}` mit LF | Exit 2 |
| ungültige Resolve-Syntax, Manifest-, Objekt- oder Integritätsprüfung | leer | `{"contract":"artifact-store-resolve-v1","error":"PROTOCOL_ERROR"}` mit LF | Exit 2 |

Der CLI-Fehler ist absichtlich kompakt und enthält keine Betriebssystemtexte,
Pfade oder geheimen Daten. Store, `objects/`, Präfixverzeichnisse,
Manifestdateien und Objekte werden descriptor-relativ mit `O_NOFOLLOW`
geöffnet; Quellen müssen regulär, owner-only und nicht hardlinked sein. Eine
Quelle oder ein Ziel außerhalb der erlaubten Verzeichnisgrenze, ein Symlink,
ein nichtreguläres Objekt, ein Objekt mit falschen Rechten oder eine
Manipulation führt kontrolliert zu `PROTOCOL_ERROR`. Fehlgeschlagene
Publikationen lassen kein partielles Objekt am Ziel zurück.

### Manifest- und Nichtzielgrenze

**IMPLEMENTIERT UND BELEGT:** Die Bibliotheks-API bietet zusätzlich einen
kanonischen `BRUCE_ARTIFACT_MANIFEST_V1`-Codec. Ein Manifest bindet `run_id`,
`node_id`, `attempt_id`, `status`, `result_code` und bis zu 1024 Objekte mit
`sha256`, `size_bytes`, `media_type` und `origin`. Es enthält nur Metadaten;
Payloadbytes, absolute Pfade, unbekannte Felder und zusätzliche Bytes werden
abgewiesen. Der Manifestbereich ist ebenfalls exklusiv und atomar.

V5 `execute` integriert den Store weiterhin nicht automatisch; die oben
beschriebene Publish-Route ist ein separater expliziter Schritt. Payload-Download,
Garbage-Collection, Netzwerktransport, externe Provider, Remote
Stores und allgemeine Runtime-Orchestrierung sind **DOKUMENTIERT / KONZEPT**.
Der Store ist keine Verschlüsselung, kein
Authentifizierungs- oder Berechtigungssystem und kein freier Plugin-Loader.

Belege: [Store-API](../../include/bruce/artifact/artifact_store_v1.hpp),
[Manifest-API](../../include/bruce/artifact/artifact_manifest_v1.hpp),
[CLI](../../apps/artifact_store_v1_cli.hpp),
[Store-Verträge](../../tests/unit/artifact_store_v1_tests.cpp),
[CLI-Verträge](../../tests/unit/artifact_store_v1_cli_tests.cpp),
[Resolve-Workflow](../../tests/integration/test_artifact_store_resolve_v1.py) und
[reale Workflows](../../tests/integration/test_artifact_store_v1.py).

## `artifact-store-v2` – kontrollierter Dateisystem-Mirror-Fetch

**IMPLEMENTIERT UND BELEGT:** V2 materialisiert genau ein vorher durch Digest
und Größe identifiziertes Mirror-Objekt in den bestehenden lokalen V1-Store:

```text
bruce artifact-store-v2 fetch SHA256 SIZE --store LOCAL_STORE --provider PROVIDER_ID --providers PROVIDERS.json
```

Dies ist die einzige V2-Route; Aktion, Argumentanzahl und Optionsreihenfolge
sind fest. `SHA256` umfasst genau 64 kleingeschriebene Hexzeichen. `SIZE` ist
eine Dezimalzahl ohne Vorzeichen oder führende Nullen außer `0`, höchstens
**33.554.432 Bytes (32 MiB)**. Provider-IDs sind 1–128 ASCII-Zeichen, beginnen
alphanumerisch und enthalten danach nur alphanumerische Zeichen, `.`, `_`
oder `-`. Pfadwerte sind nicht leer und beginnen nicht mit `-`. Fehlende,
doppelte, zusätzliche oder umgeordnete Argumente werden vor Dateizugriffen
abgewiesen.

Die ausdrücklich ausgewählte Datei `PROVIDERS.json` ist owner-only, regulär,
im Besitz der effektiven UID, genau einmal hardlinked und ohne Symlink im
Öffnungspfad. Ihr [kanonisches Format](formats.md#artifact-store-v2-providerdatei-und-fetch-bericht)
enthält nur benannte absolute Dateisystem-Roots, keine URLs oder Credentials:

```json
{"contract":"bruce-artifact-store-providers-v2","providers":[{"id":"mirror-a","root":"/absolute/root"}]}
```

Die Datei endet mit genau einem LF. Die Feldreihenfolge ist fest; es gibt
keine sonstigen Whitespaces. Provider sind nach eindeutiger ID streng
aufsteigend sortiert. Das Lesen der Konfiguration öffnet weder Mirror noch
Zielstore; erst der ausgewählte Provider wird beim Fetch geöffnet.

V2 liest ausschließlich `objects/<erste-zwei-hex>/<sha256>.blob` unter dem
konfigurierten Root. Root, `objects/` und Präfixverzeichnis müssen owner-only
und im Besitz der effektiven UID sein; alle Pfadkomponenten sind symlinkfrei.
Das Objekt muss regulär, owner-only, im Besitz derselben UID und genau einmal
hardlinked sein. Gemeinsame Vorfahren wie `/tmp` dürfen geteilt sein.
Descriptor-relative Öffnungen, Prüfungen des geöffneten Inodes und begrenztes
Streaming in private lokale Staging-Dateien prüfen exakte Größe, SHA-256 und
stabile Quellmetadaten. Erst danach wird der Zielstore geöffnet und die
unveränderte atomare/idempotente V1-Publikation aufgerufen.

Erfolg ist genau diese Feldreihenfolge mit abschließendem LF auf stdout,
leerem stderr und Exit 0; `<64-hex>` steht hier für den erwarteten Digest:

```json
{"contract":"artifact-store-fetch-v2","action":"fetch","status":"FETCHED","provider":"mirror-a","sha256":"<64-hex>","size_bytes":123}
```

Wiederholter Fetch korrekter identischer Bytes liefert denselben Bericht und
überschreibt das bestehende Objekt nicht. Jeder Fehler liefert leeres stdout,
Exit 2 und ausschließlich diese Zeile mit LF auf stderr:

```json
{"contract":"artifact-store-fetch-v2","error":"PROTOCOL_ERROR"}
```

Berichte enthalten keine Payloadbytes, privaten Pfade oder OS-Diagnosen.
Digest-/Größenfehler vor Publikation erzeugen kein neues lokales Objekt;
bestehende korrekte oder konfliktbehaftete Objekte werden nicht überschrieben.
Scheitert V1-Verzeichnissynchronisierung nach atomarer Publikation, gilt ein
frisch vollständig verifiziertes Zielobjekt als erfolgreicher Fetch. Scheitert
auch diese Prüfung durch einen weiteren I/O-Fehler, bleibt `PROTOCOL_ERROR`;
dann ist die Sichtbarkeit eines bereits publizierten vollständigen Objekts
nicht ausgeschlossen. Es gibt keinen Rollback und keine zusätzliche
Crash-Dauerhaftigkeitsgarantie. Staging wird bei normaler Rückkehr und
Exceptions bereinigt, nicht garantiert bei abruptem Prozessende.

V2 publiziert kein Manifest. Bereits vorhandene lokale Manifeste können das
gefetchte Objekt danach über V1.2 auflösen und V1.3/V1.4 lokal nutzen, auch
ohne Mirror-Kopie. V1/V1.2-Syntax und Ausgabebytes sowie Workerframes und
Bundleformate bleiben unverändert. Integrität und lokale Besitzprüfungen
bieten keine Verschlüsselung, entfernte Authentisierung/Autorisierung oder
Sicherheitsgarantie gegen einen bösartigen Prozess derselben UID.

V2 besitzt keinen Netzwerkprovider; der separate
[V3-HTTPS-Fetch](#artifact-store-v3--kontrollierter-https-fetch) ist unten beschrieben.
**DOKUMENTIERT / KONZEPT:** S3/Redis/Datenbank- oder weitere
Netzwerkprovider, Remote-Publish/-Delete/GC/Retention, Synchronisierung,
Caching, automatische DAG-Nachladung und direkte Worker-Netzwerkzugriffe.
V2 akzeptiert keine freien Quellpfade, URLs, Hosts, Ports, Header,
Credentials oder Code-Plugins und erweitert weder GPU-Ausführung, freie
Sprache, allgemeines Beweisen noch Runtime-Orchestrierung.
Der unveränderte CLI-Beispielgenerator führt V2 nicht aus; V2-Evidenz liefern
die [Core-Verträge](../../tests/unit/artifact_store_v2_tests.cpp),
[CLI-Verträge](../../tests/unit/artifact_store_v2_cli_tests.cpp) und
[realen Fetch-/Kompatibilitätsworkflows](../../tests/integration/test_artifact_store_v2.py).
Quellen: [Core](../../src/artifact/artifact_store_v2.cpp) und
[geschlossene CLI](../../apps/artifact_store_v2_cli.hpp).

## `artifact-store-v3` – kontrollierter HTTPS-Fetch

**IMPLEMENTIERT UND BELEGT:** V3 lädt genau ein über SHA-256 und exakte Größe
identifiziertes Objekt aus einem ausdrücklich ausgewählten HTTPS-Provider in
den bestehenden lokalen V1-Store:

```text
bruce artifact-store-v3 fetch SHA256 SIZE --store LOCAL_STORE --provider PROVIDER_ID --providers PROVIDERS.json
```

Dies ist die einzige V3-Route. Aktion, Argumentanzahl und Optionsreihenfolge
sind fest. `SHA256` besteht aus 64 lowercase Hexzeichen; `SIZE` ist eine
Dezimalzahl ohne Vorzeichen und führende Nullen außer `0`, höchstens
**33.554.432 Bytes (32 MiB)**. IDs erfüllen
`[A-Za-z0-9][A-Za-z0-9._-]{0,127}`. Pfadwerte sind nicht leer und beginnen
nicht mit `-`; NUL ist unzulässig. Fehlende, zusätzliche, doppelte oder
umgeordnete Argumente werden vor Datei- und Netzwerkzugriffen abgewiesen.

Die explizite owner-only Providerdatei muss regulär, im Besitz der effektiven
UID, genau einmal hardlinked und über einen symlinkfreien Öffnungspfad
erreichbar sein. Ihr [kanonisches Format](formats.md#artifact-store-v3-https-providerdatei-und-fetch-bericht)
enthält nur sortierte eindeutige IDs und HTTPS-Origins:

```json
{"contract":"bruce-artifact-store-providers-v3","providers":[{"id":"https-a","origin":"https://artifacts.example.org:443/bruce"}]}
```

Genau ein LF beendet die Datei. Der einzige abgeleitete Request ist
`GET <origin>/objects/<erste-zwei-hex>/<sha256>.blob`. V3 akzeptiert keine
freie URL, Caller-Header, Cookies, Credentials, Proxys oder alternative
Providerroute. Zertifikats- und Hostnamenprüfung sind zwingend aktiv; der
OpenSSL-Standardvertrauensspeicher einschließlich seiner Prozessumgebung
gilt. Es gibt keine V3-Option für eine CA-Datei oder einen Prüfungs-Bypass.
Nur HTTP 200 ist zulässig; Redirects und Wiederholungen sind ausgeschlossen.
Die erste aufgelöste Adresse muss erreichbar sein: V3 versucht keine weitere
DNS-Adresse, wenn die erste Verbindung scheitert. Dies ist kein DNS-Pinning.

Ein überwachter Linux-Hilfsprozess übernimmt DNS, TLS und HTTP. Er schließt
geerbte Deskriptoren mittels `close_range` bis auf seinen privaten IPC-Kanal;
scheitert das, bricht der Fetch ab. Nur der Elternprozess greift auf den
Store zu. Er erzwingt monotone absolute Fristen: **5 Sekunden** ab
Transportbeginn für DNS, Verbindung und TLS-Authentisierung sowie
**30 Sekunden insgesamt** für den Transport einschließlich Antwortframing,
TLS-Abbau und sauberem Kindprozessende. Die 30 Sekunden beginnen nicht nach
der Verbindung erneut. Bei Fehler oder Fristablauf beendet er den
Kindprozess und wartet auf dessen Ende (`waitpid`). Das ist keine Frist für
Konfigurationslesen oder lokale V1-Publikation und keine Isolation gegen
einen bösartigen Prozess derselben UID.

Antwortmetadaten unterliegen den vendorten Headergrenzen (8 KiB je
akzeptierter Headerzeile, 100 Headerfelder, 32-KiB-Zeilenlesegrenze).
`Content-Encoding` und doppelte `Content-Length` werden abgewiesen; es gibt
keine Dekompression. IPC-Blöcke sind höchstens 64 KiB groß; Antwortdaten
sind auf 32 MiB begrenzt. Der Elternprozess puffert höchstens die erwartete
Objektgröße und prüft vollständigen Transport, exakte Länge und SHA-256,
bevor er den Zielstore öffnet und atomar über V1 publiziert.

Erfolg: Exit 0, leeres stderr und exakt diese Feldreihenfolge mit LF auf stdout:

```json
{"contract":"artifact-store-fetch-v3","action":"fetch","status":"FETCHED","provider":"https-a","sha256":"<64-hex>","size_bytes":123}
```

Jeder Fehler: Exit 2, leeres stdout und ausschließlich diese Zeile mit LF auf stderr:

```json
{"contract":"artifact-store-fetch-v3","error":"PROTOCOL_ERROR"}
```

Es erscheinen keine Payloadbytes, Pfade, Origins, Hostnamen, TLS-/OS-Diagnosen
oder Teilresultate. Fehler vor Publikation erzeugen kein Objekt. Repetition
ist idempotent und überschreibt keine konfliktbehafteten Objekte. Bei einem
V1-Post-Rename-Synchronisierungsfehler zählt nur eine frische vollständige
V1-Verifikation als Erfolg; bei weiterem I/O-Fehler kann `PROTOCOL_ERROR`
trotz eines sichtbaren vollständigen Objekts auftreten. Es gibt keinen
Rollback oder stärkeren Crash-Dauerhaftigkeitsvertrag.

V3 publiziert kein Manifest. Bestehende lokale Manifeste und V1.2/V1.4
verwenden danach ausschließlich die lokale Kopie; Worker holen keine
Netzwerkdaten. V1/V1.2/V2-Ausgabebytes, V5-, Worker- und Bundleverträge
bleiben unverändert. Nicht Teil von V3 sind Authentisierung mit Credentials,
Tokens, mTLS, Proxy, Redirects, Retries, DNS-Pinning, Cache, Sync,
Remote-Publish/-Delete/GC/Retention, automatische DAG-Nachladung, direkte
Worker-Netzwerkzugriffe, GPU, Plugins, freie Sprache, allgemeines Beweisen
oder Runtime-Orchestrierung.

Belege: [Core](../../src/artifact/artifact_store_v3.cpp),
[CLI](../../apps/artifact_store_v3_cli.hpp),
[Core-Verträge](../../tests/unit/artifact_store_v3_tests.cpp),
[CLI-Verträge](../../tests/unit/artifact_store_v3_cli_tests.cpp) und
[lokale TLS-/Kompatibilitätsworkflows](../../tests/integration/test_artifact_store_v3.py).
Der unveränderte CLI-Beispielgenerator führt V3 nicht aus. Die Tests nutzen
temporäre lokale TLS-Zertifikate; sie belegen keinen öffentlichen Providerbetrieb.

## `artifact-store-v4` – hostseitiger HTTPS-Mirror und Restore

**IMPLEMENTIERT UND BELEGT:** V4 spiegelt ein vollständiges lokales
`READY`-Manifest mit allen Objekten oder stellt es in einem lokalen V1-Store
wieder her. Beide Routen sind explizite Hostoperationen:

```text
bruce artifact-store-v4 mirror MANIFEST_ID --store LOCAL_STORE --remote HTTPS_BASE
bruce artifact-store-v4 restore MANIFEST_ID --store LOCAL_STORE --remote HTTPS_BASE
```

Aktion, Argumentanzahl und Optionsreihenfolge sind fest. Manifest-IDs erfüllen
`[A-Za-z0-9][A-Za-z0-9._-]{0,127}`; auch `a..b` ist eine zulässige ID.
Storepfade sind nicht leer, beginnen nicht mit `-` und enthalten kein NUL.
`HTTPS_BASE` folgt der [beschränkten V3-Origin-Grammatik](formats.md#artifact-store-v3-https-providerdatei-und-fetch-bericht):
HTTPS, lowercase DNS oder kanonisches IPv4, optionaler Port und beschränkter
Pfadpräfix; keine Credentials, Query, Fragment oder freie Header. V4 benötigt
keine Providerdatei. Die gesamte Syntax wird vor Datei-/Netzwerkzugriff geprüft.

`mirror` öffnet einen bestehenden lokalen Store, prüft das kanonische
READY-Manifest sowie Größe und SHA-256 sämtlicher Objekte und erstellt
verifizierte lokale Snapshots vor dem ersten Upload. Es sendet pro
Manifestobjekt einen `PUT <base>/objects/<erste-zwei-hex>/<sha256>.blob`;
zuletzt folgt `PUT <base>/manifests/<MANIFEST_ID>.mirror` mit dem
[gebundenen V4-Envelope](formats.md#artifact-store-v4-mirror-envelope-und-bericht).
Lokale Storebytes werden nicht geändert. Ein fehlgeschlagener Upload kann
bereits geschriebene Remotedaten hinterlassen; es gibt keinen Remote-Rollback
oder Nachweis dauerhafter Remotespeicherung.

`restore` verlangt ein fehlendes lokales Manifest. Ein vorhandenes Manifest
wird vor dem ersten Request abgewiesen. Zuerst lädt es das Envelope per GET,
prüft die angeforderte Manifest-ID, Manifest-SHA-256, exakte Länge, kanonisches
V1-Format und `READY`, dann alle gebundenen Objekte über dieselben Objektpfade.
Alle Remoteinhalte werden vollständig geprüft und gepuffert, bevor lokale
Publikation beginnt. Vorhandene Objekte müssen zur Identität passen und
werden nie ersetzt; korrupte lokale Einträge führen zum Fehler. Objekte
werden über V1 publiziert, das Manifest zuletzt. Ein neuer Store kann dabei
angelegt werden. Nach Restore arbeiten Resolve und Worker ausschließlich
mit den lokalen Kopien, auch bei abgeschaltetem Remote.

Das Envelope ist auf **1 MiB (1.048.576 Bytes)** einschließlich seiner
Bindungszeilen begrenzt. Die Summe aller referenzierten Objektgrößen ist
höchstens **32 MiB (33.554.432 Bytes)**; wiederholte Referenzen zählen erneut.
PUT-Antwortkörper sind höchstens **64 KiB (65.536 Bytes)** groß. GET verlangt
HTTP 200; PUT akzeptiert 200, 201 und 204. Eine 204-Antwort ohne
`Content-Length` ist zulässig, ebenso mit genau `Content-Length: 0`.
204 mit Transfer-Encoding oder anderer deklarierter Länge, doppelte
`Content-Length` und `Content-Encoding` scheitern. Redirects und Retries
sind ausgeschlossen.

Der überwachte Linux-Transport übernimmt die V3-TLS-Prüfungen, den
OpenSSL-Standardvertrauensspeicher, begrenzte IPC-Blöcke und die erste
DNS-Adresse ohne Fallback. Jede Verbindung hat höchstens **5 Sekunden**
für DNS, Verbindung und TLS; alle Requests teilen eine absolute
**30-Sekunden-Frist** ab Beginn der Mirror-/Restore-Operation. Lokale
Dateisystemaufrufe sind dadurch keine präemptiv unterbrechbaren Operationen.
Transportfehler hinterlassen beim Restore vor Publikation keinen neuen Store.
Fehler während lokaler Publikation können bereits verifizierte Objekte oder
ein vollständiges Manifest hinterlassen. Die V1-Sync-Recovery verlangt eine
frische Verifikation; ein weiterer I/O-Fehler kann trotzdem `PROTOCOL_ERROR`
liefern. Ein transaktionaler Rollback aller lokalen Einträge ist nicht zugesagt.

Erfolg: Exit 0, leeres stderr und exakt diese Feldreihenfolge mit LF auf stdout;
Restore verwendet `"action":"restore","status":"RESTORED"`:

```json
{"contract":"artifact-store-mirror-v4","action":"mirror","status":"MIRRORED","manifest_id":"example","manifest_sha256":"<64-hex>","manifest_size_bytes":312,"object_count":1,"total_object_bytes":23}
```

Jeder Fehler: Exit 2, leeres stdout und exakt diese Zeile mit LF auf stderr:

```json
{"contract":"artifact-store-mirror-v4","error":"PROTOCOL_ERROR"}
```

Berichte enthalten ausschließlich Metadaten: keine Payloadbytes, lokalen
Pfade, Remote-URLs, Hostnamen, Credentials oder TLS-/OS-Diagnosen. Workerframes,
Records und bestehende V1/V2/V3-Verträge erhalten keine Remoteparameter.
Automatische DAG-Synchronisation, Credential-Speicherung, Remote-Löschung,
Multi-Writer-Konfliktauflösung, Peer-to-Peer und Remote-Replay sind nicht Teil
von V4. Die Integritätsbindung authentisiert keinen entfernten Autor.

Belege: [Core](../../src/artifact/artifact_store_v4.cpp),
[HTTPS-Transport](../../src/artifact/artifact_store_v3.cpp),
[CLI](../../apps/artifact_store_v4_cli.hpp),
[Core-Verträge](../../tests/unit/artifact_store_v4_tests.cpp) und
[lokale HTTPS-Workflows](../../tests/integration/test_artifact_store_v4.py).
Die Tests belegen temporäre Loopback-TLS-Peers einschließlich 204 ohne
Content-Length; keinen öffentlichen Providerbetrieb. Der unveränderte
CLI-Beispielgenerator führt V4 nicht aus.

## `dag-scheduler-v4` – kanonische lokale Bundles und Replay

**IMPLEMENTIERT UND BELEGT:** V4 ergänzt den V3-Dateiadapter um drei feste
Routen über dem unveränderten V2-Kern, auch mit NanoCell und MCP OFF:

```text
bruce dag-scheduler-v4 execute SPEC --output BUNDLE
bruce dag-scheduler-v4 inspect BUNDLE
bruce dag-scheduler-v4 replay BUNDLE
```

`SPEC` ist die lokale [V3-Spec-Datei](formats.md#dag-scheduler-v3-spec-datei).
Nur `literal-v1`, `append-v1` und `join-v1` sind zulässig; V3-Parser,
Callback-Arity, serielle V2-Reihenfolge und Budgets bleiben gültig.
`execute` verlangt genau diese Argumentreihenfolge und `--output`; die
Leser akzeptieren keine Optionen. Pfade sind nicht leer und beginnen nicht
mit `-`; Standardinput und zusätzliche Parameter sind nicht unterstützt.

### Veröffentlichung und Berichte

`execute` validiert und führt aus, kodiert das Bundle und veröffentlicht es
über [LocalFiles](local-files.md) atomar und exklusiv. Ein vorhandenes Ziel,
auch ein Symlink, wird nicht überschrieben. Das Zielverzeichnis muss bestehen.
Neue Bundledateien sind privat (Modus 0600); Erfolg schreibt stdout erst nach
erfolgreicher Veröffentlichung. Bei konkurrierenden Exporten desselben
Ziels gewinnt höchstens einer. `inspect` und `replay` schreiben keine Datei.

Erfolg liefert Exit 0, leeres stderr und einen JSON-Bericht mit finalem LF.
Auch gültige Startbudget-Cancellation bleibt ein vollständig berichteter
V2-Lauf mit terminalen Receipts. Alle drei Berichte enthalten
keine rohen Request- oder Payload-Bytes und keine Hex-Kodierungen dieser Daten.

| Feld | Inhalt |
| --- | --- |
| `contract` | `dag-scheduler-v4-execute-v1`, `dag-scheduler-v4-inspect-v1` oder `dag-scheduler-v4-replay-v1` |
| `action` | `execute`, `inspect` oder `replay` |
| `bundle_digest` | SHA-256 des kanonischen Bundle-Payloads, aus der zweiten Headerzeile; kein Hash der gesamten Datei |
| `scheduler_version` | `dag-scheduler-v2` |
| `dag_id`, `max_started_nodes`, `max_total_receipt_bytes` | Dekodierte V2-Spec und Budgets |
| `receipts` | Nach Knoten-ID sortierte V2-Receipts mit Callback-ID, Requestgröße/-Digest, direkten Input-Metadaten, Outcome, Fehlercode und Output-Metadaten |
| `trace` | Chronologische logische V2-Ereignisse, einschließlich `VALIDATED` und `COMPLETED` |

Die Receipt-, Input- und Output-Felder entsprechen dem
[V3-Execute-Bericht](#execute-metadatenbericht); `request_bytes`,
`payload_bytes` und `request-hex` erscheinen nicht. V4 berichtet keinen
`spec_digest` der ursprünglichen V3-Textdatei, da das Bundle die geparste
V2-Spec bewahrt. Ein optionales finales LF in der V3-Spec ändert den V3-
Dateidigest, aber nicht den V4-Lauf oder dessen Bundlebytes. Wiederholte
Aufrufe derselben Bundlebytes liefern je Route bytegleiche Berichte;
Vertrag und Aktion unterscheiden die Routen. V3-Execute bleibt bytegleich.

### Bundle, Inspector und Replay-Grenze

Das [kanonische Bundle](formats.md#dag-scheduler-v4-run-bundle) beginnt mit
`BRUCE_DAG_SCHEDULER_RUN_V4` und ist auf 5 MiB (5.242.880 Bytes) einschließlich
Header und Digest begrenzt. Es bindet die V2-Spec samt Requestbytes, alle
Receipts samt direkten Input- und Output-Payloadbytes und den Trace.
Die engeren V2-Grenzen bleiben bestehen: 1..64 Knoten, höchstens 15 direkte
Abhängigkeiten, Request/Output je höchstens 65.536 Bytes und ein Receipt-
Budget von 1..4.194.304 Bytes einschließlich jeder Input-Kopie. Die 5-MiB-
Dateigrenze ist kein Prozessspeicherlimit.

**Vertraulichkeitsgrenze:** Bundles enthalten Request- und Payload-Rohbytes
und sind lokale vertrauliche Artefakte. Metadatenberichte sind kein Ersatz
für geschützte Bundledateien. SHA-256 bietet keine Verschlüsselung und
keine Authentifizierung; sichtbare IDs, Größen und Digests bieten keine
Zugriffskontrolle und können ebenfalls Informationen verraten.

`inspect` prüft Header, Digest, Dekodierung und Kanonizität sowie die
strukturellen V2-/Katalogverträge. Es startet keine Callbacks und bestätigt
keine erneute Ausführung. Die berichteten Receipts und der Trace stammen
aus dem validierten Bundle. Ein kanonisch neu kodiertes Bundle mit
veränderten, strukturell zulässigen Receipts oder Trace kann daher den
Inspector passieren. Das unterscheidet V4-Bundleinspektion vom statischen
V3-Spec-Inspector, der überhaupt keine Laufreceipts enthält.

`replay` führt die eingebettete Spec erneut mit dem festen V3-Katalog über V2
aus, verlangt vollständige Gleichheit von Spec, Receipts und Trace sowie
byteidentisches erneutes Packen. Dafür ist die ursprüngliche Specdatei nicht
nötig. Erfolg bestätigt Integrität und lokale Reproduzierbarkeit unter diesem
Vertrag, kein mathematischer Beweis, keine Authentifizierung und keine Aussage
über Autor oder Wahrheit der Requestdaten. Eine bloß gültige äußere
Digestzeile ersetzt diese Prüfung nicht.

### Fehler, Nichtziele und Belege

Falsche, fehlende, zusätzliche oder optionartige Argumente liefern Exit 2,
leeres stdout und exakt diese kompakte stderr-Zeile mit finalem LF:

```json
{"error":"Usage: bruce dag-scheduler-v4 execute SPEC --output BUNDLE | inspect BUNDLE | replay BUNDLE"}
```

Datei-, Spec-, Budget-, Publikations- oder Bundlefehler liefern ebenfalls
Exit 2 ohne Erfolgsbericht. Der gemeinsame CLI-Fehlerpfad meldet etwa
`InputByteLimit`, `InvalidDagSchedulerV4Bundle` oder
`DagSchedulerV4ReplayMismatch`. Der Decoder prüft Größen, Anzahlen und
verbleibende Bytes vor Allokationen; falsche Header/Digests, abgeschnittene
Felder, nichtkanonische Längen/Ordnungen und Restbytes werden abgewiesen.
Fehlerdiagnosen geben die eingebetteten Request-/Payload-Rohbytes nicht aus.

Es gibt keine freien Callbacks oder Skripte, keine Netzwerk-Ausführung,
keine MCP-Ausführung, keine Parallelität und keine Migration alter
V3-Läufe. Die optionale read-only MCP-Anbindung prüft ausschließlich
V4-Struktur und Metadaten; siehe [MCP Inspector](#mcp-inspector). Auch externe Speicher, ein Persistenzdienst und allgemeine Runtime-
Orchestrierung bleiben **DOKUMENTIERT / KONZEPT**. Der explizite lokale
Bundleexport ist der einzige neue Speicherpfad. V2/V3 und Phase-DAG V2/V3
behalten ihre bisherigen Verträge.

Belege: [V4-Prozessworkflows](../../tests/integration/test_dag_scheduler_v4_cli.py),
[Codec-/Replay-Verträge](../../tests/unit/dag_scheduler_v4_tests.cpp) und
[Dokumentationsvertrag](../../tests/integration/test_cli_docs.py).
Der unveränderte Generator verify_examples.py führt V4 nicht aus;
`examples.json` und `validation.md` sind keine V4-Laufzeitbelege.
Generierte Artefakte ausschließlich über den Verifier in einer vollständigen
nativen temporären Kopie prüfen bzw. erzeugen. Der globale Dokumentationstest
setzt weiterhin MCP voraus und ist bei NanoCell/MCP OFF eingeschränkt.
Der sandboxierte Gesamt-CTest kann außerdem beim Socket-Anlegen im bestehenden
`mcp_inspector_workflows` scheitern; das ist getrennt vom V4-Vertrag zu
berichten.

## `dag-scheduler-v3` – begrenzter Spec-Dateiadapter

**IMPLEMENTIERT UND BELEGT:** V3 ist ein begrenzter Parser-/CLI-Adapter über V2,
kein neuer Scheduler. Genau zwei öffentliche Routen sind unbedingt verfügbar,
auch mit NanoCell und MCP OFF:

```text
bruce dag-scheduler-v3 execute SPEC
bruce dag-scheduler-v3 inspect SPEC
```

`SPEC` benennt eine lokale Datei, keinen Standardinput. Es gibt keine Optionen
oder weiteren Parameter. Format und kanonisches vierknotiges Beispiel stehen
in der [Spec-Referenz](formats.md#dag-scheduler-v3-spec-datei).

### Datei, Callbacks und V2-Ausführung

Die Datei beginnt exakt mit `BRUCE_DAG_SCHEDULER_SPEC_V3` und ist auf
65.536 Bytes einschließlich optionalem abschließendem LF begrenzt. Sie ist
ASCII ohne NUL oder CR, mit LF-only-Zeilenenden; CRLF ist ungültig.
Genau eine Leerzeile trennt die vollständigen Blöcke. Kommentare,
Leerraumtrimmung, implizite Standardwerte, unbekannte oder doppelte Schlüssel,
andere Feldreihenfolgen und nichtkanonische Knoten-/Abhängigkeitsreihenfolge
werden abgewiesen. `request-hex` ist leer oder hat eine gerade Anzahl
kleingeschriebener Hexziffern `0-9a-f`, ohne Präfix; `00` kodiert ein NUL-Byte.
Die Datei selbst darf kein NUL enthalten.

Der feste Katalog wird vor der Ausführung nach Callback-ID sortiert gebunden;
Callback-Namen und Arity werden geprüft:

| Callback | Direkte Abhängigkeiten | Ergebniscode | Berechnung |
| --- | --- | --- | --- |
| `literal-v1` | 0 | `LITERAL_READY` | Dekodierter Request |
| `append-v1` | 1 | `APPENDED_READY` | Input-Payload, dann dekodierter Request |
| `join-v1` | 2..15 | `JOINED_READY` | Input-Payloads in V2-Reihenfolge, Request als Separator zwischen je zwei Inputs |

Für `execute` validiert der Adapter und erzeugt `DagSchedulerSpecV2` und ruft den
unveränderten V2-Executor auf. Es gelten 1..64 Knoten, höchstens 15 direkte
Abhängigkeiten je Knoten, Startbudget 1..64 und Receipt-Budget
1..4.194.304 Bytes. IDs haben 1..64 ASCII-Zeichen aus `A-Z`, `a-z`, `0-9`,
`-`, `_`. Bestehende Abhängigkeiten und Azyklizität werden vor jedem Callback
geprüft. Die serielle Kahn-Auswahl startet den lexikographisch kleinsten
bereiten Knoten, synchron genau ein Callback zur Zeit. Inputs stammen nur
von den direkten READY-Vorgängern, nach `producer_node_id` sortiert;
transitive oder nicht erfolgreiche Vorgänger liefern keine Inputs.

Jeder Request und jede erfolgreiche Payload unterliegen der V2-Grenze von
65.536 Bytes und SHA-256-Prüfung. Append/Join zählen alle Payloads und
Separatoren überlaufsicher vor dem Zusammenfügen; Überschreitung liefert
`DagSchedulerV3PayloadLimitExceeded`, bevor eine zu große Payload entsteht.
Das Receipt-Budget `max_total_receipt_bytes` zählt retinierte Outputs und
alle Input-Kopien, keine Requests oder Metadaten. Es ist kein allgemeines
Prozessspeicherlimit. Zu wenig Startbudget liefert für alle verbleibenden
Knoten terminale `CANCELLED`-Receipts mit `START_BUDGET_EXHAUSTED`.
Receipt-Budgetüberschreitung
liefert `DagSchedulerV2ReceiptBudgetExceeded` ohne partiellen Run.

### Execute-Metadatenbericht

Erfolg liefert Exit 0, leeres stderr und genau einen vollständigen, stabil
formatierten stdout-JSON-Bericht mit einem abschließenden LF. Er enthält
keine rohen Request- oder Payload-Bytes und keine Hex-Kodierungen dieser Daten.

| Feld | Inhalt |
| --- | --- |
| `contract`, `action` | `dag-scheduler-v3-cli-v1`, `execute` |
| `spec_version` | `dag-scheduler-spec-v3` |
| `spec_digest` | SHA-256 der vollständigen unveränderten Dateibytes, einschließlich eines vorhandenen finalen LF |
| `scheduler_version` | Wahrheitsgemäß `dag-scheduler-v2` |
| `dag_id`, `max_started_nodes`, `max_total_receipt_bytes` | Geparste DAG-ID und V2-Budgets |
| `receipts` | Nach `node_id` sortierte V2-Receipts, eines je Knoten |
| `receipts[]` | `node_id`, `callback_id`, `request_size`, `request_digest`, `input_refs`, `outcome`, `failure_code`, `output` |
| `input_refs[]` | `producer_node_id`, `result_code`, `payload_size`, `payload_digest` |
| `output` | `null` oder `output_version: dag-scheduler-output-v2`, `result_code`, `payload_size`, `payload_digest` |
| `trace` | Chronologische V2-Ereignisse mit `event` und `node_id`, von `VALIDATED` bis `COMPLETED`, beide mit leerer Knoten-ID |

Das kanonische Beispiel liefert vier `READY`-Receipts: `a-source` mit
`LITERAL_READY` und 5 Bytes, `b-transform` mit `APPENDED_READY` und 10 Bytes,
`c-independent` mit `LITERAL_READY` und 2 Bytes sowie `d-join` mit
`JOINED_READY` und 28 Bytes. Retiniert werden 62 Payloadbytes einschließlich
Input-Kopien. Derselbe vollständige Spec-Bytestrom liefert bytegleiche
Berichte. Ein hinzugefügtes finales LF ist zulässig, ändert jedoch den
`spec_digest`. Größen und Digests sind Metadaten, keine Vertraulichkeits-,
Verschlüsselungs- oder Zugriffskontrollgarantie.

Der vollständige Bericht für das Beispiel mit finalem LF, am realen Binary
gemessen (keine Platzhalter oder gekürzten Receipt-Felder):

```json
{
  "contract": "dag-scheduler-v3-cli-v1",
  "action": "execute",
  "spec_version": "dag-scheduler-spec-v3",
  "spec_digest": "2152e5d27f902f8bf04113f29cdf6d3fddc625736d62232ff2ce4e3923d06d08",
  "scheduler_version": "dag-scheduler-v2",
  "dag_id": "example",
  "max_started_nodes": 4,
  "max_total_receipt_bytes": 4096,
  "receipts": [
    {"node_id": "a-source", "callback_id": "literal-v1", "request_size": 5, "request_digest": "8ed3f6ad685b959ead7022518e1af76cd816f8e8ec7ccdda1ed4018e8f2223f8", "input_refs": [], "outcome": "READY", "failure_code": "", "output": {"output_version": "dag-scheduler-output-v2", "result_code": "LITERAL_READY", "payload_size": 5, "payload_digest": "8ed3f6ad685b959ead7022518e1af76cd816f8e8ec7ccdda1ed4018e8f2223f8"}},
    {"node_id": "b-transform", "callback_id": "append-v1", "request_size": 5, "request_digest": "93f41553e2334a65bf24fff56f11668488ed30de45b92043370854a84e79dd37", "input_refs": [{"producer_node_id": "a-source", "result_code": "LITERAL_READY", "payload_size": 5, "payload_digest": "8ed3f6ad685b959ead7022518e1af76cd816f8e8ec7ccdda1ed4018e8f2223f8"}], "outcome": "READY", "failure_code": "", "output": {"output_version": "dag-scheduler-output-v2", "result_code": "APPENDED_READY", "payload_size": 10, "payload_digest": "8610fb69f8ec56759b2fb33fd43f9a05fefbd2d49cd35c8b5786284434537af3"}},
    {"node_id": "c-independent", "callback_id": "literal-v1", "request_size": 2, "request_digest": "96a296d224f285c67bee93c30f8a309157f0daa35dc5b87e410b78630a09cfc7", "input_refs": [], "outcome": "READY", "failure_code": "", "output": {"output_version": "dag-scheduler-output-v2", "result_code": "LITERAL_READY", "payload_size": 2, "payload_digest": "96a296d224f285c67bee93c30f8a309157f0daa35dc5b87e410b78630a09cfc7"}},
    {"node_id": "d-join", "callback_id": "join-v1", "request_size": 16, "request_digest": "cf3987609056cda4271e4b3c23cb6dc7ba078519d0cc9ef75db7cdaf4876ae1f", "input_refs": [{"producer_node_id": "b-transform", "result_code": "APPENDED_READY", "payload_size": 10, "payload_digest": "8610fb69f8ec56759b2fb33fd43f9a05fefbd2d49cd35c8b5786284434537af3"}, {"producer_node_id": "c-independent", "result_code": "LITERAL_READY", "payload_size": 2, "payload_digest": "96a296d224f285c67bee93c30f8a309157f0daa35dc5b87e410b78630a09cfc7"}], "outcome": "READY", "failure_code": "", "output": {"output_version": "dag-scheduler-output-v2", "result_code": "JOINED_READY", "payload_size": 28, "payload_digest": "5ef532d3f9f965d6e032cb5ced1ec52a257f1a8a46411021ee7fb5ad0313d524"}}
  ],
  "trace": [
    {"event": "VALIDATED", "node_id": ""},
    {"event": "STARTED", "node_id": "a-source"},
    {"event": "READY", "node_id": "a-source"},
    {"event": "STARTED", "node_id": "b-transform"},
    {"event": "READY", "node_id": "b-transform"},
    {"event": "STARTED", "node_id": "c-independent"},
    {"event": "READY", "node_id": "c-independent"},
    {"event": "STARTED", "node_id": "d-join"},
    {"event": "READY", "node_id": "d-join"},
    {"event": "COMPLETED", "node_id": ""}
  ]
}
```

### Read-only Inspector

**IMPLEMENTIERT UND BELEGT:** `inspect` ist ein read-only Pfad desselben
Parser-/CLI-Adapters. Er nutzt `parse_spec()` für die vollständige V3-Format-,
Callback-ID-/Arity- und V2-Strukturvalidierung. Er startet keine Callbacks
und ruft `execute_dag_scheduler_v2` nicht auf. Es entstehen keine Payloads,
keine Receipts und kein Laufresultat; der Bericht enthält keinen Trace.

Die statische Kahn-Reihenfolge wählt jeweils den
lexikographisch kleinsten bereiten Knoten anhand von IDs und Abhängigkeiten. Sie entspricht der
V2-Startreihenfolge bei ausschließlich `READY`-Ergebnissen und enthält jeden
Knoten genau einmal, auch bei einem Startbudget unter der Knotenzahl. Das
Beispiel liefert `a-source`, `b-transform`, `c-independent`, `d-join`.
Die Planung simuliert keine Callbackfehler, Payloadgrößen, Receipt-Budgets
oder Startbudget-Cancellation und sagt den Erfolg eines echten Laufs nicht
voraus. Beide Budgets werden validiert und berichtet, ohne Ausführung.

Erfolg liefert Exit 0, leeres stderr und genau einen stabilen JSON-Bericht
mit abschließendem LF. Wiederholte Aufrufe derselben Dateibytes sind
byteidentisch. Die erfolgreiche `execute`-Ausgabe bleibt unverändert.

| Feld | Inhalt |
| --- | --- |
| `contract`, `action` | `dag-scheduler-v3-inspect-v1`, `inspect` |
| `spec_version`, `scheduler_version` | `dag-scheduler-spec-v3`, `dag-scheduler-v2` |
| `spec_digest` | SHA-256 der unveränderten Dateibytes, identisch zu `execute` für dieselbe Datei |
| `dag_id`, `max_started_nodes`, `max_total_receipt_bytes` | Validierte DAG-ID und beide V2-Budgets |
| `nodes` | Knoten in der kanonischen Spec-Reihenfolge |
| `nodes[]` | Ausschließlich `node_id`, `callback_id`, `dependencies`, `request_size`, `request_digest` |
| `dependencies` | Kanonische direkte Abhängigkeits-IDs |
| `planned_start_order` | Ausschließlich Knoten-IDs in statischer Kahn-Reihenfolge |

Es gibt keine rohen Request- oder Payload-Bytes, keine Hex-Kodierungen dieser
Daten und kein `request-hex` im Inspector-Bericht. Leere und NUL-Requests
erscheinen nur als Größe und Digest. IDs, Größen und Digests bleiben sichtbare
Metadaten und bieten keine Vertraulichkeit oder Zugriffskontrolle.

Vollständiger Inspector-Bericht der kanonischen Beispiel-Spec mit finalem LF,
am realen Binary gemessen:

```json
{
  "contract": "dag-scheduler-v3-inspect-v1",
  "action": "inspect",
  "spec_version": "dag-scheduler-spec-v3",
  "spec_digest": "2152e5d27f902f8bf04113f29cdf6d3fddc625736d62232ff2ce4e3923d06d08",
  "scheduler_version": "dag-scheduler-v2",
  "dag_id": "example",
  "max_started_nodes": 4,
  "max_total_receipt_bytes": 4096,
  "nodes": [
    {"node_id": "a-source", "callback_id": "literal-v1", "dependencies": [], "request_size": 5, "request_digest": "8ed3f6ad685b959ead7022518e1af76cd816f8e8ec7ccdda1ed4018e8f2223f8"},
    {"node_id": "b-transform", "callback_id": "append-v1", "dependencies": ["a-source"], "request_size": 5, "request_digest": "93f41553e2334a65bf24fff56f11668488ed30de45b92043370854a84e79dd37"},
    {"node_id": "c-independent", "callback_id": "literal-v1", "dependencies": [], "request_size": 2, "request_digest": "96a296d224f285c67bee93c30f8a309157f0daa35dc5b87e410b78630a09cfc7"},
    {"node_id": "d-join", "callback_id": "join-v1", "dependencies": ["b-transform", "c-independent"], "request_size": 16, "request_digest": "cf3987609056cda4271e4b3c23cb6dc7ba078519d0cc9ef75db7cdaf4876ae1f"}
  ],
  "planned_start_order": ["a-source", "b-transform", "c-independent", "d-join"]
}
```

### Gemeinsame Fehlergrenzen

Fehlende, falsche, zusätzliche oder optionartige Argumente liefern Exit 2,
leeres stdout und exakt diese kompakte Zeile auf stderr mit abschließendem LF:

```json
{"error":"Usage: bruce dag-scheduler-v3 execute|inspect SPEC"}
```

Datei-, Parser-, Struktur-, Zyklus-, Callback- oder Receipt-Budgetfehler
liefern ebenfalls Exit 2 und keinen Teilbericht. Der Haupt-CLI-Fehlerpfad
schreibt JSON auf stderr, etwa `{"error": "InputByteLimit"}` für eine zu
große Datei oder `{"error": "InvalidDagSchedulerV3SpecHex"}` für ungültiges
Hex. Datenarme Parserkennungen geben Request-Hex/Payloads nicht wieder;
das ist keine allgemeine Vertraulichkeitszusage für IDs oder Dateipfade.
Für `inspect` werden alle Datei-, Format-, Callback-ID-/Arity-, Struktur-,
Zyklus- und Eingabegrößenfehler vor der Ausgabe abgewiesen: kein Teilbericht.
Die permanente 65.537-Byte-Regression prüft die Grenze für beide Routen;
das zusätzliche erlaubte finale LF überschreitet bereits die 65.536 Bytes.

### Grenzen und Belege

Es gibt keine freien Callback-Namen, Skripte oder Plugins, keine Persistenz,
keine Bundle-/Replay-Schnittstelle, keine Threads, keine Parallelität,
keine GPU-Ausführung, keine Netzwerkausführung, keine MCP-Anbindung und
keine allgemeine Runtime-Orchestrierung. Externe Payloadspeicher,
Phase-DAG-Adapter und diese weitergehenden Fähigkeiten bleiben
**DOKUMENTIERT / KONZEPT**. Phase-DAG V3 mit seiner eigenen Paralleloption
ist ein unveränderter separater Vertrag; V1/V2 bleiben unverändert.

Belege: [reale V3-Prozesse](../../tests/integration/test_dag_scheduler_v3_cli.py),
[Parser-/Payloadgrenzen](../../tests/unit/dag_scheduler_v3_cli_tests.cpp) und
[Dokumentationsvertrag](../../tests/integration/test_cli_docs.py).
`test_dag_scheduler_v3_cli.py` prüft Bericht, NUL-Bytes, direkte Input-Metadaten,
Digest, Bytegleichheit, Header/LF/CRLF, Parserfehler, Zyklus, Usage und Rohdaten-
Abgrenzung für beide Routen sowie den vollständigen statischen Inspector-Plan.
Die C++-Verträge prüfen zusätzlich callbackfreie Planung und Verzweigungen.
Der unveränderte Generator verify_examples.py führt V3 nicht aus, also weder
`execute` noch `inspect`.
Seine `examples.json` und `validation.md` sind keine V3-Laufzeitbelege;
der Quellenindex kann V3-Diagnoseliterale erfassen. Geschützte Generator-
Artefakte ausschließlich in einer vollständigen nativen temporären Kopie
ohne Leerzeichen über den bestehenden Verifier prüfen bzw. erzeugen:

```sh
scripts/with-host-deps.sh python3 docs/cli/verify_examples.py /tmp/bruce-inspect-task3-build/bruce
```

Der globale Generatortest setzt weiterhin MCP voraus und ist mit
NanoCell/MCP OFF unabhängig vom V3-Prozesstest eingeschränkt. Ein
sandboxierter Gesamt-CTest kann außerdem den unveränderten
`mcp_inspector_workflows` beim Socket-Anlegen sperren; ein gesonderter
Host-Lauf ersetzt keinen grünen Sandbox-Lauf.

## `dag-scheduler-v2` – fester direkter Payload-Fluss

**IMPLEMENTIERT UND BELEGT:** ein eigener adapterneutraler C++20-Kern für
begrenzten lokalen Byte-Payload-Fluss. Die CLI bietet genau ein festes
In-Memory-Demo, auch mit NanoCell und MCP OFF:

```text
bruce dag-scheduler-v2 demo flow
```

Der Kern validiert Version, Request-Digests, IDs, einen strikt nach
`callback_id` sortierten eindeutigen Katalog und einen azyklischen Graphen
vor dem Lauf. Er erlaubt 1..64 Knoten, höchstens 15 direkte Abhängigkeiten je
Knoten und Startbudget 1..64. IDs und Ergebniscodes sind 1..64 ASCII-Zeichen
aus `A-Z`, `a-z`, `0-9`, `-`, `_`. Requests und erfolgreiche Payloads sind
jeweils auf 65.536 Bytes begrenzt; jeder Digest muss exakt dem SHA-256 der
zugehörigen Bytes entsprechen.

Die serielle Kahn-Auswahl startet jeweils den lexikographisch kleinsten bereiten Knoten;
es läuft synchron genau ein Callback zur Zeit. Ein Callback erhält seinen
unveränderten Request und ausschließlich die vollständigen verifizierten
Payloads seiner direkten READY-Vorgänger, nach `producer_node_id` sortiert,
zusammen mit Ergebniscode und Digest. Indirekte oder nicht erfolgreiche
Vorgänger werden nicht als Inputs übergeben.

### Feste Spec und Receipt-Budget

`flow` verwendet `dag_id=flow-v2`, `max_started_nodes=4` und
`max_total_receipt_bytes=4096`. Alle Requests sind fest, nichtleer und korrekt
gehasht. Die tatsächlichen Payload-Bytes werden im Prozess weitergegeben:

| Knoten | Direkte Inputs | Ergebniscode | Outputgröße |
| --- | --- | --- | --- |
| `a-source` | keine | `SOURCE_READY` | 5 Bytes |
| `b-transform` | `a-source` (5 Bytes) | `TRANSFORM_READY` | 10 Bytes |
| `c-independent` | keine | `INDEPENDENT_READY` | 5 Bytes |
| `d-join` | `b-transform` (10 Bytes), dann `c-independent` (5 Bytes) | `JOIN_READY` | 16 Bytes |

Alle vier Receipts sind `READY`. Startreihenfolge: `a-source`, `b-transform`,
`c-independent`, `d-join`. Transform und Join prüfen und verbrauchen ihre
direkten Inputs; Join erhält keinen transitiven Input von `a-source`.

Das C++-Receipt-Budget liegt zwischen 1 und 4.194.304 Bytes und zählt die
retinierten Output-Payloads einschließlich aller Input-Kopien in `input_refs`.
Requests, Digests, IDs, Ergebnis- und Fehlercodes zählen nicht zu diesem
Budget. Der feste Lauf retiniert 56 Payloadbytes: 5 + (10 + 5) + 5 +
(16 + 10 + 5). Dies ist kein Gesamtprozess-Speicherlimit. Würde ein Receipt
das Budget überschreiten, propagiert `DagSchedulerV2ReceiptBudgetExceeded`
ohne partiellen Run.

Ungültige Callback-Rückgaben, falsche Digests und Callback-Ausnahmen
veröffentlichen ebenfalls keinen partiellen Run; bereits verursachte
Callback-Seiteneffekte werden dadurch nicht rückgängig gemacht. Ein gültiges
`failed` hat leere Payload und leeren Digest und wird als terminales `FAILED`
mit Ergebniscode als `failure_code` gespeichert. `FAILED`, `BLOCKED` und
`CANCELLED` haben weder Output noch Input-Kopien. Nicht erfolgreiche
Vorgänger blockieren Nachkommen transitiv mit `DEPENDENCY_NOT_READY`, bevor
Cancellation oder Startbudget geprüft werden. Vor jedem weiteren Start geht
`CONTROLLED_CANCELLATION` einem `START_BUDGET_EXHAUSTED` vor. Diese
C++-Verträge sind keine weiteren auswählbaren CLI-Demos.

### JSON, Determinismus und Exitcodes

Erfolg liefert Exit 0, leeres stderr und genau einen vollständig berechneten,
stabil formatierten stdout-JSON-Bericht mit abschließendem Zeilenumbruch.
Der Renderer zeigt keine rohen Request- oder Payload-Bytes; sichtbar sind nur
Digests, Größen und die folgenden Metadaten:

| Feld | Inhalt |
| --- | --- |
| `contract`, `scenario` | `dag-scheduler-v2-cli-v1`, `flow` |
| `scheduler_version`, `dag_id` | `dag-scheduler-v2`, `flow-v2` |
| `max_started_nodes`, `max_total_receipt_bytes` | 4, 4096 |
| `receipts` | Nach `node_id` sortiert, genau eines je Knoten |
| `receipts[]` | `node_id`, `callback_id`, `request_digest`, `request_size`, `input_refs`, `outcome`, `failure_code`, `output` |
| `input_refs[]` | `producer_node_id`, `result_code`, `payload_digest`, `payload_size` |
| `output` | `output_version: dag-scheduler-output-v2`, `result_code`, `payload_digest`, `payload_size` |
| `trace` | Chronologischer logischer Trace mit `event` und `node_id` |

Der Trace beginnt mit `VALIDATED`, enthält pro Start `STARTED` und `READY`
und endet mit `COMPLETED`; Anfang und Ende haben leere `node_id`. Er zeichnet
keine Zeitmessungen, Threads oder physischen Wellen auf. Wiederholte Aufrufe
der festen Demo liefern bytegleiche Berichte, auch im Standard-/OFF-Vergleich.

Der Kern ist bei gleichen Spec-, Control- und Callback-Rückgaben
deterministisch. Er kann gültige, aber unterschiedliche Callback-Rückgaben
in getrennten Läufen nicht automatisch als inhaltliche Nichtdeterministik
erkennen. Es gibt keine erneute Ausführung zur Erkennung, weil sie auch
Callback-Seiteneffekte duplizieren würde. Die Reinheit eingebundener Callbacks
bleibt Verantwortung ihres Einbinders; Digestvalidierung belegt die
Byteintegrität, keine semantische Gleichheit verschiedener Rückgaben.

Nur exakt die kleingeschriebenen Argumente `demo flow` sind zulässig.
Fehlende, zusätzliche, anders geschriebene oder parameterartige Argumente
liefern Exit 2, leeres stdout und ausschließlich diesen kompakten Fehler auf
stderr mit abschließendem Zeilenumbruch:

```json
{"error":"Usage: bruce dag-scheduler-v2 demo flow"}
```

### Grenzen und Belege

Es gibt keine freien DAG-Dateien, keine freien Callback-Namen,
keine freien Budgets, keine freien Payloads oder Requests und keine weiteren Parameter.
V2 bietet keine Threads, keine GPU-Ausführung, keine Netzwerkausführung,
keine Persistenz, keine Bundle-/Replay-Schnittstelle und
keine allgemeine Runtime-Orchestrierung. Bundle/Persistenz/Replay,
externe Payloadspeicher oder Artefaktstores, ein Phase-DAG-V3-Adapter,
Parallelität, GPU, Netzwerk und allgemeine Orchestrierung bleiben für diesen
Kern **DOKUMENTIERT / KONZEPT**. V1, Phase-DAG V2/V3 und MCP bleiben
unverändert; die vorhandene Paralleloption von Phase-DAG V3 gehört zu seinem
eigenen Vertrag. Byte-Payloads bieten keine Verschlüsselung oder
Zugriffskontrolle; das Verbergen in CLI-JSON ist keine Speicher-Sicherheitsgrenze.

Belege: [Kernverträge](../../tests/unit/dag_scheduler_v2_tests.cpp),
[reale CLI-Workflows](../../tests/integration/test_dag_scheduler_v2_cli.py)
und [Dokumentationsverträge](../../tests/integration/test_cli_docs.py).
Der unveränderte Generator verify_examples.py führt diese V2-Demo nicht aus.
`examples.json` und `validation.md` belegen seine bestehenden Szenarien,
keine V2-Funktionalität. Der Quellenindex erfasst auch V2-Diagnoseliterale;
der echte Workflow `test_dag_scheduler_v2_cli.py` ist der Laufzeitbeleg.
Die drei geschützten Artefakte nur in einer vollständigen nativen temporären
Projektkopie ohne Leerzeichen über den unveränderten Verifier regenerieren:

```sh
scripts/with-host-deps.sh python3 docs/cli/verify_examples.py /tmp/bruce-dag-scheduler-v2-final/bruce
```

Der bestehende globale Dokumentationsgeneratortest setzt MCP voraus;
mit NanoCell/MCP OFF scheitert seine Erwartung `InvalidMcpPort` am
korrekten `UnknownCommand: mcp`. Diese Konfigurationsgrenze ist kein
V2-Fehler; der V2-Workflow ist in beiden Konfigurationen eigenständig belegt.
Ein sandboxierter Gesamt-CTest kann außerdem den bestehenden
`mcp_inspector_workflows` beim lokalen Socket-Anlegen sperren.

## `dag-scheduler-v1` – feste serielle Referenzdemos

**IMPLEMENTIERT UND BELEGT:** ein unabhängiger, adapterneutraler C++20-Kern.
Die CLI bietet drei feste In-Memory-Demos als begrenzte lokale Referenz. Die
Befehle sind auch mit NanoCell und MCP OFF verfügbar; sie ergänzen den
`ApplicationTask`-Katalog nicht.

```text
bruce dag-scheduler-v1 demo mixed
bruce dag-scheduler-v1 demo budget
bruce dag-scheduler-v1 demo cancelled
```

Der Kern validiert vor dem Lauf einen festen, eindeutig nach `callback_id`
sortierten Callback-Katalog und einen azyklischen Graphen: 1..64 Knoten,
0..15 direkte Abhängigkeiten je Knoten und Startbudget 1..64. IDs und
Ergebniscodes sind 1..64 ASCII-Zeichen aus `A-Z`, `a-z`, `0-9`, `-`, `_`.
Die serielle Kahn-Auswahl startet den lexikographisch kleinsten bereiten Knoten;
zu jedem Zeitpunkt läuft genau ein Callback synchron. Ein neu bereiter
Nachfolger geht daher einem späteren bereits bereiten unabhängigen Knoten vor.
Callback-Inputs enthalten ausschließlich die direkten READY-Vorgänger,
nach `producer_node_id` sortiert, jeweils mit deren `result_code`.

Ein Callback liefert `ready` oder `failed` mit einem gültigen Ergebniscode.
Ein gültiges `failed` ist ein normales terminales `FAILED`-Receipt. Alle
noch nicht gestarteten Nachkommen eines nicht bereiten terminalen Vorgängers
werden transitiv `BLOCKED/DEPENDENCY_NOT_READY`, bevor Kontrolle oder Budget
greifen. Vor jedem möglichen Start wird die lokale Abbruchkontrolle geprüft;
Abbruch geht einem gleichzeitig erschöpften Startbudget vor. Ungültige
Spezifikationen, Kataloge oder Callback-Ergebnisse veröffentlichen keinen
partiellen `DagSchedulerRunV1`; Callback-Ausnahmen propagieren. Bereits
verursachte externe Callback-Effekte werden dadurch nicht rückgängig gemacht.
Diese C++-Validierungsfehler sind keine frei auswählbaren CLI-Fehlerdemos.

### Feste Szenarien

`mixed` hat `dag_id=mixed-v1`, fünf Knoten und Budget 5:

| Knoten | Callback | Direkte Vorgänger | Outcome / terminal_code |
| --- | --- | --- | --- |
| `a-root` | `root-ready-v1` | keine | `READY/ROOT_READY` |
| `b-fail` | `fail-v1` | `a-root` | `FAILED/EXPECTED_FAILURE` |
| `c-independent` | `independent-ready-v1` | keine | `READY/INDEPENDENT_READY` |
| `d-blocked` | `must-not-start-v1` | `b-fail` | `BLOCKED/DEPENDENCY_NOT_READY` |
| `e-finish` | `finish-v1` | `c-independent` | `READY/FINISHED` |

Startreihenfolge: `a-root`, `b-fail`, `c-independent`, `e-finish`.
Der logische Trace blockiert `d-blocked` nach `b-fail` und vor dem Start von
`c-independent`. Der Callback von `d-blocked` läuft nie. `b-fail` erhält
`a-root/ROOT_READY`; `e-finish` erhält `c-independent/INDEPENDENT_READY`.

`budget` hat `dag_id=budget-v1`, drei Knoten und Budget 1. Nur `a-root`
mit `root-ready-v1` startet und liefert `READY/ROOT_READY`. Der abhängige
`b-child` und der unabhängige `c-independent` erhalten beide
`CANCELLED/START_BUDGET_EXHAUSTED` und leere `input_refs`.

`cancelled` hat `dag_id=cancelled-v1`, dieselben drei Knoten und Budget 3.
Der erste Callback `cancel-root-v1` von `a-root` liefert `READY/ROOT_READY`
und setzt eine lokale Abbruchflagge. Die Kontrolle liest sie vor dem nächsten
Start; `b-child` und `c-independent` erhalten
`CANCELLED/CONTROLLED_CANCELLATION` und leere `input_refs`. In beiden
Demos laufen keine weiteren Callbacks. Ihre verbleibenden Knoten verwenden
`must-not-start-v1`, das bei einem unerwarteten Aufruf eine Ausnahme wirft.

### JSON, Reihenfolge und Exitcodes

Alle drei Demos liefern Exit 0, leeres stderr und vollständig berechnetes,
stabil formatiertes stdout-JSON mit genau einem abschließenden Zeilenumbruch.
Auch `FAILED`, `BLOCKED` und `CANCELLED` sind hier gültige terminale Outcomes;
Exit 0 besagt, dass die feste Referenz abgeschlossen wurde.

| Feld | Vertrag |
| --- | --- |
| `contract` | `dag-scheduler-v1-cli-v1` |
| `scenario` | `mixed`, `budget` oder `cancelled` |
| `scheduler_version` | `dag-scheduler-v1` |
| `dag_id` | `mixed-v1`, `budget-v1` oder `cancelled-v1` |
| `max_started_nodes` | Festes Budget 5, 1 oder 3 |
| `receipts` | Genau ein terminales Receipt je Knoten, nach `node_id` sortiert |
| `receipts[]` | `node_id`, `callback_id`, `input_refs`, `outcome`, `terminal_code` |
| `input_refs[]` | `producer_node_id`, `result_code`; nur direkte erfolgreich ausgeführte Vorgänger |
| `trace` | chronologische logische Reihenfolge; nicht nach Knoten-ID sortiert |
| `trace[]` | `event`, `node_id`; Ereignisse `VALIDATED`, `STARTED`, `READY`, `FAILED`, `BLOCKED`, `CANCELLED`, `COMPLETED` |

Der Trace beginnt mit `VALIDATED` und endet mit `COMPLETED`; beide verwenden
eine leere `node_id` (`""`). Er enthält keine Zeitmessungen oder physischen
Wellen. Wiederholte Aufrufe liefern bytegleiche Berichte. Der reale Workflow
prüft komplette JSON-Werte und rohe Bytes; jede CLI-Wiederholung ist ein neuer
Prozess und belegt keine in-process Prüfung der Abbruchbindung.

Nur exakt `demo` mit einem der drei kleingeschriebenen Szenarionamen ist
zulässig. Fehlende, zusätzliche, anders geschriebene, dateiartige oder
parameterartige Argumente liefern Exit 2, leeres stdout und ausschließlich
folgenden stderr-JSON-Fehler mit abschließendem Zeilenumbruch:

```json
{"error": "Usage: bruce dag-scheduler-v1 demo mixed|budget|cancelled"}
```

### Grenzen und Belege

Es gibt keine freien DAG-Dateien, keine freien Callback-Namen,
keine freien Budgets und keine weiteren Parameter. Diese Referenz bietet keine Threads,
keine GPU-Ausführung, keine Netzwerkausführung, keine Persistenz,
keine Bundle-/Replay-Schnittstelle und keine allgemeine Runtime-Orchestrierung.
Es gibt keine Erweiterung von Phase-DAG V2/V3 und keine MCP-Anbindung;
Signed-Affine-DAG V1, Toy-Relation V1 und die bestehende Brücke bleiben
unverändert. Parallelität oder eine Integration dieses Kerns in Phase-DAG
V2/V3, externe Adapter, Payloadspeicher und allgemeine Runtime-Arbeit bleiben
**DOKUMENTIERT / KONZEPT**. Die vorhandene begrenzte Paralleloption von
Phase-DAG V3 ist ein eigener unveränderter Vertrag.

Belege: [Kernverträge](../../tests/unit/dag_scheduler_v1_tests.cpp),
[reale CLI-Workflows](../../tests/integration/test_dag_scheduler_v1_cli.py)
und [Dokumentationsverträge](../../tests/integration/test_cli_docs.py).
Der unveränderte Generator verify_examples.py führt diese Demos nicht aus.
`examples.json` und `validation.md` belegen nur seine bestehenden Szenarien;
der Quellenindex erfasst auch Scheduler-Diagnoseliterale. Der reale Workflow
`test_dag_scheduler_v1_cli.py` ist der Laufzeitbeleg der drei Demos.
Die geschützten Artefakte ausschließlich in einer vollständigen nativen
temporären Kopie vom dortigen Projektstamm mit dem Standardbuild regenerieren:

```sh
scripts/with-host-deps.sh python3 docs/cli/verify_examples.py /tmp/bruce-dag-scheduler-v1-task3-build/bruce
```

Bekannte Konfigurationsgrenze: `cli_documentation_determinism` ruft diesen
Generator auf, der unverändert MCP-Prüfungen voraussetzt. Bei NanoCell und
MCP OFF liefert `mcp --port 0` daher korrekt `UnknownCommand: mcp`
statt des erwarteten `InvalidMcpPort`; diese Dokumentationsprüfung ist
für die OFF-Konfiguration inkompatibel. Das wurde bereits auf einer
wiederhergestellten pre-Task-2-Basis reproduziert und ist kein Schedulerfehler.
Der separate Scheduler-Workflow bleibt auch OFF verfügbar. Ein Standardlauf
mit gesperrten Sandbox-Sockets kann zudem `mcp_inspector_workflows`
beim Socket-Anlegen einschränken; daraus folgt keine Schedulerbegrenzung.

## `signed-affine-dag-v1` – feste Referenzdemos

**IMPLEMENTIERT UND BELEGT:** ein eigenständiger lokaler C++20-Referenzprototyp.
Die CLI bietet genau zwei feste In-Memory-Demos. Die Befehle sind auch ohne NanoCell
verfügbar und ergänzen den `ApplicationTask`-Katalog nicht.

```text
bruce signed-affine-dag-v1 demo merge
bruce signed-affine-dag-v1 demo cancellation
```

Beide Szenarien starten bei 2. Der Knoten `a-add` verwendet `add-two-v1`
(x+2), `z-double` verwendet `double-v1` (2*x), jeweils mit Vorgänger `source`.
Die unterschiedlichen Herleitungen zum Wert 4 werden zu genau einem
semantischen Zustand zusammengeführt, der beide Provenienzen behält.
`merge` verwendet zweimal +1 und liefert Bilanz 2; `cancellation` verwendet
+1 für `a-add` und -1 für `z-double` und liefert Bilanz 0. Auch bei Bilanz 0
bleiben Zustand und beide Pfadrecords erhalten. Der Quellenzustand bei 2
bleibt mit Gewicht +1 und einer Provenienz zusätzlich im Bericht.
Gewichte werden pro Knoten beigetragen und je Zustand summiert; sie werden
nicht entlang der Vorgängerkette multipliziert oder vererbt.

Erfolg liefert Exit 0, leeres stderr und einen lesbaren JSON-Bericht mit
abschließendem Zeilenumbruch auf stdout:

| Feld | Inhalt |
| --- | --- |
| `contract` | `signed-affine-dag-v1-cli-v1` |
| `scenario` | `merge` oder `cancellation` |
| `states` | Zustände aufsteigend nach Integerwert |
| `states[].value` | Kanonischer Int64-Wert: hier 2 und 4 |
| `states[].weight_balance` | Exakte Summe der Pfadgewichte dieses Zustands |
| `states[].paths` | Alle Provenienzen, aufsteigend nach `node_id` |
| `paths[].node_id` | `source` bei Wert 2, `a-add` und `z-double` bei Wert 4 |
| `paths[].predecessor_node_id` | `null` an der Quelle, sonst `source` |
| `paths[].operator_id` | Leerer String an der Quelle, sonst feste Operator-ID |
| `paths[].input_value`, `paths[].output_value` | An der Quelle beide 2, sonst 2 und 4 |
| `paths[].path_weight` | Integer -1 oder +1, wie oben beschrieben |

Wiederholte Aufrufe desselben Szenarios liefern bytegleiche Berichte.
Der Execution DAG enthält die einzelnen Operatoranwendungen; der abgeleitete
Semantic-State-Index fasst gleiche Werte zusammen. Der Kern prüft IDs,
Quellenfelder, Vorgänger, feste Operatoren, Gewichte und Zyklen vor Ausführung.
Bereite Knoten werden in topologischer Knoten-ID-Reihenfolge ausgeführt.
Der Kernkatalog enthält zusätzlich `subtract-one-v1` (x-1), das die beiden
CLI-Demos nicht verwenden. Int64-Multiplikation, Addition und Bilanzsummen
sind überlaufgeprüft; `SignedAffineDagArithmeticOverflow` beendet die
Kernberechnung ohne Teilresultat. Die festen Demos erzeugen keinen Überlauf
und bieten keine Eingabe, um diese Kernfehler frei auszulösen.

Genau `demo` und einer der beiden kleingeschriebenen Szenarionamen sind
zulässig. Fehlende oder zusätzliche Argumente, andere Aktionen/Szenarien
und Optionen wie `--output` liefern Exit 2, leeres stdout und ausschließlich
`{"error":"Usage: bruce signed-affine-dag-v1 demo merge|cancellation"}` auf stderr.
Der vollständige Bericht wird erst nach erfolgreicher Berechnung und
Formatierung ausgegeben.

Grenzen: keine Graphdateien, keine freien Formeln, keine Persistenz,
keine Erweiterung von Phase-DAG V2/V3. Es gibt keine Bundle-/Replaybefehle,
Plugins, allgemeinen Scheduling-APIs, GPU-/Netzausführung oder
MCP-Anbindung für diesen Prototypen. Die größere
[Affine Phase-DAG-Roadmap](../concepts/affine-phase-dag.md) bleibt
**DOKUMENTIERT / KONZEPT**. Belege:
[Kernverträge](../../tests/unit/signed_affine_dag_v1_tests.cpp),
[reale CLI-Workflows](../../tests/integration/test_signed_affine_dag_v1_cli.py)
und [Dokumentationsverträge](../../tests/integration/test_cli_docs.py).

## `signed-affine-relation-v1` – feste Referenzbrücke

**IMPLEMENTIERT UND BELEGT:** eine lokale C++20-Toy-Referenzbrücke, keine reale
Kryptographie. Signed-Affine-DAG V1 und Toy-Relation V1 bleiben unverändert:
ihre öffentlichen APIs, Befehle und bisherigen JSON-Berichte werden nicht
erweitert. Die neue CLI bietet genau ein festes In-Memory-Demo, auch ohne
NanoCell und MCP:

```text
bruce signed-affine-relation-v1 demo merge
```

Die Bridge führt den Signed-Affine-DAG V1 genau einmal aus und rekonstruiert
aus dessen Vorgängerrecords die Form `m × s + c`. Genau eine ausgeführte Quelle
ist erforderlich; Koeffizientenkomposition und Rückrechnung verwenden geprüfte
Int64-Arithmetik. Fehler, einschließlich Überlauf oder nicht invertierbarer
Toy-Koeffizientendifferenz, liefern kein partielles Brückenergebnis.

Das feste Demo verwendet die öffentliche Quelle `2` (`source`) und `n=101`:

| Auswahl | Operator | `(m,c)` | Integer-Ausgabe | Toy-Koeffizienten `(a,b)` | Punkt |
| --- | --- | --- | --- | --- | --- |
| `left: a-add` | `add-two-v1` | `(1,2)` | `1 × 2 + 2 = 4` | `(2,1)` | `4` |
| `right: z-double` | `double-v1` | `(2,0)` | `2 × 2 + 0 = 4` | `(0,2)` | `4` |

Die Gruppe ist `G=1`, `Q=2`. Allgemein gilt die modulo-normalisierte Abbildung
`Q=s mod n`, `a=c mod n`, `b=m mod n` nach `[0,n)`, also
`P = (aG + bQ) mod n`. Gleiche ausgeführte Integerwerte erzeugen so eine echte
Toy-Kollision. Die angeforderte `left`/`right`-Herkunft bleibt erhalten;
die Toy-API ordnet die kanonische Kollision lexikografisch nach Provenienz-ID:
`existing_provenance_id: a-add`, `new_provenance_id: z-double`, Punkt 4.
Toy-Relation V1 übernimmt Ableitung und unabhängige Rückverifikation.
`coefficient_delta=100`, `value_delta=99` liefern `candidate_k=2`;
Relation und Gesamtbericht tragen erst nach erfolgreicher Prüfung `VERIFIED`.
Der Kandidat ist nur der bereits öffentliche Quellwert, kein Geheimnis.

Erfolg liefert Exit 0, leeres stderr und genau einen deterministischen
JSON-Bericht mit abschließendem Zeilenumbruch auf stdout. Wiederholte Aufrufe
liefern bytegleiche Berichte.

| Feld | Inhalt |
| --- | --- |
| `contract` | `signed-affine-relation-v1-cli-v1` |
| `scenario` | `merge` |
| `source` | `node_id: source`, `value: 2` |
| `group` | `modulus: 101`, `generator: 1`, `public_q: 2` |
| `left`, `right` | Angeforderte Pfade `a-add` und `z-double`, wie oben |
| `left/right.node_id`, `source_node_id`, `source_value` | Pfad-ID, Quelle `source`, Wert 2 |
| `left/right.multiplier`, `offset`, `output_value` | Vollständige Form und ausgeführter Wert: `(1,2,4)` bzw. `(2,0,4)` |
| `left/right.toy_provenance` | Dieselbe Pfad-ID als `provenance_id`; `coefficient_state` mit `a`, `b`, `canonical_point`, wie oben |
| `collision` | `canonical_point: 4`, `existing_provenance_id: a-add`, `new_provenance_id: z-double` |
| `relation` | `modulus: 101`, `coefficient_delta: 100`, `value_delta: 99`, `candidate_k: 2`, `verification_status: VERIFIED` |
| `status` | `VERIFIED` |

Unter dem Befehlsnamen `signed-affine-relation-v1` sind nur die
kleingeschriebenen Argumente `demo merge` zulässig. Fehlende, zusätzliche,
anders geschriebene, dateiartige oder parameterartige Argumente liefern
Exit 2, leeres stdout und ausschließlich
`{"error":"Usage: bruce signed-affine-relation-v1 demo merge"}` auf stderr.
Der vollständige Bericht wird vor der Ausgabe berechnet und formatiert;
es gibt keine Fehlerdemo und keinen Teilbericht.

Grenzen: keine Suche, kein Secret-Recovery-Verfahren, kein DLP-Löser,
keine reale Kryptographie, keine elliptische Kurve und kein secp256k1.
Es gibt keine freien Formeln, keine Graphdateien, keine Gruppenparameter,
keine Dateien, keine Persistenz, keine Bundle-/Replaybefehle und
keine Erweiterung von Phase-DAG V2/V3. Netzwerk, GPU, Plugins, Scheduler
und MCP-Anbindung gehören nicht zu dieser Brücke. Die größere
[Affine Phase-DAG-Roadmap](../concepts/affine-phase-dag.md) bleibt
**DOKUMENTIERT / KONZEPT**.

Belege: [Kernverträge](../../tests/unit/signed_affine_relation_v1_tests.cpp),
[reale CLI-Workflows](../../tests/integration/test_signed_affine_relation_v1_cli.py)
und [Dokumentationsverträge](../../tests/integration/test_cli_docs.py).
Der unveränderte Generator verify_examples.py führt dieses Demo nicht aus.
`examples.json` und `validation.md` belegen deshalb nur seine bestehenden
Szenarien; der Quellenindex erfasst auch Diagnoseliterale der neuen Brücke.
Der reale Workflow `test_signed_affine_relation_v1_cli.py` prüft das vollständige
JSON, Exitcodes, leere Gegenstreams und wiederholte bytegleiche Ausgaben.
Die drei generierten CLI-Artefakte ausschließlich in einer vollständigen
nativen temporären Projektkopie vom dortigen Projektstamm regenerieren:

```sh
scripts/with-host-deps.sh python3 docs/cli/verify_examples.py /tmp/bruce-signed-affine-relation-native-build/bruce
```

## `toy-relation-v1` – feste Referenzdemos

**IMPLEMENTIERT UND BELEGT:** eine eigenständige lokale C++20-Referenz für
Kollisionen und modulare Relationen in der kleinen additiven Gruppe `Z/nZ`.
Die CLI bietet genau zwei feste In-Memory-Demos, auch ohne NanoCell und MCP:

```text
bruce toy-relation-v1 demo collision
bruce toy-relation-v1 demo noninvertible
```

Ein Zustand speichert normalisierte Koeffizienten `a,b` und den kanonischen
Punkt `P = (a*G + b*Q) mod n`. Zwei verschiedene, nicht leere Provenienz-IDs
am selben Punkt erzeugen einen Collision Record. Das Paar wird lexikografisch
nach `provenance_id` geordnet, bevor Collision Record und Relation entstehen;
auch vertauschte Kerneingaben ergeben dieselben Records. Die Relation lautet
`(b1-b2)*k = a2-a1 (mod n)` mit normalisierten Differenzen. Nur eine
invertierbare Koeffizientendifferenz liefert einen Kandidaten. Die getrennte,
unabhängige Rückverifikation prüft `Q = k*G mod n`, beide Provenienzpunkte
und die Relation, bevor die positive CLI-Ausgabe entsteht.

`collision` verwendet `n=101`, `G=1`, `Q=17`: `left=(2,3)` und
`right=(36,1)` treffen sich bei `canonical_point=53`. Daraus folgen
`coefficient_delta=2`, `value_delta=34` und `k=17` (`candidate_k=17`).
Bericht und Relation tragen nach der getrennten Prüfung `VERIFIED`.

`noninvertible` verwendet `n=15`, `G=1`, `Q=4`: `left=(0,5)` und
`right=(5,0)` treffen sich bei `canonical_point=5`. Die Differenz 5 ist modulo
15 nicht invertierbar. Der Kern liefert `NonInvertibleToyRelationCoefficient`
und keinen Relation Record. Nur diesen erwarteten Fehler aus der Ableitung
fängt die feste Demo ab: Ihr Bericht behält die Kollision und enthält
`status: NO_UNIQUE_RELATION`, `reason: NonInvertibleToyRelationCoefficient`,
kein `candidate_k` und kein `relation`-Feld. Das ist ein kontrollierter
Nicht-Erfolgsbefund zur eindeutigen Relation, weiterhin mit Exit 0.

Beide Demos liefern Exit 0, leeres stderr und genau einen JSON-Bericht mit
abschließendem Zeilenumbruch auf stdout. Wiederholte Aufrufe desselben
Szenarios liefern bytegleiche Berichte.

| Feld | Inhalt |
| --- | --- |
| `contract` | `toy-relation-v1-cli-v1` |
| `scenario` | `collision` oder `noninvertible` |
| `group` | `modulus`, `generator`, `public_q` wie oben |
| `provenances` | Zwei Einträge, aufsteigend nach `provenance_id`: `left`, `right` |
| `provenances[].coefficient_state` | `a`, `b`, `canonical_point` |
| `collision` | `canonical_point`, `existing_provenance_id: left`, `new_provenance_id: right` |
| `relation` | Nur bei `collision`: `modulus`, `coefficient_delta`, `value_delta`, `candidate_k`, `verification_status: VERIFIED` |
| `status` | `VERIFIED` oder `NO_UNIQUE_RELATION` |
| `reason` | Nur bei `noninvertible`: `NonInvertibleToyRelationCoefficient` |

Genau `demo` und einer der beiden kleingeschriebenen Szenarionamen sind
zulässig. Fehlende oder zusätzliche Argumente, andere Aktionen/Szenarien
und Optionen liefern Exit 2, leeres stdout und ausschließlich
`{"error":"Usage: bruce toy-relation-v1 demo collision|noninvertible"}` auf stderr.
Andere Kernfehler werden nicht als Nichtinvertierbarkeit behandelt; sie
bleiben Error-only-JSON auf stderr mit Exit 2, ohne Teilbericht.

Grenzen: keine freien Gruppenparameter, keine Dateien, keine Suche,
keine freien Formeln, keine Persistenz und keine Bundle-/Replaybefehle.
Der feste Kandidat ist kein Geheimnis; dies ist kein diskreter-Logarithmus-Löser,
keine Realwelt-Kryptographie, keine elliptische Kurve und kein secp256k1.
Der Kern akzeptiert nur `3 <= n <= 65535`, `1 <= G < n` mit `gcd(G,n)=1`
und normalisierte `Q,a,b < n`; er weist ungültige Werte vor der Ableitung ab.
Diese Bibliotheksgrenzen sind keine CLI-Parameter. Es gibt
keine Erweiterung von Phase-DAG V2/V3 oder Signed-Affine-DAG V1,
keine Scheduler-/Payloadverträge, Plugins, Netzwerk-/GPU-Ausführung oder
MCP-Anbindung. Die weitergehende
[Affine Phase-DAG-Roadmap](../concepts/affine-phase-dag.md) bleibt
**DOKUMENTIERT / KONZEPT**.

Belege: [Kernverträge](../../tests/unit/toy_relation_v1_tests.cpp),
[reale CLI-Workflows](../../tests/integration/test_toy_relation_v1_cli.py) und
[Dokumentationsverträge](../../tests/integration/test_cli_docs.py).
Der unveränderte `verify_examples.py` erfasst Quellen-Diagnosen, führt aber
keine Toy-Relation-Demo aus. Die generierten Beispiele und die
[Validierungsnotiz](validation.md) belegen daher diese Demos nicht;
dafür gelten die separaten realen CLI-Workflows.

## MCP Inspector

**IMPLEMENTIERT UND BELEGT:** Optionaler statischer Mod mit
`BRUCE_BUILD_MOD_MCP=ON` (Standard ON). Er setzt
`BRUCE_BUILD_MOD_NANOCELL=ON` voraus; andernfalls scheitert die Konfiguration
mit `McpRequiresNanoCell`. Mit MCP OFF fehlen Manifest, Befehl und Hilfe.

```text
bruce mcp [--port 1..65535]
```

Standardport: 8080. Ausschließlich dezimale Werte von 1 bis 65535 sind zulässig.
Nur keine Zusatzargumente oder genau ein Paar `--port N` werden angenommen.
Ungültige Portwerte liefern `InvalidMcpPort`, fehlende, doppelte oder andere
Argumente `InvalidMcpArguments`: JSON auf stderr, Exit 2. Der Dienst läuft
im Vordergrund; Ctrl+C beendet ihn. Er bindet ausschließlich `127.0.0.1`.
Ein nicht verfügbarer angeforderter Port führt zu kontrolliertem Exit ungleich
0, ohne andere Adresse oder Ersatzport und ohne fachliche JSON-Ausgabe.

Der read-only Inspector prüft bestehende Phase-DAG V2 Bundles mit dem festen
NanoCell-/Evidence-Gate-Katalog über
`replay_phase_dag_run_v2(bytes, phase_dag_nanocell_catalog_v2())`; Phase-DAG V3
verwendet den festen NanoCell-/Payload-Gate-Katalog über
`replay_phase_dag_run_v3(bytes, phase_dag_nanocell_catalog_v3())`.
Replay berechnet intern den deterministischen erwarteten Lauf zur
Konsistenzprüfung; die Oberfläche bietet keine neuen DAG-Ausführungen.
Uploads bleiben nur im Request-Speicher; keine Persistenz. Der Browser
übermittelt vollständige rohe Bundlebytes, keinen serverseitig geöffneten
Dateipfad. DAG-Scheduler V4 wird dagegen ausschließlich strukturell geprüft.
Es gibt ausschließlich folgende Methoden und Routen:

| Aufruf | Antwort |
| --- | --- |
| `GET /` | 200, eingebettete Oberfläche: Übersicht, Graph, Receipts, Trace, Prüfung sowie bei V3 Scheduler-Erklärung |
| `GET /api/v1/health` | 200, `{"status":"OK"}` |
| `POST /api/v1/inspect` | Nur V2, höchstens 1 MiB (1048576 Bytes); 200 mit `VALID` und bisherigem replaybasiertem V2-Modell, sonst 422 mit festem `REJECTED` |
| `POST /api/v3/inspect` | Nur V3, höchstens 5 MiB (5242880 Bytes); 200 mit `VALID` und V3-Metadaten, sonst 422 mit festem `REJECTED` |
| `POST /api/dag-scheduler-v4/inspect` | Nur DAG-Scheduler V4, höchstens 5 MiB (5242880 Bytes); 200 mit strukturell validierten Metadaten und `VALID`, sonst 422 mit festem `REJECTED` |

Das gemeinsame Transportlimit beträgt 5 MiB: größere Anfragen an alle drei
Inspect-Routen liefern 413 mit `{"status":"REJECTED","code":"REJECTED"}`
vor Dekodierung oder Replay. Die unabhängige V2-Fachgrenze bleibt 1 MiB; V2-Uploads über 1 MiB
bis einschließlich 5 MiB liefern 422. Versionsverwechslungen und Bundles mit
fehlgeschlagener Struktur- oder Digestprüfung liefern an der jeweils gewählten
Route 422. Bei Phase-DAG V2/V3 werden außerdem manipulierte Laufdaten durch
Replay geprüft; ein Replay-Mismatch liefert 422. Bei DAG-Scheduler V4 kann
eine strukturell gültige Änderung mit korrekt neu berechneten Digests dagegen
`VALID` erhalten. Diese Strukturprüfung bestätigt nicht, dass der aufgezeichnete Lauf tatsächlich ausgeführt wurde.
Engere Kern- und Kataloggrenzen gelten zusätzlich; 5 MiB allein garantieren
kein erfolgreiches V3-Replay.

Unerwarteter Dienstfehler: 500 mit `{"status":"ERROR","code":"ERROR"}`.
Die API antwortet als `application/json`; fachliche Ablehnung enthält nur
`{"status":"REJECTED","code":"REJECTED"}`, keine hochgeladenen Bytes,
Dateipfade oder Exceptiondetails. Nicht registrierte Pfade/Methoden erhalten 404.
Es gibt weiterhin genau eine Health-Route.

Die Oberfläche verlangt eine explizite Auswahl zwischen Phase-DAG V2 und
Phase-DAG V3 oder DAG-Scheduler V4 (Startauswahl V2). Sie bestimmt Titel, Zielroute und lokale
1-MiB-/5-MiB-Prüfung; keine automatische Erkennung anhand des Dateiinhalts.
`VALID` enthält DAG-Version/-ID, Knoten mit Adapteridentitäten und Abhängigkeiten,
Receipts mit Requestdigest, Inputreferenzen, Outcome/Fehlercode, Output oder
null sowie Trace mit Ereignis und Knoten-ID. Bei V3 zeigen Inputs und Outputs
zusätzlich tatsächliche `payload_size` und Payload-Digests:
`producer_payload_digest` am Input und `payload_digest` am Output.
JSON und Browser-DOM zeigen nur Metadaten, keine `request_bytes`,
`context_bytes`, `output_bytes`, `evidence_bytes` oder `payload_bytes`.
Erfolgreiche V3-Antworten enthalten zusätzlich `scheduler_explanation`: Die
Oberfläche zeigt dessen `canonical-logical-v1`, `parallelism_recorded: false`,
Knotenergebnis, Abhängigkeiten und direkte `blocking_predecessors`. Das ist
eine replay-abgeleitete logische Erklärung, kein physischer Ausführungstrace:
keine Zeitdaten, Startreihenfolge, Waves, Thread-IDs oder Parallelitätsgrenze
sind im Bundle gespeichert.
Alle Ansichten werden mit `textContent` gefüllt. Reset und Versionswechsel
leeren Dateiauswahl und Ansichten und invalidieren ausstehende Antworten.
Uploadreferenzen werden nach Anfrageabschluss freigegeben; das behauptet kein
physisches Überschreiben durch die Speicherverwaltung des Browsers.

Es gibt keine Jobs oder Importe, keine Adapterplugins, keinen externen
Payloadspeicher, keine Redis-/Language-Verwaltung, Prozessausführung,
Dateisystemnavigation, Adapter-Discovery, externe Bindung, generischen Beweiser,
GPU oder freie Sprache. Replay/Digest belegt Bundlekonsistenz, keine
Autorenidentität und keinen allgemeinen mathematischen Beweis. Historische
Historische Cockpit-Fragmente bleiben ungebaut. Siehe
[V2-Abnahmebelege](../superpowers/specs/2026-10-12-phase-dag-v2-cockpit-inspector-design.md),
[C++-Verträge](../../tests/mods/mcp/mcp_inspector_tests.cpp) und
[HTTP-/DOM-Verträge](../../tests/mods/mcp/test_mcp_inspector.py).

**IMPLEMENTIERT UND BELEGT:** DAG-Scheduler V4 is a separate read-only
structural inspector at exact `POST /api/dag-scheduler-v4/inspect`, with
explicit selection and a 5 MiB (5242880 bytes) limit on local `127.0.0.1`.
It checks the V4 header, complete bundle digest, decoding and canonical
repacking; no callbacks and no re-execution. `VALID` confirms structural
bundle consistency only, not successful replay, callback purity, author
identity or a mathematical proof. A structurally valid alteration with a
recomputed digest may pass; the inspector does not establish that the recorded
receipts or trace were executed. Phase-DAG V2/V3 keep their existing
replay-based routes and independent 1-MiB-V2/5-MiB-V3 limits.

V4 JSON and text-safe DOM show metadata only: `scheduler_version` is
`dag-scheduler-v2`; the overview contains DAG ID, start/receipt budgets and
node count. Nodes expose IDs, dependencies, `callback_id`, `request_size`
and `request_digest`; receipts add outcome/failure code and direct input
references (`producer_node_id`, `result_code`, `payload_size`, `payload_digest`),
plus output version, result code, size and digest or null. Trace entries
contain event and node ID. V4 has no `scheduler_explanation`; the V3 logical
explanation stays hidden and cleared when V4 is selected. No request,
context, output, evidence, input or payload bytes, hex/base64 encodings,
source paths or raw bundle text are returned. Bundles are local confidential
artifacts containing raw requests and payloads; metadata filtering provides
no encryption and no authentication or access control. Bytes remain only in
request memory with no persistence. Reset/version change clears selection
and views and invalidates pending replies; release does not promise physical
memory erasure.

There are no new DAG executions, no jobs or imports, no adapter plugins,
no replay buttons or routes, no persistent uploads, no payload download,
no dynamic plugins and no external access. Free language, a general prover,
GPU/network execution, external payload stores, Redis/Language management,
process starts, filesystem browsing and general runtime orchestration remain
**DOKUMENTIERT / KONZEPT**. The unchanged CLI example verifier covers MCP
argument parsing and V2/V3 examples; V4 structural HTTP/DOM/privacy evidence
comes from the dedicated MCP contracts, not generated examples.


## `mods` und `route` – Built-ins sowie installierte Data-/Logic-Mods

**IMPLEMENTIERT UND BELEGT:** `mods` projiziert die statischen Built-ins und
lokal installierte Data-/Logic-Pakete in einen gemeinsamen Katalog. Data-Mods
ergänzen nummerierte Tree-Knoten und kontrollierte Textmuster. Logic-Mods
ergänzen dieselben Routingdaten um genau einen signierten Worker, der nur über
die bestehende isolierte Logic-Mod-V1-Grenze ausgeführt wird. Routing allein
wählt ein Template; es führt keine Berechnung aus und begründet keinen
mathematischen Beweis.

| Aufruf | Wirkung |
| --- | --- |
| `bruce mods inspect PACKAGE` | Data-/Logic-Umschlag streng erkennen und Metadaten ausgeben; kein Vertrauensbeleg |
| `bruce mods verify PACKAGE --trust-store PATH` | Begrenztes Paketformat, Hash, Ed25519-Vertrauen und kind-spezifisches Schema prüfen; keine Installation |
| `bruce mods install PACKAGE --mod-root PATH --trust-store PATH` | Data oder Logic installieren; Zustand `installed_inactive` |
| `bruce mods update PACKAGE --mod-root PATH --trust-store PATH` | Neue Version atomar installieren und aktivieren; alte Versionen für Rollback behalten |
| `bruce mods list [--mod-root PATH]` | Immer Built-ins, mit Root zusätzlich alle installierten Data-/Logic-Versionen als `bruce-mod-catalog-v2` |
| `bruce mods status MOD_ID [VERSION] [--mod-root PATH]` | Den gemeinsamen Katalog nach ID und optional Version filtern |
| `bruce mods activate MOD_ID VERSION --mod-root PATH` | Genau diese installierte Version aktivieren; bisher aktive Version deaktivieren |
| `bruce mods disable MOD_ID --mod-root PATH` | Aktive Version deaktivieren; Dateien bleiben erhalten |
| `bruce mods run MOD_ID --template TEMPLATE --input TEXT --mod-root PATH` | Aktiven installierten Logic-Mod mit frisch gepinntem Worker ausführen |
| `bruce route --text TEXT --mod-root PATH [--output REPORT] [--mod-details]` | Text durch den aktiven Tree routen; V1 standardmäßig, V2-Besitzdetails nur opt-in |
| `bruce route replay REPORT --mod-root PATH [--mod-details]` | V1- oder ausdrücklich V2-formatigen Report vollständig gegen den aktuellen Snapshot prüfen |

Alle Pfade sind explizit. Es gibt keine automatischen Benutzerpfade,
mitgelieferten vertrauenswürdigen Schlüssel, zentrale Mod-Dienste oder einen
eigenen BRUCE-Systembenutzer. `install` und `update` legen `--mod-root` privat
für den aufrufenden lokalen Benutzer an; dessen Elternverzeichnis muss
existieren. Bei einem bestehenden Verzeichnis sind Nutzerbesitz und Rechte
`0700` erforderlich. Vertrauen wird bei der ersten Mutation in
`truststore.pub` festgelegt und muss später identisch sein. Mutationen halten
eine gemeinsame Root-Sperre und veröffentlichen den betroffenen Data- oder
Logic-Journalzustand atomar. Die übrigen Root-basierten Befehle erzeugen keinen
leeren Ersatz, wenn die Registry fehlt. `list` und `status` können ohne Root
die Built-ins anzeigen.

Pakete sind auf 1 MiB, Truststores auf 65536 Bytes begrenzt. Der Leser
akzeptiert nur eine reguläre Datei, folgt keinem finalen Symlink und verwirft
eine während des Lesens veränderte Datei. Data-Paketformat und
Schlüsselverwaltung sind im [V1-Fixturevertrag](../../tests/fixtures/runtime_mods/README.md)
beschrieben; Logic ergänzt den [Logic-Mod-Umschlag](formats.md#logic-mod-plugin-v1-paket-und-laufrecord).
Der öffentliche Testschlüssel darf nicht in Produktionsinstallationen vertraut
werden. `verify` mutiert nichts; Mountkonflikte und Golden Cases werden bei
Installation/Aktivierung geprüft. Logic-Worker liegen extrahiert privat mit
Modus `0500` und werden für jede Bindung erneut gegen Paket und Digest geprüft.

`install` ist idempotent für identische Bytes, ersetzt aber keine gleich
benannte Version mit anderem Digest. `update` fasst Installation und Aktivierung
in genau einer Mutation des betroffenen Kinds zusammen: identische Updates
ändern das Journal nicht; eine neue Version wird aktiv und die bisher aktive
Version bleibt `disabled` installiert. `activate MOD_ID ALTE_VERSION` ist der
explizite Rollback. Built-in-IDs (`core`, `language`, `nanocell`, `mcp`) sind
für Installation, Aktivierung, Deaktivierung und `mods run` unveränderlich.

`mods run` verlangt eine aktive Logic-Version mit `worker.execute`, ein dazu
gehöriges Template und höchstens 4096 Eingabebytes. Der Host bindet Registry,
Paket, Worker, Template und Capability frisch und startet die bereits
dokumentierte Worker-Sandbox. Data-Mods sind nicht ausführbar. Erfolg liefert
`installed-logic-mod-run-v1` mit Ergebnis und Bindungsdigests, aber ohne
Workerpfad oder Diagnoserohdaten. Timeout, Signal, ungültige Hostvalidierung
oder veraltete/tamperte Workerbytes sind kontrollierte Fehler auf stderr mit
Exit 2.

`route` akzeptiert höchstens 4096 UTF-8-Bytes. ASCII-Großbuchstaben und
Whitespace werden normalisiert; Wörter, Reihenfolge und vollständige Muster
müssen passen. Keine oder mehrere passende Zeilen ergeben `UNKNOWN` bzw.
`AMBIGUOUS` mit Exit 0, genau eine ergibt `MATCHED`. Nur dann ist
`selected_template` gesetzt, sonst `null`. Es gibt keine wahrscheinliche Auswahl.

`route-report-v1` enthält unverändert `text`, `registry_digest`, `status`,
`candidate_addresses`, `selected_template` und `active_packages` mit ID, Version
und vollständigem Paket-Digest. Tree und Paketliste stammen aus einem gemeinsam
gesperrten Snapshot. Report und Replay-Datei sind auf 1 MiB inklusive finaler
Newline begrenzt; eine größere Kandidatenliste führt zu `RouteReportTooLarge`
bevor ein Export erstellt wird. `--output` schreibt den vollständigen Report mit finaler
Newline exklusiv; vorhandene Ziele werden abgewiesen. Replay akzeptiert exakt
diesen kanonischen Report und vergleicht alle Felder. Ein geänderter aktiver Tree
führt zu `RouteRegistryDigestMismatch`, ein geänderter Befund bei gleicher
Tree-Version zu `RouteReportMismatch`. Ein anderer aktiver Pakethash wird auch
dann erkannt, wenn beide Versionen dieselben Tree-Knoten haben. Der Report ist
ein Reproduktionsbeleg, kein signierter Herkunftsnachweis.

Mit `--mod-details` wird stattdessen `route-report-v2` erzeugt. `candidates`
enthält je Adresse Template und `owner`; `selected_owner` ist bei `MATCHED`
gesetzt. Besitzer nennen `id`, `origin`, `kind`, `version` und
`package_digest`. Aktive Pakete tragen zusätzlich `kind`. Replay verlangt für
V2 ebenfalls `--mod-details`; V1 ohne Flag bleibt bytegleich.

Erfolg liefert ausschließlich JSON auf stdout. Parser-, Vertrauens-, Datei-
und Zustandsfehler liefern JSON auf stderr und Exit 2. Signaturen und
Truststore-Inhalte werden nicht ausgegeben. `install`, `update`, `activate` und
`disable` ändern die ausdrücklich gewählte Registry auch ohne `--output`.
Der vollständige Sicherheits- und Zustandsvertrag steht in
[Local Mod Lifecycle V2](../contracts/10-local-mod-lifecycle-v2.md).

## `logic-mod-plugin-v1` – explizite isolierte Sitzung

**IMPLEMENTIERT UND BELEGT:** Ein separat signiertes Logic-Paket kann in
einer einzigen lokalen Sitzung installiert, ausdrücklich aktiviert, ausgeführt
und geprüft werden. Voraussetzung ist Linux mit ausführbaren versiegelten
`memfd`s, `close_range`, Seccomp User Notification und
`SECCOMP_USER_NOTIF_FLAG_CONTINUE`. Fehlt eine Voraussetzung oder schlägt die
Einrichtung fehl, läuft kein Worker ohne diese Begrenzungen weiter.

```sh
bruce logic-mod-plugin-v1 session PACKAGE --trust-store TRUST \
  --actions install,activate,run,inspect,replay --input '3/4 hoch 2' \
  --record NEW_RECORD

bruce logic-mod-plugin-v1 session PACKAGE --trust-store TRUST \
  --actions install,activate,replay --input '3/4 hoch 2' \
  --replay-record RECORD

bruce logic-mod-plugin-v1 session PACKAGE --trust-store TRUST \
  --actions install,activate,run,inspect,replay --input '3/4 hoch 2' \
  --input-artifact MANIFEST_ID:OBJECT_INDEX --store LOCAL_STORE \
  --record NEW_RECORD

bruce logic-mod-plugin-v1 session PACKAGE --trust-store TRUST \
  --actions install,activate,run,inspect,replay --input '3/4 hoch 2' \
  --output-manifest RESULT_ID --store LOCAL_STORE --record NEW_RECORD

bruce logic-mod-plugin-v1 session PACKAGE --trust-store TRUST \
  --actions install,activate,replay --input '3/4 hoch 2' \
  --output-manifest RESULT_ID --store LOCAL_STORE --replay-record RECORD
```

`PACKAGE` ist das [Logic-Paket](formats.md#logic-mod-plugin-v1-paket-und-laufrecord),
`TRUST` ein expliziter Runtime-Mod-Truststore. Erlaubt sind ausschließlich
`--trust-store`, `--actions`, `--input`, `--record`, `--replay-record`,
`--input-artifact`, `--output-manifest` und `--store`, jeweils einmal mit Wert.
`--store` erscheint genau einmal, wenn mindestens eine der beiden
Referenzoptionen vorhanden ist; andernfalls fehlt es. Ein kombinierter
Input-/Outputlauf teilt diesen einen lokalen Store. `--input-artifact` lautet
exakt `MANIFEST_ID:OBJECT_INDEX`; der
Index ist eine kanonische vorzeichenlose Dezimalzahl. Der Storepfad wird nur
vom Host geöffnet und erscheint nie im Workerframe oder Record. Die
kommagetrennte Aktionsliste enthält höchstens 16 Aktionen
aus `install`, `activate`, `run`, `inspect`, `replay`, `disable`, `revoke`.
Genau ein `install` steht zuerst; höchstens ein `run` und ein `replay` sind
zulässig. Zustandsübergänge prüfen die weitere Reihenfolge. `run` aktiviert
nichts selbst. `inspect` prüft den Zustand; der abschließende JSON-Bericht
enthält den endgültigen Sitzungszustand.

Vor Installation werden beide Signaturen, die Templatebindung, das geschützte
Worker-Artefakt und die signierten Routing-Golden-Cases geprüft. Golden Cases
sind Routingprüfungen, keine Worker-Testläufe. Die Registry existiert nur im
Prozessspeicher; nach Prozessende ist nichts dauerhaft installiert oder aktiv.
Die Zustände sind `DISCOVERED`, `VERIFIED`, `GOLDEN_TESTED`,
`INSTALLED_INACTIVE`, `ACTIVE`, `DISABLED`, `REVOKED`. Deaktivierte Versionen
können ausdrücklich reaktiviert werden, widerrufene nicht. Eine neue Version
ersetzt keine aktive Version stillschweigend. Unmittelbar vor jedem Start
fordert der Host eine neue Bindung an; diese ist ein Autorisierungssnapshot,
kein während des laufenden Workers nachträglich widerrufbares Token.

`run` und `replay` benötigen `--input`. Der einzige Hostvalidator ist
`math.rational.power.v1` mit Ergebniscodec `rational-v1`: normalisierte Eingabe
`BASIS hoch EXPONENT`, dezimale ganze/rationale Basis mit höchstens 64 Zeichen,
Exponent 0..16. Der Host berechnet das kanonische Ergebnis unabhängig mit GMP.
Ein akzeptiertes Ergebnis behält `claim_scope=candidate_only` und
`validator=rational-power-v1`.

Zulässig sind ausschließlich diese vier streng sortierten Capability-Mengen:
`worker.execute`, `artifact.input.v1,worker.execute`,
`artifact.output.v1,worker.execute` und
`artifact.input.v1,artifact.output.v1,worker.execute`.
Die Eingabeoption benötigt `artifact.input.v1`. Der Host
akzeptiert genau ein lokales `READY`-Artifact-Store-V1.2-Objekt; dessen
Manifest, Index, SHA-256, Größe und vollständige Bytes werden vor jedem Spawn
geprüft. Maximal 1048576 Bytes sind erlaubt. Der Host materialisiert eine
versiegelte read-only Memfd auf FD 6. Für den Worker ist sie eine reine
Lesekapazität: `read`, `fstat`, `lseek` und `close` sind erlaubt; Schreiben und
file-backed Mapping sind verweigert. Der Worker erhält keinen Storepfad, keine
weitere Payloadreferenz und keine Berechtigung, Store oder Netzwerk zu öffnen.
Ohne Eingabeoption bleibt der Worker inputfrei.

**IMPLEMENTIERT UND BELEGT:** Result Artifact V1 wird ausdrücklich mit
`--output-manifest MANIFEST_ID --store LOCAL_STORE` angefordert und benötigt
`artifact.output.v1`. Der Host prüft Berechtigung, Manifest-ID und Store vor
Spawn und bindet Ausgabe- und Ausführungsrecht unmittelbar vor Spawn frisch
an dieselbe Paket-/Registry-/Workeridentität. Erst nach normalem Workerende
und Hostakzeptanz importiert er ausschließlich das kanonische `result` als
unveränderte UTF-8-Bytes, nie den privaten `response_frame`. Die feste Grenze
beträgt 16384 Bytes (16 KiB); das gemeinsame stdout/stderr-Budget gilt zusätzlich
einschließlich Protokollrahmen. Der feste Codec ist `logic-mod-result-text-v1`.
Das neue `READY`-/`ACCEPTED`-Manifest enthält genau ein Objekt am Index 0.
Eine identische Veröffentlichung ist idempotent, eine abweichende Belegung
endet mit `ArtifactManifestConflict`; es gibt kein Überschreiben. Ein bereits
importiertes Objekt kann nach einem späteren Manifestkonflikt erhalten bleiben.
Ablehnung, Timeout, Sandboxabbruch oder Outputlimit veröffentlichen kein
Ergebnismanifest. Der Worker erhält weder Storepfad, Manifest-ID, Store-/
Manifest-FD noch zusätzliche Schreib-, Syscall- oder Netzwerkrechte.
Ohne Ausgabeoption bleiben bisherige Request- und Recordbytes unverändert.
Mehrfachausgaben, Streams, freie Codecs, Remote-Publish, Garbage-Collection,
Wiederholungen, direkte Plugin-zu-Plugin-Übergabe und parallele Plugin-Ausführung gehören
nicht zu diesem Pfad.

| Hostbudget | CLI-Standard | C++-Grenzen |
| --- | ---: | ---: |
| Wandzeit | 250 ms | 1..5000 ms |
| Adressraum | 64 MiB | 8..256 MiB |
| rohe Eingabe | 4096 Bytes | 1..4096 Bytes |
| stdout und stderr zusammen | 16384 Bytes | 1..65536 Bytes |

Die CLI bietet keine Budgetoptionen. Der Host setzt zusätzlich CPU-Limit
`ceil(wall_ms/1000)` Sekunden, Dateigröße/Core-Dumps auf null und überwacht
Output und Wandzeit; Fehlerpfade beenden und reapieren den Child-Prozess.
Ein statischer 64-Bit-Little-Endian-ELF ohne Interpreter läuft aus frisch
gebundenen, versiegelten Bytes. Request auf stdin und gemeinsamer Output auf
stdout/stderr sind die einzigen Workerdeskriptoren nach Start, außer FD 6 beim
expliziten Payloadpfad. Umgebung ist leer, argv enthält nur
`logic-worker-v1`, cwd ist ein privates temporäres Verzeichnis. Der Worker
erhält keine lesbaren Paketdateien, Payloadreferenzen oder Hostpfade; FD 6
übermittelt ausschließlich die zuvor versiegelten Bytes. Er besitzt keinen
Dateizugriff, keine Shell, Netzwerk-, Unterprozess-, Registry-, Truststore-,
Budget- oder Claim-Scope-Autorität. Der Host erlaubt genau das erste `execveat`
seines vertrauenswürdigen Startstubs; jeder spätere Versuch endet mit
`EXEC_DENIED`. Reguläres `execve` wird direkt durch Seccomp beendet.

`--record` verlangt `run` und publiziert eine neue Datei nach den lokalen
Dateiregeln. `--replay-record` verlangt `replay` ohne `run`; ohne diese Option
prüft `replay` den Lauf derselben Sitzung. Replay startet keinen Prozess,
fordert aber erneut die aktuelle aktive Bindung an und vergleicht Request,
Budgets, Antwort, unabhängiges Validatorergebnis und jeden Recordbyte. Bei
Payload löst es dieselbe lokale Referenz erneut auf; fehlende, nicht-READY,
manipulierte oder in der gebundenen Identität abweichende Objekte werden vor
einer Erfolgsaussage abgelehnt.
Bei Ergebnisartefakten muss `--output-manifest` samt `--store` zur im Record
vorhandenen Ausgabe passen. Replay öffnet den Store frisch und prüft das
vollständige feste Einobjekt-Manifest einschließlich READY/ACCEPTED, Codec und
Origin sowie Objekt-SHA-256 und Länge gegen das erneut hostvalidierte Ergebnis;
die Store-Objektverifikation belegt dessen Bytes. Fehlende oder manipulierte
Objekte/Manifeste führen zu einem kontrollierten Fehler. Replay erzeugt keinen
Store und startet keinen Worker.
Dies bestätigt Transkriptkonsistenz, keine signierte Ausführungsattestation.

JSON-Vertrag: `logic-mod-plugin-v1-session`, Felder `state`, `package_digest`,
`worker_sha256`, `registry_digest`, `replay_verified`, `runs`. `runs` enthält
null oder einen Lauf mit `accepted`, `status`, `result`, `claim_scope`,
`validator`, `request_digest`, `response_digest`. Ein abgelehnter Lauf enthält
keine Ausgabemetadaten. Nur nach erfolgreicher Veröffentlichung
kommen `output_manifest_id`, `output_object_index`, `output_sha256`,
`output_size_bytes`, `output_codec` hinzu; keine Storepfade oder rohen
Artifactbytes. Ein kontrolliert abgelehnter
Workerlauf liefert Exit 2 mit diesem JSON auf stdout und ohne akzeptiertes
Ergebnis; Options-, Aktivierungs- und Replayfehler nutzen die CLI-Fehlerausgabe.

Belege: [Paketverträge](../../tests/unit/logic_mod_plugin_v1_tests.cpp),
[Workerverträge](../../tests/unit/logic_mod_worker_v1_tests.cpp),
[Payload-Workerverträge](../../tests/unit/logic_mod_payload_worker_v1_tests.cpp),
[reale CLI-Workflows](../../tests/integration/test_logic_mod_plugin_v1.py) und
[Payload-CLI-Workflows](../../tests/integration/test_logic_mod_payload_v1.py),
[Result-Artifact-Verträge](../../tests/unit/logic_mod_result_artifact_v1_tests.cpp)
und [Result-Artifact-CLI-Workflows](../../tests/integration/test_logic_mod_result_artifact_v1.py).
Der unveränderte CLI-Beispielverifier führt diese Sitzung nicht aus;
generierte Beispiele sind keine Logic-Worker-Laufbelege. Ein produktiver
Logic-Paketbuilder, dauerhafte Registry, automatische Updates, Wasm,
Shared Libraries im Host und freie Pluginsprachen sind nicht enthalten.

## `logic-mod-chain-v1` – zwei serielle Artifact-Schritte

**IMPLEMENTIERT UND BELEGT:** Der Host führt genau zwei ausdrücklich gewählte,
separat signierte Logic-Pakete A und B seriell aus. Es gelten die Linux-/
Sandboxvoraussetzungen, der feste Validator `math.rational.power.v1` und die
unveränderten Standardbudgets der [Logic-Mod-Sitzung](#logic-mod-plugin-v1--explizite-isolierte-sitzung).

```sh
bruce logic-mod-chain-v1 run \
  FIRST_PACKAGE --first-trust FIRST_TRUST --first-input TEXT_A \
  SECOND_PACKAGE --second-trust SECOND_TRUST --second-input TEXT_B \
  --store LOCAL_STORE \
  --first-output-manifest MANIFEST_A \
  --second-output-manifest MANIFEST_B \
  --record CHAIN_RECORD

bruce logic-mod-chain-v1 replay \
  FIRST_PACKAGE --first-trust FIRST_TRUST --first-input TEXT_A \
  SECOND_PACKAGE --second-trust SECOND_TRUST --second-input TEXT_B \
  --store LOCAL_STORE \
  --record CHAIN_RECORD
```

Die gezeigte Reihenfolge ist verbindlich; jeder Positions- und Optionsplatz
ist genau einmal mit einem Wert zu belegen. Texte mit Leerzeichen müssen als ein Shellargument
zitiert werden, etwa `'3/4 hoch 2'`. Leere, nur aus Leerraum bestehende,
unbekannte, doppelte, umsortierte oder zusätzliche Argumente werden vor jedem
Paket-, Truststore-, Store- oder Recordzugriff als
`InvalidLogicModChainArguments` abgewiesen. Replay akzeptiert keine
Ausgabe-Manifestoptionen: beide IDs stammen aus dem Record.

A benötigt exakt `artifact.output.v1,worker.execute`, B exakt
`artifact.input.v1,artifact.output.v1,worker.execute`. Beide Pakete durchlaufen
getrennte Discover-/Verify-/Golden-Test-/Install-/Activate-Lebenszyklen in
getrennten flüchtigen Registries mit dem jeweils angegebenen Truststore.
B autorisiert seine eigene Ausgabe; A's Outputberechtigung wird nicht
wiederverwendet. Vor A prüft der Host beide Pakete einschließlich statischer
nativer ELF-Struktur, Capabilities, Eingabelänge/-normalisierbarkeit, Budgets
und die beiden gültigen, verschiedenen Manifest-IDs. Frische Ausführungs-/Artefaktbindungen gelten
unmittelbar vor dem jeweiligen Workerstart.

Nach normalem Ende und unabhängiger Hostakzeptanz veröffentlicht der Host A's
kanonisches `result` als Result Artifact V1 nach `MANIFEST_A`. Ausschließlich
Objekt 0 dieses gerade veröffentlichten READY-Manifests wird frisch aus
demselben lokalen Store geprüft und B als versiegelte read-only Payload-Memfd
auf FD 6 bereitgestellt. B erhält außerdem `TEXT_B`, wählt aber keine eigene
Artefaktreferenz. Der Host veröffentlicht B's akzeptiertes `result` nach
`MANIFEST_B`, ebenfalls genau ein Objekt an Index 0 mit
`logic-mod-result-text-v1` und höchstens 16384 Bytes. Die Worker kommunizieren
nicht miteinander und erhalten keine Store-/Manifestpfade, Ausgaberechte oder
zusätzlichen Syscallrechte. Der unveränderte Hostvalidator prüft das Ergebnis
gegen die jeweilige Texteingabe; dies ist kein allgemeiner Nachweis, wie B den
Payloadinhalt verwendet hat.

Nur eine vollständig akzeptierte Chain publiziert `CHAIN_RECORD`: eine neue
private reguläre Datei, atomar ohne Ersetzen eines vorhandenen Ziels und mit
Modus 0600. Das Elternverzeichnis muss existieren. Symlinkkomponenten, `..`
und abschließende `/`, `/.` oder `/./` sind unzulässig. Replay verlangt eine
reguläre Datei des effektiven Benutzers, ohne Gruppen-/Fremdrechte, mit genau
einem Hardlink und höchstens 524288 Bytes; Spezialdateien werden vor dem Lesen
abgewiesen. Der [kanonische Chain-Record](formats.md#logic-mod-artifact-chain-v1-record-und-json)
enthält private Eingabe-/Ergebnis-/Antwortbytes der eingebetteten Run-Records.
Er ist keine öffentliche Metadatenansicht und keine Verschlüsselung.

Replay verifiziert beide Pakete erneut, stellt ihre aktiven Bindungen her und
prüft beide unveränderten Run-Records samt unabhängigem Hostvalidator.
`MANIFEST_A:0` und `MANIFEST_B:0` werden aus einem vorhandenen Store frisch
auf READY/ACCEPTED, vollständige feste Manifestidentität, Codec, SHA-256,
Größe und gespeicherte Ergebnisbytes geprüft. Es startet keinen Worker,
erzeugt keinen Store und schreibt keine Artefakte oder Records. Dies bestätigt
Transkriptkonsistenz unter aktueller Autorisierung, keine signierte historische
Ausführungsattestation.

Erfolg liefert Exit 0 und eine JSON-Zeile auf stdout mit Vertrag
`logic-mod-artifact-chain-v1`, `action`, `accepted`, `status`, `chain_id`,
`first`, `second`. Jede vorhandene Ausgabe enthält nur `manifest_id`,
`sha256`, `size_bytes`, `codec`; keine Pfade, Eingaben, Ergebnisse oder
Workerdiagnostik. Kontrolliertes Worker-Versagen liefert Exit 2 mit demselben
JSON-Vertrag und Status `CHAIN_FIRST_FAILED` oder `CHAIN_SECOND_FAILED`;
nicht publizierte Ausgaben und die Chain-ID sind null. Scheitert A, startet B
nicht. Scheitert B, kann A's Artefakt bestehen bleiben, aber es gibt kein
B-Ergebnismanifest und keinen Chain-Record. Andere Lauf-/Dateifehler liefern
Exit 2 mit `{"error":"LogicModChainFailed"}` auf stderr, Replayfehler
`{"error":"CHAIN_REPLAY_MISMATCH"}`. Scheitert erst die Recordpublikation,
können beide bereits akzeptierten Artefakte bestehen bleiben; dieser Schritt
ist keine Transaktion mit Rollback über beide Artefakte und Record.

V1 hat genau zwei Schritte, einen lokalen Store und einen festen Übergang
A nach B. Keine externen Payloads für A, weiteren Schritte, Verzweigungen,
Schleifen, Parallelität, Retries, Streams, freien Codecs, Remote-Stores,
Netzwerk, dynamische Paketauswahl oder allgemeine Orchestrierung.
Belege: [Core-/Replay-/Vorabprüfungen](../../tests/unit/logic_mod_artifact_chain_v1_tests.cpp)
und [reale CLI-Workflows](../../tests/integration/test_logic_mod_artifact_chain_v1.py).
Der unveränderte CLI-Beispielverifier führt die Chain nicht aus; seine
generierten Beispiele sind kein Chain-Laufbeleg.

## `logic-mod-artifact-dag-v2` – begrenzter serieller Artifact-DAG

**IMPLEMENTIERT UND BELEGT:** V2 führt 1–16 explizite, signierte Logic-Mods
in einem lokalen Store seriell aus. Die [JSON-Spec und Formate](formats.md#logic-mod-artifact-dag-v2-spec-payload-record-und-json)
legen Knoten und direkte Vorgänger fest.

```text
bruce logic-mod-artifact-dag-v2 run SPEC_JSON --store LOCAL_STORE --record DAG_RECORD
bruce logic-mod-artifact-dag-v2 replay SPEC_JSON --store LOCAL_STORE --record DAG_RECORD
```

Genau diese Reihenfolge gilt für beide Aktionen. Jeder Wert muss ein einzelnes,
nicht leeres und nicht ausschließlich aus Leerraum bestehendes Argument sein;
Pfadargumente dürfen nicht mit `--` beginnen. Fehlende, zusätzliche, unbekannte,
doppelte oder umsortierte Argumente scheitern vor Dateizugriff mit Exit 2,
leerem stdout und `{"error":"InvalidLogicModDagArguments"}` auf stderr.

Die Spec ist eine reguläre UTF-8-JSON-Datei mit höchstens 65536 Bytes.
Unbekannte oder doppelte JSON-Schlüssel (auch nach Escape-Decodierung),
ungültiges UTF-8, falsche Typen, Restbytes, mehr als 16 Knoten, doppelte IDs,
unsortierte/doppelte Vorgänger, Selbstkanten, fehlende Vorgänger, Zyklen oder
Manifest-Aliase werden vor Paket-, Truststore-, Record- und Storezugriff
abgewiesen. Spec-, Paket-, Truststore- und Recordpfade werden über
No-follow-Deskriptoren gelesen; Symlinkkomponenten, `..` und abschließende `/`
oder `/.` sind unzulässig. Spezialdateien werden vor dem Lesen abgewiesen.
Relative Paket-/Truststorepfade beziehen sich auf das Arbeitsverzeichnis.

Jeder Knoten hat einen eigenen Discover-/Verify-/Golden-Test-/Install-/Activate-
Lebenszyklus und eine flüchtige Registry unter seinem expliziten Truststore.
Das feste Template ist `math.rational.power.v1`. Wurzelpakete benötigen exakt
`artifact.output.v1,worker.execute`; Nichtwurzelpakete exakt
`artifact.input.v1,artifact.output.v1,worker.execute`. Alle Pakete,
Autorisierungen, nativen statischen ELF-Strukturen, Texte/Budgets und gültigen,
paarweise verschiedenen Input-/Output-Manifest-IDs werden vor dem ersten
Worker und jeder Storemutation geprüft. Bestehende Store-Ziele werden vorher
schreibgeschützt auf sichere Publikationsziele geprüft.

Die Ausführung wählt jeweils den lexikographisch kleinsten ausführbaren Knoten.
Wurzeln erhalten kein FD-6-Payload. Für jeden Nichtwurzelknoten prüft der Host
alle direkten Vorgängerartefakte frisch auf vollständige READY/ACCEPTED-
Identität und Ergebnisbytes. Er publiziert deren ID-sortiertes kanonisches
DAG-Inputdokument unter dem angegebenen `input_manifest` als internes Artifact
mit Codec `logic-mod-dag-input-v2`. Ausschließlich dessen Objekt 0 wird über den
bestehenden Payload-Capability-V1-Vertrag als versiegelte read-only Memfd auf
FD 6 bereitgestellt; der Worker-Inputcodec bleibt `artifact-bytes-v1`.
Nicht deklarierte Vorfahren werden nicht zusätzlich aufgenommen. Der Host
publiziert jedes akzeptierte Ergebnis als Result Artifact V1 mit Codec
`logic-mod-result-text-v1` und höchstens 16384 Bytes. Worker erhalten weder
Store-/Paketpfade noch Storezugriff oder zusätzliche FD-/Seccomp-Rechte.
Der unveränderte Hostvalidator prüft jedes Ergebnis gegen dessen Texteingabe;
dies belegt keine beliebige fachliche Verwendung des Vorgängerpayloads.

Nur vollständiger Erfolg publiziert `DAG_RECORD`: eine neue private reguläre
Datei, Modus 0600, atomar ohne Ersetzen. Das Elternverzeichnis muss existieren.
Replay verlangt eine reguläre Datei des effektiven Benutzers ohne Gruppen-/
Fremdrechte, mit genau einem Hardlink und höchstens 8388608 Bytes. Der Record
enthält private normalisierte Inputs, Ergebnisse und Response-Frames der
unveränderten eingebetteten Run-V1-Records, keine separat kopierten Payloads,
Hostpfade oder Workerdiagnostik. Er ist ein vertraulicher Klartextcontainer.

Replay startet keinen Worker, erstellt keinen Store und schreibt keine
Artefakte/Records. Es prüft Spec-Digest, aktuelle Paketautorisierung,
Knotenreihenfolge, Run-V1-Bindungen/Budgets/Hostvalidierung und alle Result-
sowie internen Input-Artefakte frisch aus dem vorhandenen Store. Es bestätigt
Transkriptkonsistenz unter aktueller Autorisierung, keine historische
Ausführungsattestation.

Erfolg liefert Exit 0 und eine Metadaten-JSON-Zeile auf stdout. Kontrolliertes
Worker-Versagen liefert denselben Vertrag mit Exit 2, `accepted:false`,
`status:"DAG_NODE_FAILED"` und `dag_id:null`. Die Ausführung stoppt sofort;
auch weitere unabhängige Knoten starten nicht. Bereits publizierte Result-
und interne Input-Artefakte können bestehen bleiben; ein DAG-Record entsteht
nicht. Andere Lauf-/Dateifehler liefern Exit 2 und
`{"error":"LogicModDagFailed"}` auf stderr; Replayfehler
`{"error":"DAG_REPLAY_MISMATCH"}`. Spec- und Preflight-Ablehnungen erfolgen
vor jeder Storemutation und erzeugen keinen Store/Record. Nach erfolgreichem
Preflight kann bereits die Worker-Vorbereitung einen Store anlegen; ein
anschließender Setupfehler vor dem ersten Workerstart, etwa bei `socketpair`,
kann ihn ohne DAG-Record hinterlassen. Scheitert erst die Recordpublikation,
können alle Artefakte bestehen bleiben. Es gibt keine Transaktion mit Rollback.

V2 bietet keine Parallelität, Retries, dynamischen Knoten, externen Root-
Payloads, Remote-Stores, Netzwerk, Streams, freien Codecs oder allgemeine
Orchestrierung. Chain V1, Phase-DAG und DAG-Scheduler bleiben eigenständige
Verträge. Belege: [Core/Replay/Preflight](../../tests/unit/logic_mod_artifact_dag_v2_tests.cpp)
und [reale CLI-Workflows](../../tests/integration/test_logic_mod_artifact_dag_v2.py).
Die DAG-V2-Preflight-Fixtures prüfen ein spätes signiertes Paket mit literalen
Nicht-ELF-Workerbytes. Eine Regression mit strukturell beschädigten nativen
ELF-Dateien wird in DAG V2 nicht unabhängig wiederholt.
Der unveränderte CLI-Beispielverifier führt diesen DAG nicht aus; seine
generierten Beispiele sind kein DAG-Laufbeleg.

## `logic-mod-artifact-dag-v3` – begrenzte parallele Artifact-Wellen

**IMPLEMENTIERT UND BELEGT:** V3 verwendet die strikte V2-Spec mit
`schema_version:2`, dieselben 1–16 signierten Knoten, Preflight-, Store-,
Payload-, Budget- und sicheren Dateiverträge. Die separate CLI lautet:

```text
bruce logic-mod-artifact-dag-v3 run SPEC_JSON --store LOCAL_STORE --record DAG_RECORD [--max-parallel-nodes 1..4]
bruce logic-mod-artifact-dag-v3 replay SPEC_JSON --store LOCAL_STORE --record DAG_RECORD
```

Die optionale Grenze steht ausschließlich am Ende von `run`; zulässig sind
genau die einzelnen Zeichen `1`, `2`, `3`, `4`, Standard ist `1`. Vorzeichen,
führende Nullen, Leerraum, zusätzliche Optionen und jede Paralleloption bei
`replay` ergeben vor jeglichem Dateizugriff Exit 2, leeres stdout und
`{"error":"InvalidLogicModDagArguments"}` auf stderr. Die Grenze ist reine
lokale Hoststeuerung und kein Spec-Feld; sie erscheint weder in Spec-Digest,
DAG-ID, Record noch öffentlicher JSON.

Bei Grenze 1 verwendet V3 die serielle V2-Ausführung. Bei höheren Grenzen
friert der Host die aktuell bereiten Knoten ein und wählt höchstens N davon
in der bestehenden stabilen topologischen Reihenfolge. Sie bilden eine
physische Welle unabhängiger Worker. Der Host wartet alle gestarteten Worker
ab und reapt sie, bevor er Ergebnisse übernimmt oder eine neue Welle beginnt.
Neu bereite Nachfolger starten erst in einer späteren Welle. Ergebnisse und
Record folgen weiterhin der kanonischen V2-Reihenfolge; physische Start- oder
Abschlusszeiten sind keine neue Recordordnung. Gleiche deterministische
Workertranskripte ergeben für Grenzen 1–4 bytegleiche Records und DAG-IDs.

Jeder Knoten behält eigene Autorisierung, Budgets und unterschiedliche
Input-/Output-Manifeste im gemeinsamen lokalen Store. Nichtwurzeln erhalten
weiterhin ausschließlich ihre frisch geprüften direkten Vorgänger über das
versiegelte read-only FD-6-Payload. Worker-, FD- und Seccomp-Rechte bleiben
unverändert. Ein kontrolliertes Knotenversagen beendet nach vollständig
abgewarteter Welle den DAG mit Exit 2, `accepted:false`,
`status:"DAG_NODE_FAILED"` und `dag_id:null`. Erfolgreiche Peers derselben
Welle dürfen Result Artifacts behalten; weitere Wellen starten nicht.
Alle bereits gestarteten Worker werden auch bei einer Laufzeitausnahme
abgewartet. Ein DAG-Record entsteht ausschließlich bei vollständigem Erfolg;
Artefakte und nach Preflight angelegte Stores können bei Fehlern bestehen
bleiben. Es gibt keinen Rollback.

Replay übernimmt unverändert die schreibgeschützte V2-Prüfung ohne Workerstart
und ohne Parallelsteuerung. V3 akzeptiert dabei das bestehende kanonische
V2-Recordformat; eine neue V3-Recordhülle gibt es nicht. Die öffentliche
Metadaten-JSON entspricht V2 mit `contract:"logic-mod-artifact-dag-v3"`;
`nodes` enthält gestartete Knoten in kanonischer logischer Reihenfolge.
Datei-/Lauffehler bleiben `LogicModDagFailed`, Replayfehler
`DAG_REPLAY_MISMATCH`. Details stehen im
[Formatvertrag](formats.md#logic-mod-artifact-dag-v3-kompatible-formate).

V3 ergänzt keine Retries, globalen Ressourcenbudgets, Fairness, Prioritäten,
dynamischen Knoten, Remote-Stores, Netzwerk-, GPU- oder allgemeine
Runtime-Orchestrierung. V2 und Phase-DAG V3 bleiben eigenständige Verträge.
Belege: [Core/Wellen/Replay](../../tests/unit/logic_mod_artifact_dag_v3_tests.cpp)
und [reale CLI-Workflows](../../tests/integration/test_logic_mod_artifact_dag_v3.py).
Der unveränderte CLI-Beispielverifier führt V3 nicht aus; seine generierten
Beispiele sind kein V3-Laufbeleg.

## `logic-mod-artifact-dag-v4` – lokale Hostbudgets für Artifact-Wellen

**IMPLEMENTIERT UND BELEGT:** V4 ergänzt die V3-Wellen um lokale Hostgrenzen.
Die strikte V2-Spec mit `schema_version:2`, 1–16 signierten Knoten,
Preflight, Knotenautorisierung, Einzelbudgets sowie Store-/Dateiverträge
bleiben bestehen. Die separate CLI lautet:

```text
bruce logic-mod-artifact-dag-v4 run SPEC_JSON --store LOCAL_STORE --record DAG_RECORD [--max-parallel-nodes N] [--max-total-wall-ms MS] [--max-total-payload-bytes BYTES] [--max-started-workers N] [--cancel-after-ms MS]
bruce logic-mod-artifact-dag-v4 replay SPEC_JSON --store LOCAL_STORE --record DAG_RECORD
```

Die ersten sechs Argumente nach dem Befehlsnamen haben die gezeigte feste
Reihenfolge. Danach darf `run` jede der folgenden Optionen höchstens einmal
in beliebiger Reihenfolge verwenden; `replay` akzeptiert keine davon.

| Option | Inklusiver Bereich | Standard |
| --- | --- | --- |
| `--max-parallel-nodes` | 1–4 | 1, serielle Wellen |
| `--max-total-wall-ms` | 1–60000 ms | keine globale Zeitgrenze |
| `--max-total-payload-bytes` | 0–8388608 Bytes | keine globale Bytegrenze |
| `--max-started-workers` | 1–16 | 16 |
| `--cancel-after-ms` | 1–60000 ms | kein Cancel-Timer |

Zahlen sind dezimal und ohne Vorzeichen, führende Nullen oder Leerraum.
Unbekannte, doppelte, unvollständige oder außerhalb des Bereichs liegende
Optionen sowie jede Run-Option bei Replay ergeben vor jeglichem Dateizugriff
Exit 2, leeres stdout und `{"error":"InvalidLogicModDagArguments"}` auf stderr.
Die Grenzen sind keine Spec-Felder und erscheinen weder in Spec-Digest,
DAG-ID, Erfolgsrecord noch öffentlicher JSON.

Vor jeder eingefrorenen Welle prüft der Host die monotone Laufzeit ab Eintritt
in den Core-Lauf einschließlich Preflight, die bisherigen Workerstarts und
die Summe der vollständigen internen Input-Dokumente für diese Welle.
Das Bytebudget zählt diese Input-Artifactbytes kumulativ je Empfängerknoten,
einschließlich ihrer Hülle und direkten Vorgängerergebnisse; es zählt weder
Workerprogramme noch alle Store-, Ausgabe- oder Prozessspeicherbytes.
Wurzeln ohne internes Input Artifact verbrauchen dabei null Bytes, sodass
auch Grenze 0 einen reinen Wurzellauf zulässt. Byte- und Startgrenzen erlauben
genaue Gleichheit. Die ganze Welle wird abgewiesen, falls sie die verbleibende
Byte- oder Startgrenze überschritte; die Welle wird nicht verkleinert und
keines ihrer Input Artifacts wird dann veröffentlicht.

Erreichte Zeitgrenzen werden ausschließlich vor einer Welle geprüft;
`cancel-after-ms` hat an dieser Grenze Vorrang vor den globalen Budgets.
Laufende Worker werden vollständig abgewartet und gereapt, auch wenn der Timer
währenddessen abläuft. Es gibt keinen globalen Mid-Worker-Kill. Eine letzte
erfolgreiche Welle kann deshalb trotz inzwischen erreichtem Timer noch
`ACCEPTED` ergeben. Bestehende Einzelbudgets der Worker gelten weiterhin.

Ein globaler Budgetstopp liefert Exit 2, `accepted:false`,
`status:"DAG_GLOBAL_BUDGET_EXHAUSTED"` und `dag_id:null`; der Cancel-Timer liefert
entsprechend `status:"DAG_HOST_CANCELLED"`. Bereits abgeschlossene Knoten und
Artefakte bleiben erhalten; weitere Wellen starten nicht. `node_order` umfasst
die gesamte logische Ordnung, `nodes` die vollständig abgeschlossenen Wellen
in dieser Ordnung. Bei Knotenversagen bleibt `DAG_NODE_FAILED` nach dem
vollständigen Abwarten der Welle erhalten. Nur ein vollständiger Erfolg
publiziert einen DAG-Record. Es gibt keinen Rollback.

Replay prüft unverändert schreibgeschützt und ohne Worker das kanonische
V2-Recordformat, das auch V3 verwendet. Identische deterministische
Workertranskripte ergeben bei erfolgreichen Läufen unabhängig von den
Hostgrenzen bytegleiche Records und DAG-IDs. Die Metadaten-JSON verwendet
`contract:"logic-mod-artifact-dag-v4"`; Eingaben, Ergebnisse, Payloadbytes,
Pfade, Zeitmessungen und Hostgrenzen werden nicht ausgegeben. Datei-/Lauffehler
bleiben `LogicModDagFailed`, Replayfehler `DAG_REPLAY_MISMATCH`. Siehe den
[Formatvertrag](formats.md#logic-mod-artifact-dag-v4-lokale-budgets-ohne-formatwechsel).

V4 ergänzt keine Retries, Fairness, Prioritäten, dynamischen Knoten,
Remote-Stores, GPU-/Netzwerk-Ausführung oder allgemeine Runtime-Orchestrierung.
Worker-/FD-/Seccomp-Rechte, V2/V3 und Phase-DAG bleiben unverändert.
Belege: [Core/Budgets/Replay](../../tests/unit/logic_mod_artifact_dag_v4_tests.cpp)
und [reale CLI-Workflows](../../tests/integration/test_logic_mod_artifact_dag_v4.py).
Der unveränderte CLI-Beispielverifier führt V4 nicht aus; seine generierten
Beispiele sind kein V4-Laufbeleg.

## `logic-mod-artifact-dag-v5` – hostseitiger Remote-Restore erwarteter Vorgänger

**IMPLEMENTIERT UND BELEGT:** V5 übernimmt die strikte V2-Spec, die V3-Wellen
und alle V4-Hostbudgets unverändert. Es ergänzt ausschließlich `run` um einen
expliziten, lokalen Hostschalter:

```text
bruce logic-mod-artifact-dag-v5 run SPEC_JSON --store LOCAL_STORE --record DAG_RECORD [--remote-restore HTTPS_BASE] [V4-RUN-OPTIONEN]
bruce logic-mod-artifact-dag-v5 replay SPEC_JSON --store LOCAL_STORE --record DAG_RECORD
```

`--remote-restore` steht höchstens einmal mit einem einzelnen, nichtleeren
Argument am Ende des festen Basisaufrufs. Es darf mit den fünf V4-Run-Optionen
in beliebiger Reihenfolge kombiniert werden. Die bestehenden V4-Grenzen und
Defaults gelten weiter: `--max-parallel-nodes` 1–4 (Standard 1),
`--max-total-wall-ms` 1–60000, `--max-total-payload-bytes` 0–8388608,
`--max-started-workers` 1–16 (Standard 16) und `--cancel-after-ms` 1–60000.
Die Restore-URL wird als strikte V4-HTTPS-Basis validiert. HTTP, Userinfo,
Query, Fragment, ungültige Hosts und leere Werte werden abgewiesen. Doppelte,
unvollständige oder unbekannte Optionen sowie **jede** Run-Option bei `replay`
scheitern vor Datei-, Store-, Netzwerk- oder Workerzugriff mit Exit 2, leerem
stdout und `{"error":"InvalidLogicModDagArguments"}` auf stderr.

Die Option ist eine Hostentscheidung und gehört weder in die JSON-Spec noch
in Spec-Digest, DAG-ID, Record oder öffentliche Erfolgs-JSON. V5 meldet
`contract:"logic-mod-artifact-dag-v5"`; die übrigen Metadatenfelder folgen
V4. URL, Remoteantworten, Payloadbytes, Pfade und Hostkontrollen werden nicht
ausgegeben.

Vor dem FD-6-Handoff eines Nichtwurzelknotens bestimmt der Host ausschließlich
aus dessen bereits gebundener erwarteter Manifest-ID, Objekt-Digest und Größe,
welches Vorgängerartifact nötig ist. Ist das lokale Manifest sicher vorhanden,
gibt es keinen Remote-GET. Fehlt das Manifest tatsächlich, darf V5 den
V4-Restore verwenden: Ein bereits lokal vorhandenes Objekt muss vorher Digest und Größe
erfüllen; ein fehlendes Objekt wird durch denselben Restore nachgeladen. Ein
unlesbares, beschädigtes, nicht reguläres oder symlink-basiertes Ziel ist kein
Fehlen: V5 bricht ohne Netzwerkzugriff und ohne Start des Nachfolgerworkers ab.
Nach einem erfolgreichen Restore verifiziert der Host die lokale READY/
ACCEPTED-Manifest-, Codec-, Digest-, Größen- und Ergebnisbindung erneut,
bevor er das unveränderte versiegelte read-only Payload-FD 6 übergibt.

Remote kann folglich keine neue Knotenidentität, neue Payloadreferenz oder
Ausführungswahrheit einführen. V5 nutzt nur den bestehenden V4-HTTPS-Restore;
es spiegelt nicht automatisch, speichert keine Credentials, stellt keine
Wurzelartefakte wieder her und gibt Workern weder Netzwerk noch Storezugriff.
TLS-, HTTP-, Envelope-, Manifest-, Codec-, Größen- oder Digestfehler lassen
den betroffenen Nachfolger ungestartet. Bereits veröffentlichte Artefakte
können wie bei V2–V4 bestehen bleiben; es gibt keinen Rollback.

`replay` bleibt das unveränderte schreibgeschützte V2-Record-Replay: Es
akzeptiert `--remote-restore` nicht, startet keinen Worker, führt keinen
Netzwerkzugriff aus und schreibt keine Artefakte. Bei gleichen deterministischen
Workertranskripten bleiben V2/V3/V4/V5-Erfolgsrecords bytegleich.
V5 ergänzt keine Retries, Remote-Ausführung, GPU, freien Code, externen
Payloadspeicher oder allgemeine Runtime-Orchestrierung. Belege:
[V5-Host-Core](../../src/mod/logic_mod_artifact_dag_v5.cpp),
[V5-CLI](../../apps/logic_mod_artifact_dag_v5_cli.hpp),
[Core-Verträge](../../tests/unit/logic_mod_artifact_dag_v5_tests.cpp) und
[TLS-/CLI-Workflows](../../tests/integration/test_logic_mod_artifact_dag_v5_cli.py).
Der unveränderte CLI-Beispielverifier führt V5 nicht aus.

## `logic-mod-artifact-dag-v6` – Linux-lokale Hostsupervision mit Diagnose-Sidecar

**IMPLEMENTIERT UND BELEGT:** V6 übernimmt V2-Spec, V3-Wellen, V4-Budgets und
den optionalen V5-Restore. Nur `run` verwendet einen Linux-lokalen, vom Host
besessenen Attempt; `replay` bleibt der bestehende offline, workerfreie
V2-Record-Check.

```text
bruce logic-mod-artifact-dag-v6 run SPEC_JSON --store LOCAL_STORE --record DAG_RECORD --report REPORT [--remote-restore HTTPS_BASE] [V4-RUN-OPTIONEN]
bruce logic-mod-artifact-dag-v6 replay SPEC_JSON --store LOCAL_STORE --record DAG_RECORD
```

`--report REPORT` ist bei `run` genau einmal Pflicht. Der Host erzeugt darin
exakt `REPORT.run/control/supervision-v6.jsonl` (Verzeichnisse `0700`, Datei
`0600`). Doppelte, leere, unvollständige oder unbekannte Optionen scheitern
mit Exit 2 vor Worker- oder Sidecarzugriff. `replay` nimmt ausschließlich die
sechs Grundargumente an und weist `--report`, `--remote-restore` sowie jede
V4-Run-Option vor Datei-, Netzwerk-, Worker- oder Sidecarzugriff ab.

V6 übernimmt die fünf V4-Optionen unverändert: `--max-parallel-nodes` 1–4
(Standard 1), `--max-total-wall-ms` 1–60000 ms, `--max-total-payload-bytes`
0–8388608 Bytes, `--max-started-workers` 1–16 (Standard 16) und
`--cancel-after-ms` 1–60000 ms. Ebenso ist `--remote-restore HTTPS_BASE`
weiterhin eine strikte, run-only V5-Hostoption. Diese Werte, die URL, der
Reportpfad und das Sidecar sind keine Spec-Felder und erscheinen weder in
Spec-Digest, DAG-ID, Record noch in der öffentlichen Ergebnis-JSON.

Ein gestarteter Worker läuft als bestehender signierter, versiegelter
Logic-Mod-Prozess mit gegebenenfalls read-only FD 6. Der Host überwacht nur
seine gebundene Linux-Prozessgruppe, reapt sie und kann einen beobachteten
Cancel erzwingen: maximal 1000 ms Grace, dann `SIGTERM`, maximal weitere
50 ms, dann `SIGKILL`. Dies ist eine feste Hostpolicy, kein Workerprotokoll
und keine CLI-Option. Es gibt keinen öffentlichen PID-Cancel und keine
Zusicherung eines cgroup- oder hostweiten Ressourcenaggregats.

Das Sidecar ist nur Diagnostik. Seine kanonischen, global sequenzierten JSONL-
Ereignisse beschreiben `ATTEMPT_STARTED`, optional `CANCEL_SENT`, `TERM_SENT`
und `KILL_SENT`, danach `REAPED` und `ATTEMPT_FINISHED`. Es enthält keine
Pfade, PIDs/PGIDs, Payloads, Frames, URLs, Signaturen oder Geheimnisse. Peak-
Speicher ist als `OBSERVED`, `UNAVAILABLE` oder `NOT_APPLICABLE` markiert;
ein fehlender Wert wird nie zu `0`. Beschädigte, syntaktisch abgeschnittene
oder unsichere Sidecars werden vom read-only Validator abgewiesen und verändern
weder Record noch Replay. Ein vollständig kanonisches, noch nicht terminales
Lifecycle-Präfix bleibt als Diagnosezwischenstand lesbar und ist kein
Erfolgsbeweis.

Terminale Sidecar-Gründe sind geschlossen: `READY_ACCEPTED`, `WORKER_FAILED`,
`WORKER_BLOCKED`, `HOST_CANCELLED`, `WALL_TIME_LIMIT`, `MEMORY_LIMIT`,
`INPUT_LIMIT`, `OUTPUT_LIMIT`, `PROCESS_START_FAILED`, `PROCESS_CRASHED`,
`PROTOCOL_ERROR`, `DIGEST_MISMATCH` und `SUPERVISION_UNAVAILABLE`. Die
implementierte Zuordnung verwendet bei nicht beweisbarer Speicherursache
`PROCESS_CRASHED`; V6 behauptet keine öffentlich belegte `MEMORY_LIMIT`-
Klassifikation. Erfolgreiche deterministische V2–V6-Läufe schreiben weiterhin
bytegleiche kanonische V2-Records; V6 fügt keinen Recordumschlag hinzu.

Belege: [V6-DAG-Core](../../src/mod/logic_mod_artifact_dag_v6.cpp),
[Attempt-Handle](../../src/mod/logic_mod_worker_attempt_v6.cpp),
[Sidecar](../../src/mod/logic_mod_supervision_log_v6.cpp),
[Core-Verträge (V6-CMake-Target)](../../tests/unit/logic_mod_artifact_dag_v5_tests.cpp),
[Sidecar-Verträge](../../tests/unit/logic_mod_supervision_log_v6_tests.cpp)
und [CLI-Workflows](../../tests/integration/test_logic_mod_artifact_dag_v6_cli.py).

## `runtime-orchestrator-v1` – lokale FIFO-Steuerung für V6-Läufe

**IMPLEMENTIERT UND BELEGT:** Der Runtime Orchestrator V1 ist ein einzelner,
lokaler foreground-Prozess. Er nimmt mehrere vollständig beschriebene
`logic-mod-artifact-dag-v6`-Läufe über eine begrenzte JSONL-Eingabe an und
delegiert jeden zugelassenen Lauf ausschließlich an die bestehende V6-Host-API.
Er startet keinen Worker selbst, signalisiert keine PID/PGID, liest keine
Workerframes und verändert weder V2–V6-Spec/Digest/DAG-ID noch erfolgreiche
kanonische Recordbytes oder Replay.

```text
bruce runtime-orchestrator-v1 serve \
  --max-queued-runs N --max-active-runs N \
  --max-reserved-parallel-nodes N --max-reserved-started-workers N \
  --max-reserved-payload-bytes N
```

Alle fünf positiven Grenzen sind Pflicht; es gibt weder Defaults noch eine
Kapazitätszusage. Sie sind Admission-Accounting für aktive Runs und deren
deklarierte V6-Reservierungen, keine harte globale CPU-, RAM-, Prozess-,
I/O-, Netzwerk- oder cgroup-Grenze. Insbesondere bildet die Summe von
V6-`RLIMIT_AS`-Werten keine Hostspeichergrenze ab.

Der Host verarbeitet genau ein kanonisches, begrenztes JSON-Objekt je
stdin-Zeile und gibt genau eine kanonische JSON-Antwort je stdout-Zeile aus.
`submit` verlangt die V6-Planfelder `spec`, `store`, `record`, `report`,
`max_parallel_nodes`, `max_total_wall_ms`, `max_total_payload_bytes` und
`max_started_workers` sowie die vier gleichlautenden
`reservation_max_*`-Werte. Optional bleiben `cancel_after_ms` und
`remote_restore` V6-Kontrollen. `status` und `cancel` nehmen ausschließlich
eine opaque `run_id`. Struktur- und Bereichsprüfung geschieht vor jedem
Plan-, Paket-, Truststore-, Store- oder Reportzugriff. EOF beendet nur die
Annahme; bereits zugelassene Läufe werden vollständig gedraint, bevor der
Host regulär endet.

**IMPLEMENTIERT UND BELEGT:** `admission-status` nimmt genau eine opaque
`run_id` und liest ausschließlich deren bereits vorhandenen In-Memory-Status.
Die Antwort ist eine kleine, redigierte Zulassungsdiagnose: `QUEUED_NOT_ADMITTED`,
`ADMISSION_RESERVED`, `ACTIVE`, `BLOCKED_HOST_RESERVATION`,
`CANCELLED_BEFORE_ADMISSION`, `TERMINAL_AFTER_ADMISSION` oder `UNKNOWN`.
Sie liefert nur Sequenzen und deklarierte Reservation; weder die native
Kennung noch Pfade, Plan, V6-Abschlusswerte oder Sidecar-Referenzen werden
projiziert. Die Aktion öffnet keine Datei und startet keinen Lauf. Eine
unbekannte gültige Kennung ergibt `RUN_NOT_FOUND`.

Die Admission ist deterministisches FIFO über `(enqueue_seq, run_id)`: Nur
der älteste wartende Auftrag wird betrachtet. Ein gültiger, momentan nicht
passender Kopf blockiert jüngere Aufträge absichtlich; ein Auftrag, dessen
einzelne Reservierung ein Hostmaximum überschreitet, endet als `BLOCKED` mit
`RESERVATION_EXCEEDS_HOST`. Es gibt keine Prioritäten, Backfilling, Aging,
Preemption, Retry oder Backoff.

Ein Queue-Cancel erzeugt keinen V6-Call, Worker, Sidecar, Store-Objekt oder
Record. Ein Active-Cancel setzt ausschließlich die private V6-Host-Latch,
zeigt `DRAINING` und hält die Reservierung bis V6 vollständig zurückkehrt;
erst danach wird `CANCELLED` veröffentlicht. Ein von V6 selbst erzeugtes
`DAG_HOST_CANCELLED` ohne Orchestrator-Latch wird als
`FAILED/V6_CANCELLED_EXTERNALLY` geführt. Öffentliche Statusantworten
enthalten nur opaque Kennungen, Sequenzen, Zustand/Grund, Reservation,
begrenzte V6-Abschlusswerte und eine opaque Sidecar-Referenz. Sie enthalten
keine Pfade, URLs, PID/PGID, Credentials, Frames, Payloads, Signaturen oder
Rohtelemetrie; ein Sidecar ist Diagnose und niemals Statusautorität.

V1 ist nicht persistent: Beim Prozessende gehen Queue, Reservierungen,
Latches und Status verloren. Es gibt keine Queue-Rekonstruktion, Recovery,
Record-/Sidecar-Reparatur, Artifact-Rollback, Socket-, HTTP-, RPC-,
WebSocket-, MCP- oder Mehrbenutzer-Schnittstelle. Ein V6-Record oder
Artefakt, das vor dem Ende bereits veröffentlicht war, behält allein seinen
bestehenden V6-Vertrag.

Belege: [Runtime-Core](../../src/runtime/runtime_orchestrator_v1.cpp),
[foreground CLI](../../apps/runtime_orchestrator_v1_cli.hpp),
[Controller-Verträge](../../tests/unit/runtime_orchestrator_v1_tests.cpp) und
[CLI-/Hostende-Workflows](../../tests/integration/test_runtime_orchestrator_v1_cli.py).
Siehe auch [Admission Diagnostics V1](../contracts/09-admission-diagnostics-v1.md).

## `logic-mod-result-inspect` – lokales Ergebnis schreibgeschützt prüfen

**IMPLEMENTIERT UND BELEGT:** Der eigenständige Inspector liest einen bestehenden
Logic-Mod-Laufrecord und prüft dessen optionales Result Artifact V1 im lokalen
Store, ohne Sitzung, Registry, Paket, Truststore oder Workerstart.

```text
bruce logic-mod-result-inspect RUN_RECORD --store LOCAL_STORE
```

Genau drei Argumente nach dem Befehlsnamen sind erlaubt, in dieser Reihenfolge.
Fehlende, doppelte, vertauschte, zusätzliche oder unbekannte Optionen sowie
leere oder nur aus Whitespace bestehende Pfadargumente werden vor Dateizugriff
mit Exit 2 und leerem stdout abgewiesen. Pfadargumente dürfen nicht mit `--`
beginnen. `--store` ist auch bei Records ohne Ausgabe Pflicht; in diesem Fall
wird der Store nicht geöffnet.

`RUN_RECORD` muss eine lesbare reguläre Datei des effektiven Benutzers mit
exakt einem Hardlink und ohne Gruppen-/Andere-Berechtigungen sein (etwa Modus
0600), höchstens 131072 Bytes. Symlinks in jeder Pfadkomponente, `..`, fehlender
Dateiname, Verzeichnisse, FIFOs und Geräte werden abgewiesen. Der Inspector
liest ausschließlich über den zuvor geprüften Deskriptor, akzeptiert höchstens
131072 Bytes und weist Wachstum über diese Grenze ab. Ein Lesechunk kann dabei
Bytes über den akzeptierten Inhalt hinaus erfassen. Unsichere, fehlende, unlesbare oder
nichtkanonische Records ergeben denselben bereinigten `INVALID_RECORD`-Report.

Bei gültiger Argumentform folgt genau ein LF-terminiertes JSON-Objekt auf stdout
und Exit 0, auch bei `INVALID_RECORD`, fehlendem Store oder beschädigtem
Artefakt. Anwendungen müssen deshalb `status` auswerten. Die vollständigen
[Felder und Statusregeln](formats.md#logic-mod-result-inspector-v1-json)
definieren den Vertrag `logic-mod-result-inspect-v1`.

`READY` bedeutet erfolgreiche lokale Prüfung des festen Ergebnismanifests und
der Ergebnisbytes. Es bestätigt weder aktuelle Registryautorisierung noch
mathematisches Replay oder signierte historische Ausführung. `accepted` und
Präsenzfelder stammen aus dem geparsten Record. Der Inspector veröffentlicht,
repariert und schreibt nichts, erzeugt keinen Store und startet keinen Worker.
Er gibt keine lokalen Pfade, Eingabe-/Ergebnisbytes, Antwortframes oder
Workerdiagnostik aus. Lesen kann Zugriffszeiten verändern. Die getrennten
Manifest-/Verifikationslesevorgänge bilden keine atomare Momentaufnahme bei
konkurrierenden Änderungen.

Belege: [Inspectorverträge](../../tests/unit/logic_mod_result_inspector_v1_tests.cpp)
und [reale CLI-Workflows](../../tests/integration/test_logic_mod_result_inspector_v1.py)
mit Statusmatrix, unsicheren Recordpfaden, Zeitlimits und Vorher-/Nachhervergleich
der Fixtures (ohne atime). Der bestehende CLI-Beispielverifier führt diesen
Inspector nicht aus; generierte Beispiele ersetzen diese Laufbelege nicht.

## `language` – optionale deterministische Language-Mod

```text
bruce language interpret --text TEXT --train TRAIN.csv --holdout HOLDOUT.csv
               [--grammar v1|v2|v3] [--train-alias ALIAS --holdout-alias ALIAS]
               [--max-bits N] [--max-denominator N] [--max-candidates N]
               [--max-operations N] [--max-trace N] [--max-results N] --output DRAFT
bruce language select DRAFT --alternative ID --output SELECTED_DRAFT
bruce language execute DRAFT --output LANGUAGE_RUN
bruce language replay LANGUAGE_RUN
```

`interpret` liest beide CSV-Dateien begrenzt ein und schreibt ausschließlich einen kanonischen Entwurf. `--text`, `--train`, `--holdout` und `--output` sind Pflicht. Die sechs optionalen Grenzen entsprechen der rationalen Core-Anfrage: `--max-bits` (Standard 4096), `--max-denominator` (8), `--max-candidates` (100000), `--max-operations` (1000000), `--max-trace` (100002) und `--max-results` (128). Für diese Optionen gelten die bestehenden Grenzen der exakten Forschung. Andere Forschungs-, Sitzungs-, Replay- oder Ausführungsoptionen sind im Sprachpfad nicht zulässig. `interpret` startet den Core nicht.

Die Standardgrammatik `--grammar v1` erkennt genau zwei normalisierte Sätze: `Untersuche @train mit @holdout auf rationale Formeln.` und `Untersuche @left und @right auf rationale Formeln.` Die Eingabepfade heißen in beiden Fällen ausschließlich `--train` und `--holdout`. Beim ersten Satz werden ihre Bytes intern als `train`/`holdout` gebunden; beim zweiten als `left`/`right`, in derselben Reihenfolge. Die zweite Form liefert zwei Alternativen und erfordert `select`; `--alternative` nimmt exakt eine der ausgegebenen IDs. Nicht erkannter Text, ungültiges UTF-8 und eine fehlende Auswahl führen zu Exit 2 ohne neuen Export.

`--grammar v2` und `v3` wählen die separat versionierten kontrollierten
Grammatiken. Beide akzeptieren `--train-alias` und `--holdout-alias` (Standard
`train` und `holdout`) für die beiden expliziten CSV-Eingaben. V1 weist diese
Aliasoptionen mit `LanguageAliasOptionsRequireV2` ab. Unbekannte Grammatikwerte
führen zu `UnsupportedLanguageGrammar`; die Grammatikwahl macht den Sprachpfad
nicht zu einem allgemeinen Chat. Der neue `route`-Befehl ist ein separater
Tree-Router und verwendet diese Language-Grammatiken nicht.

`select` nimmt einen Entwurf als einziges Positionsargument sowie `--alternative` und `--output`. `execute` nimmt einen kanonischen, gegebenenfalls ausgewählten Entwurf und `--output`. `replay` nimmt nur den Sprachlauf als Positionsargument. Der Befehl existiert nur mit `BRUCE_BUILD_MOD_LANGUAGE=ON`; mit `OFF` folgt die normale `UnknownCommand: language`-Route. Entwurf und Lauf werden vor Verwendung kanonisch geprüft; alle Exporte sind exklusiv und überschreiben vorhandene Ziele nicht.

Die Antworten sind `language-interpretation-v1`, `language-selection-v1`, `language-execution-v1` und `language-replay-v1`. Die ersten beiden zeigen den Status `UNDERSTOOD` oder `AMBIGUOUS`, Grammatikversion, Alternativen, gewählte ID und Eingabealiase mit Digests. `execute` exportiert einen `BRUCE_MOD_RUN_V1`-Umschlag mit Language-Mod-Manifest, kanonischem Entwurf und dem unveränderten inneren `BRUCE_LANGUAGE_RUN_V1`. `replay` akzeptiert sowohl diesen Umschlag als auch direkte historische V1-Läufe. Dessen `core` enthält die geprüfte native Sitzung und den Core-Befund einschließlich `INCOMPLETE`, wenn ein Budget erschöpft ist. Dieser Sprachpfad setzt keine mathematische Wahrheit oder Gates und ist kein allgemeiner Chat. [Format und Bindung](formats.md#sprachentwurf-und-sprachlauf).

## `task-spec` – explizit gebundene Token-Aufgabe V1

```text
bruce task-spec create --train TRAIN.csv --holdout HOLDOUT.csv
                       --route rational-portfolio-v1 --token-table-version v1 --output SPEC
                       [--min N] [--max N] [--max-bits N] [--max-denominator N]
                       [--max-power N] [--max-degree N] [--max-trace-bytes N]
                       [--max-candidates N] [--max-operations N] [--max-trace N] [--max-results N]
bruce task-spec inspect SPEC
bruce task-spec execute SPEC --output RUN
bruce task-spec replay RUN
```

`create` verlangt alle fünf angegebenen Optionen. V1 akzeptiert ausschließlich die Route `rational-portfolio-v1` mit Token-Tabelle `v1`. Die beiden CSV-Dateien müssen verschieden sein; ihre Originalbytes werden als Assets `train` und `holdout` mit SHA-256-Digests eingebettet. Eine V1-Datei enthält genau die Token `Train`, `Holdout` und `Rational Portfolio` in dieser Reihenfolge; ihre versionsgebundenen IDs und ihre Reihenfolge sind Teil des Vertrags. Die Rollen `train` und `holdout` sind ausdrücklich an diese IDs und die gleichnamigen Asset-Aliase `train` beziehungsweise `holdout` gebunden; `route` hat keinen Asset-Alias. Aus freiem Text werden weder Pfade noch Rollen erraten. `inspect` zeigt die Bindungen, geordneten Token-IDs, Asset-Digests, Route, Domäne, Grenzen und Budgets, ohne den Core auszuführen. Eventuelle Alternativen müssen vor der Ausführung eindeutig ausgewählt sein.

Die Standardwerte für `--min`, `--max`, `--max-bits`, `--max-denominator`, `--max-power`, `--max-degree`, `--max-trace-bytes`, `--max-candidates`, `--max-operations`, `--max-trace` und `--max-results` sind `-8`, `8`, `4096`, `8`, `4`, `3`, `1048576`, `100000`, `1000000`, `100002` und `128`. Die Grenzen steuern die native rationale Portfolio-Forschung; erschöpfte Budgets können `INCOMPLETE` ergeben. Ungültige Grenzen werden abgewiesen. `execute` akzeptiert nur einen kanonischen Spec und `--output`; zusätzliche Forschungs-, Erfahrungs-, Lern-, Publikations- oder Gateoptionen sind keine TaskSpec-Optionen.

Bereits vor dem Spec-Export und bei der Kompilierung gelten: 1..16384 Bits, 1..16 als Nennergrenze, jeweils 1..8 für Potenz und Grad, 256..4194304 Tracebytes, 1..1000000 Kandidaten, 1..10000000 Operationen, 2..100002 Traceereignisse und 1..128 Ergebnisse. Das ganzzahlige Intervall von `--min` bis `--max` muss 1..4096 Werte enthalten. Diese Prüfungen gelten auch für eingelesene kanonische Artefakte; eine andere Token-Tabellenversion als `v1` wird unabhängig vom Erzeuger abgewiesen.

`create`/`inspect` liefern `tokenized-task-spec-v1`. Die kanonische Binärdatei beginnt mit `BRUCE_TOKENIZED_TASK_SPEC_V1`, gefolgt von Body-SHA-256 und längenpräfixierten Feldern. `execute`/`replay` liefern `tokenized-task-run-v1`; die Laufdatei beginnt mit `BRUCE_TOKENIZED_TASK_RUN_V1` und bindet den vollständigen Spec, seinen Digest, das native Bundle samt Digest, Phasen, Erklärung und Core-Evidenz. Beide Exporte sind exklusiv; vorhandene Dateien werden nicht überschrieben. `replay` benötigt die ursprünglichen CSV-Pfade nicht. Es prüft Header, kanonische Bytes und Digests, kompiliert die gebundenen Rollen erneut und replayt das native Bundle. Ein nicht passender Spec, veränderte Bytes oder abweichende native Evidenz führen zu Exit 2 statt zu einer umgedeuteten Antwort.

Im JSON stehen die äußeren Phasen von `RECEIVED` bis `COMPLETED` und unter `core` die unveränderte native Sitzung. Zu den erreichbaren nativen Status gehören beispielsweise `VERIFIED_ON_SAMPLES`, `REFUTED`, `INCOMPLETE`, `NO_CANDIDATE` und `AMBIGUOUS`; dies ist keine abschließende Statusliste. `NO_CANDIDATE` bezeichnet eine vollständig untersuchte Familie ohne passenden Kandidaten, `AMBIGUOUS` mehrere nicht eindeutig entschiedene Kandidaten. `general_proof`, `learn_gate` und `publish_gate` bleiben `false`. V1 ist weder freie Sprache noch eine Wahrheitsmaschine und aktiviert weder Regeln noch Gates. Ein Sample-Befund ist kein allgemeiner Beweis.

Beispiel mit zwei verschiedenen CSV-Dateien im Format `x,y`:

```bash
bruce task-spec create --train identity.csv --holdout identity-hold.csv \
  --route rational-portfolio-v1 --token-table-version v1 --output identity.task-spec
bruce task-spec inspect identity.task-spec
bruce task-spec execute identity.task-spec --output identity.task-run
bruce task-spec replay identity.task-run
```

Mit den festen `identity.csv`-/`identity-hold.csv`-Eingaben der [Dokumentationsprüfung](verify_examples.py) und den Standardbudgets ergibt dieser Lauf exakt `INCOMPLETE` mit `stop_reason: "ChildIncomplete"`; das Replay bewahrt diesen Befund.

## `nanocell` – optionaler statischer NanoCell-Mod

```text
bruce nanocell run --core HEX16 --active-index 0..7 --context-id ID
                   --context-file FILE --technique TECHNIQUE
                   [--max-operations N] [--max-trace-entries N] [--output NEU]
bruce nanocell replay RUN
```

Der Befehl existiert nur mit `BRUCE_BUILD_MOD_NANOCELL=ON`; mit `OFF` ist er weder im Katalog noch in der Hilfe registriert und folgt der normalen `UnknownCommand: nanocell`-Route. `run` verlangt die fünf erstgenannten Optionen. `--core` sind genau 16 Hexziffern für acht Bytes, ohne `0x` oder Trenner; Groß- und Kleinbuchstaben sind erlaubt. Der Index wählt eines dieser Bytes. `--context-id` ist ein gültiger Bezeichner und `--context-file` liefert die tatsächlichen, höchstens 4096 Kontextbytes; die CLI berechnet ihren SHA-256-Digest. Für die Referenztechniken `masked-match-v1` und `rotate-match-v1` enthält die Datei exakt `nanocell-reference-context-v1` (ohne Zeilenumbruch). `context-conflict-v1` verlangt dagegen `nanocell-reference-context-conflict-v1`; mit dem ersten Kontext ergibt sie regulär `CONTEXT_REJECTED`.

`--technique` akzeptiert ausschließlich diese drei eingebauten IDs; eigene Programme oder frei formulierte Techniken akzeptiert die CLI nicht. `--max-operations` ist standardmäßig 16, `--max-trace-entries` 18. Beide sind begrenzte UInt64-Werte; die NanoCell-Kernvalidierung erzwingt 0..16 Operationen und 2..18 Traceeinträge. Ein Budgetende ergibt `INCOMPLETE` mit Exit 0, keine automatische Fortsetzung. `--output` schreibt ausschließlich einen äußeren `BRUCE_MOD_RUN_V1`-Umschlag mit Manifest `nanocell`/`nanocell-mod-v1`, kanonischer NanoCell-Anfrage und unverändertem inneren `BRUCE_NANOCELL_RUN_V1`; ohne Option bleibt nur die JSON-Ausgabe. `replay` akzeptiert genau einen Bundlepfad: Neue äußere Umschläge müssen Manifest, Codecs, Digests und die Anfrage-zu-Run-Bindung bestehen; historische direkte V1-Dateien bleiben ein read-only Kompatibilitätsweg. Keine allgemeinen Sitzungs-, Forschungs- oder Gateoptionen gelten hier; `MATCHED`, `NO_MATCH`, `CONTEXT_REJECTED` und `INCOMPLETE` bleiben ausschließlich technische NanoCell-Status. [Format](formats.md#nanocell-kontext-und-bundle) · [Status und Felder](results-and-control.md#nanocell-ergebnis-und-replay).

## `phase-dag-v2` – begrenzte NanoCell-DAG-Ausführung

**IMPLEMENTIERT UND BELEGT:** mit `BRUCE_BUILD_MOD_NANOCELL=ON` stehen genau
folgende drei Aufrufe zur Verfügung; mit NanoCell OFF fehlen Befehl und Hilfe:

```text
bruce phase-dag-v2 execute SPEC --output BUNDLE
bruce phase-dag-v2 inspect BUNDLE
bruce phase-dag-v2 replay BUNDLE
```

`execute` liest den [strikten V1-Text-Spec](formats.md#phase-dag-v2-spec-und-bundle)
und die ausdrücklich benannten Kontexte, validiert den vollständigen DAG vor
Ausführung und verwendet ausschließlich den festen Adapter
`nanocell-phase-v1`/`v1` sowie das feste `evidence-gate-v1`/`v1`. Die
Ausführung ist seriell, deterministisch und auf höchstens 16 Knoten begrenzt.
Knotenblöcke werden nach ID kanonisiert;
unter ausführbaren Knoten entscheidet die lexikographisch kleinste ID.
Abhängigkeiten binden Vorgängerbelege; der feste NanoCell-Adapter übernimmt
keine Vorgängerbytes als neuen Kontext. Jeder Knoten verwendet seinen eigenen
expliziten Kontext und seine feste Technik. Ein Evidence Gate wertet nur die
Status- und Digest-Referenzen seiner direkten Vorgänger aus: mindestens ein
Vorgänger und ausschließlich `COMPLETE` ergeben `COMPLETE`, jeder andere
READY-Output ergibt `REJECTED`. Seine genaue Spec-Form steht im
[Formatvertrag](formats.md#phase-dag-v2-spec-und-bundle).

`--output` ist verpflichtend und steht genau nach `SPEC`; zusätzliche Optionen
und Argumente werden abgelehnt. Der Export veröffentlicht ein kanonisches
`BRUCE_PHASE_DAG_RUN_V2`-Bundle atomar und exklusiv ohne Link/Unlink-Fallback:
Das Ziel darf nicht existieren, sein Elternverzeichnis muss existieren.
Ein Dateifehler liefert Exit 2 und
keinen Erfolgsreport. JSON-Umleitung ersetzt dieses Bundle nicht.

Fehler vor der Veröffentlichung beim Validieren, Ausführen oder Packen sowie
eine Zielkollision erzeugen kein neues Bundle. Nach erfolgreicher atomarer
Veröffentlichung kann eine fehlgeschlagene Verzeichnissynchronisation dagegen
`ExportCommitDurabilityUnknown` mit Exit 2 melden, obwohl das vollständige Bundle
bereits sichtbar sein kann. Die CLI löscht es nicht als Rollback; Ziel und Inhalt
vor Wiederholung prüfen. [Dateivertrag](local-files.md#phase-dag-v2-veröffentlichung).

`inspect` und `replay` verlangen genau einen Bundlepfad, keine Optionen.
Beide lesen höchstens 5 MiB und bestätigen Header, kanonische Bytes, Digests
und Belege durch deterministische erneute Ausführung mit demselben festen
Adapterkatalog. Beide liefern denselben Metadatenreport wie `execute`, mit jeweils
anderem `action`, ohne Dateiänderung und ohne erneutes Öffnen des Spec oder
seiner ursprünglichen Kontextdateien. Die JSON-Receipts sind nach ID sortiert,
keine Aufzeichnung der zeitlichen Ausführungsreihenfolge.

Auch `FAILED`, `BLOCKED` oder `CANCELLED` sind gültige Teilbefunde mit Exit 0
und gespeichertem Bundle, sofern Ausführung und Export erfolgreich enden.
Die drei Aufrufe sind unabhängig von den allgemeinen Sitzungs- und Gateoptionen;
die CLI bietet keine Signal-/Cancellation-Option. Sie implementiert keinen
generischen Scheduler, beliebige Adapter, GPU-Ausführung, allgemeinen
Formelbeweiser oder freie Sprache. [JSON und Teilbefunde](results-and-control.md#phase-dag-v2-ergebnis-und-teilbefunde)
· [Gemessene Beispiele](examples.json).

## `phase-dag-v3` – portable Payload-Kanten

**IMPLEMENTIERT UND BELEGT:** mit `BRUCE_BUILD_MOD_NANOCELL=ON` sind diese
separaten drei Befehle verfügbar; mit NanoCell OFF fehlen Befehl und Hilfe:

```text
bruce phase-dag-v3 execute SPEC --output BUNDLE [--max-parallel-nodes N]
bruce phase-dag-v3 inspect BUNDLE
bruce phase-dag-v3 replay BUNDLE
```

`execute` liest den [V3-Text-Spec](formats.md#phase-dag-v3-spec-und-bundle)
mit `BRUCE_PHASE_DAG_SPEC_V3`, validiert alle Requests und führt höchstens
16 Knoten deterministisch aus. Der feste Katalog enthält genau
`nanocell-phase-v3`/`v1` und `payload-gate-v1`/`v1`; eine freie Adapter- oder
Versionswahl gibt es nicht. Der NanoCell-Knoten veröffentlicht einen kleinen
Ergebnispayload. Das Gate konsumiert die echten Payloadbytes aller direkten
READY-Vorgänger, in der sortierten `depends`-Reihenfolge; es erzeugt wiederum
einen Manifestpayload, den ein weiteres Gate konsumieren kann. Indirekte
Vorgänger werden nicht als Inputs übergeben. Jeder Payload ist auf 65536 Bytes
begrenzt und zählt zum kodierten Receiptbudget.

`--output` ist verpflichtend und steht genau nach `SPEC`. Optional folgt danach
`--max-parallel-nodes N` nur für `execute`: Standard 1, zulässig 1..4.
N besteht vollständig aus Dezimalziffern, ohne Vorzeichen, Leerzeichen oder
Restzeichen. Ungültige Zahlen, Überlauf und Werte außerhalb 1..4 liefern
`InvalidPhaseDagParallelLimit` mit Exit 2, ohne neues Bundle. Andere Optionen,
Argumentreihenfolgen und doppelte Optionen werden abgelehnt.

Unabhängige Adapter können in deterministisch gebildeten physischen Wellen
mit höchstens N gleichzeitigen Aufrufen laufen. Sie erhalten ausschließlich
bereits logisch abgeschlossene direkte Vorgängerinputs. Jede Welle wird ganz
abgewartet; Resultate werden zwischengespeichert und in der bisherigen
lexikographischen seriellen Kahn-Reihenfolge übernommen. Auch ein dadurch
früher ausführbarer Nachfolger behält seinen bisherigen Platz vor späteren
unabhängigen Knoten. Receipts und logischer Trace beschreiben diese kanonische
Ordnung, keine tatsächlichen Thread-Start- oder Abschlusszeiten.
Standard und explizite 1..4 erzeugen für denselben Spec bytegleiche Bundlebytes;
Replay bleibt seriell. Das Startbudget kann die mögliche Parallelität weiter
begrenzen. Keine Performanceverbesserung wird behauptet.

Das `BRUCE_PHASE_DAG_RUN_V3`-Bundle wird
atomar und exklusiv veröffentlicht; Ziel und Elternverzeichnis folgen dem
[vorhandenen Dateivertrag](local-files.md#phase-dag-v2-veröffentlichung).
Das Ziel darf nicht existieren. Vor Veröffentlichung erzeugen Fehler kein
Bundle; `ExportCommitDurabilityUnknown` kann nach Veröffentlichung Exit 2
melden, obwohl die Datei bereits sichtbar ist. Vor Wiederholung Inhalt prüfen.

`inspect` und `replay` nehmen genau einen Bundlepfad ohne Optionen an und lesen
höchstens 5 MiB. Beide prüfen kanonische V3-Bytes, alle Digests und bytegleiches
deterministisches Replay mit demselben festen Katalog. Spec und Kontextdateien
werden dafür nicht erneut benötigt. Beide schreiben keine Datei und liefern
nur den [Metadatenreport](results-and-control.md#phase-dag-v3-ergebnis-und-payloadgrenzen),
keine Payload-Rohbytes. Gültige Teilbefunde bleiben Exit 0.

V2 bleibt kompatibel und besitzt seine eigenen Marker, Befehle und Adapter.
V2- und V3-Bundles sind nicht austauschbar. Der MCP Inspector prüft
bestehende Bundles über die getrennten V2-/V3-Routen und eine explizite
Versionauswahl. Kein externer Payloadspeicher, Adapterplugin oder allgemeiner
Scheduler ist implementiert.
[Gemessene Beispiele](examples.json).

## `coordinate` – reversible Adressen

```text
bruce coordinate --x X --y Y
bruce coordinate --z Z
```

Genau eine der beiden Formen verwenden. X, Y und Z sind dezimale vorzeichenlose 64-Bit-Ganzzahlen. Negative Werte sind unzulässig. Vorwärts gilt `z=(x+y)*(x+y+1)/2+y`; ein nicht in UInt64 darstellbares Ergebnis führt zu `CoordinateOverflow`. Das ist eine Adresse, nicht der Grid-Wert `x^y`.

Ausgabe: `x`, `y`, `z` als Dezimalstrings. Sind beide Koordinaten kleiner als 8, kommt `local_z` als JSON-Zahl hinzu: `local_z=8*y+x`. Lokale und globale Adresse sind unterschiedliche Zuordnungen. `--z` erlaubt keine zusätzlichen Optionen; ansonsten sind `--x` und `--y` Pflicht.

## `calculate` – einzelne exakte Rechenoperation

```text
bruce calculate --op OP --left A --right B
                [--domain bigint|rational] [--max-bits N] [--modulus M]
```

| Option | Standard | Bedeutung / Grenze |
| --- | --- | --- |
| `--op` | Pflicht | `add`, `sub`, `mul`, `div`, `divmod`, `pow`, `modpow` |
| `--left`, `--right` | Pflicht | Zahlen der gewählten Domäne; rechts bei Potenzen UInt64-Exponent |
| `--domain` | `bigint` | `bigint` oder `rational`; kein `int64` |
| `--max-bits` | 4096 | 1..16384 Bits; Betrag des Zählers und Nenner getrennt begrenzt |
| `--modulus` | keiner | Positiver ganzzahliger Modul für `modpow`; bei anderen Operationen nicht verwenden |

`add/sub/mul/div` rechnen A+B, A−B, A×B bzw. A/B. BigInt-`div` verlangt Rest 0, sonst `NonIntegralDivision` mit Quotient und Rest. `divmod` ist nur für BigInt zulässig; es liefert A=B×q+r mit `0 <= r < abs(B)`, auch für negative Operanden. Division durch 0 scheitert.

`pow` verwendet einen nichtnegativen UInt64-Exponenten; negative und rationale Exponenten sind nicht unterstützt. `0^0` ergibt hier 1. `modpow` ist BigInt-only und berechnet `A^B mod M`; das Fehlen des Moduls scheitert beim Zahlenparser. Zwischenwerte und Resultate unterliegen den implementierten Domänenprüfungen, aber `--max-bits` ist kein Prozessspeicherlimit.

JSON: `domain`, `operation`, `max_bits` sowie `value` als String oder `quotient` und `remainder` als Strings. Dieser Befehl besitzt weder `--output` noch Replay-/Sitzungsoptionen.

## `grid` – einen Pfad auswerten

```text
bruce grid --path '2,6/4,3' [--mode exact|modular|metric]
           [--rule leaf-local-power|scaled-exponential-v1]
           [--stage 0..57]
           [--max-bits N] [--modulus M] [--zero-zero one|stop]
           [gemeinsame Sitzungsoptionen] [--compatibility-family session]
           [--output DATEI]
```

Ein Pfad besteht aus 1..64 Paaren `x,y`, getrennt durch `/`. Jede lokale Koordinate liegt in 0..7. Alle Schritte beschreiben die Adresse; ausgewertet wird ausschließlich das letzte Paar nach der gewählten Regel. Standard ist `x^y`: `2,6/4,3` ergibt deshalb 64. Es ist kein Potenzturm und keine Auswertung aller `64^Tiefe` Knoten.

| Option | Standard | Bedeutung |
| --- | --- | --- |
| `--path` | Pflicht | Lokaler Pfad |
| `--rule` | `leaf-local-power` | Geschlossene Auswahl: `leaf-local-power` für `x^y`, `scaled-exponential-v1` für `100*(2^stage-1)`; andere Kennungen ergeben `InvalidGridRule` |
| `--stage` | nicht gesetzt | Nur für `scaled-exponential-v1`: dezimale Stufe 0..57 unabhängig von Blatt-`y`; ohne Option bleibt `stage=leaf.y` |
| `--mode` | `exact` | `exact`: UInt64-Regelwert; `modular`: Rest; `metric`: exakte Bitlänge |
| `--max-bits` | 64 | 1..64; im Modus `exact` Ergebnisgrenze. In `metric` wird intern mit 64 Bits gerechnet, in `modular` begrenzt der Modul das Ergebnis; der Parameter wird trotzdem validiert |
| `--modulus` | keiner | In `modular` Pflicht, UInt64 >0; sonst verboten |
| `--zero-zero` | `one` | `one`: normale Regelauswertung (bei `leaf-local-power` gilt 0^0=1); `stop`: letztes Paar 0,0 beendet unter beiden Regeln nur diesen Zweig ohne Auswertung |

`metric` liefert `bit_length` und `metric_exact`, sonst erscheint `value`. Ein gestoppter Zweig liefert keines davon. `evaluations` ist 0 oder 1; `theoretical_space` ist eine symbolische Größenbeschreibung.

**IMPLEMENTIERT UND BELEGT:** `scaled-exponential-v1`
verwendet fest Basis 2 und Skala 100. Ohne `--stage` bestimmt Blatt-`y`
die Stufe 0..7 mit den Werten `0, 100, 300, 700, 1500, 3100, 6300, 12700`.
`bruce grid --path 2,3 --rule scaled-exponential-v1` liefert daher 700.
Mit `--stage N` gilt stattdessen die explizite Stufe 0..57; alle Pfadpaare
bleiben lokale Adressen mit Koordinaten 0..7. Beispielsweise liefert
`bruce grid --path 2,6 --rule scaled-exponential-v1 --stage 57`
den Wert 14411518807585587100. `--stage 0` liefert an einem nicht gestoppten
Blatt 0. Nichtnumerische, negative, überlaufende oder außerhalb 0..57 liegende
Werte ergeben `InvalidGridStage`; eine gültige Stage mit `leaf-local-power`
ergibt `StageRequiresScaledExponentialRule`.
Die Ausgabe trägt `rule: "SCALED_EXPONENTIAL_V1"` sowie `stage`, `base: 2`,
`scale: 100` und `formula: "100*(2^stage-1)"`; Trace und Replay binden diese
Regelinformation. Explizite Stufen ergänzen `stage_source: "explicit"` und
verwenden `grid-spec-v2` sowie `BRUCE_GRID_BUNDLE_V2`. Ohne `--stage`
bleiben Ausgabe, Cache-Schlüssel, Trace und V1-Einzelbundle bytekompatibel;
historische skalierte V1-Bundles verwenden weiterhin Blatt-`y`.

`exact` berechnet den begrenzten UInt64-Wert und verwirft ein Ergebnis über
`--max-bits` mit `IntegerBitLimit`. `metric` bestimmt die exakte Bitlänge
innerhalb UInt64 unabhängig von dieser Exact-Grenze (Wert 0 hat Bitlänge 0).
`modular` berechnet `(100 mod M) * ((2^stage mod M - 1) mod M) mod M`
mit sicherer Subtraktion und Multiplikation; auch M=1 ist zulässig.
Stage 57 benötigt 64 Ergebnisbits; `exact --max-bits 63` verwirft sie.
Bei `--zero-zero stop` stoppt weiterhin ausschließlich die Adresse
`(0,0)`; etwa `(2,0)` wird ohne `--stage` regulär zu 0 ausgewertet.

Die Skala ist abstrakt; die Regel ist kein physikalischer Blasensimulator.
NanoCell, Worker-, GPU- und Netzwerkausführung sowie freie Formeln oder
Stage-Ausdrücke werden dadurch nicht erweitert.
Belege: [Grid-Verträge](../../tests/unit/grid_tests.cpp),
[CLI-Workflows](../../tests/integration/test_cli.py) und
[Stage-Vertrag](../superpowers/specs/2026-10-16-scaled-exponential-grid-stage-v1-design.md).

## `grid-stage-sweep` – begrenzte Stufenreihe mit Diagnose

```text
bruce grid-stage-sweep --path '2,6' --from-stage A --to-stage B
                       [--mode exact|metric|modular] [--max-bits 1..64]
                       [--modulus M] [--cache-bytes 0..1048576]
                       [--debug] [--output RUN]
                       [gemeinsame Serialisierungs-/Verifikationsoptionen]
```

**IMPLEMENTIERT UND BELEGT:** Der eigenständige Sweep V1 verwendet fest
`scaled-exponential-v1`, also `100*(2^stage-1)`. Beide Grenzen sind Pflicht,
liegen in `0..57` und erfüllen `A <= B`; das zusammenhängende inklusive
Intervall enthält höchstens 58 Einträge. Nichtnumerische, negative,
überlaufende, vertauschte oder außerhalb liegende Grenzen ergeben
`InvalidStageRange`. Der bestehende lokale Pfadvertrag (1..64 Paare,
Koordinaten 0..7) bleibt erhalten; Blatt-`y` bestimmt keine Sweep-Stufe.
Intern gilt fest `zero_zero=one`: Auch der Pfad `0,0` wertet jede angeforderte
Stufe aus, ohne Stop-Sonderbehandlung.

Standard ist `exact` mit 64 Bits. Nur `IntegerBitLimit` im Exact-Modus wird
zum exportierbaren Teilbefund: `status: "INCOMPLETE"`,
`stop_reason: "IntegerBitLimit"` und `stopped_stage` bezeichnen die erste
nicht darstellbare Stufe. Alle vorherigen Einträge bleiben erhalten; für die
fehlerhafte Stufe gibt es keinen Ergebniseintrag. Beispielsweise behält
`--from-stage 0 --to-stage 57 --max-bits 63` genau 57 Einträge für 0..56 und
stoppt bei 57. Scheitert bereits die Startstufe, ist das Präfix leer.
Ein vollständiger Lauf meldet `COMPLETED`, `Completed` und
`stopped_stage: null`. `metric` liefert die exakte UInt64-Bitlänge unabhängig
von der Exact-Grenze; `modular` verlangt einen UInt64-Modulus >0. Die
bestehenden Modus-, Bitlimit- und Modulusprüfungen gelten auch hier.

Der JSON-Report `grid-stage-sweep-v1` enthält den gemeinsamen aufgelösten
`spec`, kanonischen `path`, Grenzen, `requested_entries`, `processed_entries`,
Cachepolitik/-budget/-statistik und Endstatus. `records` ist strikt nach
Stufe aufsteigend; jeder Eintrag enthält den nullbasierten `index`, `stage`,
`stage_source: "explicit"`, `cache_key_digest`, `cache_event`,
`status: "EVALUATED"` und das Grid-Ergebnis unter `result` (mit `value`
oder `bit_length`). Ohne `--debug` enthalten weder gespeicherte Einträge noch
JSON/Evidence deren Grid-Traces. Das wertlose Flag `--debug` ergänzt pro
Eintrag `trace`; Werte, Reihenfolge, Cache-Schlüssel, Ereignisse und
Cacheabrechnung bleiben gleich.

Jeder Lauf und jedes Replay beginnen mit einem frischen GridCache. Das
Bytebudget hat Standard 65536, 0 umgeht die Speicherung. Gezählt werden
kanonischer Schlüssel und vollständiges Ergebnisbundle vor dem Entfernen
nicht angeforderter Traces; es ist kein RAM-Limit. Die bestehende FIFO-Politik
mit höchstens 64 Einträgen gilt. Jede explizite Stage besitzt eine eigene
Cacheidentität; ohne Stufenwiederholungen entstehen regulär nur `MISS_*`.

`--output` verwendet den bestehenden exklusiven Export. `replay RUN`,
`assess-run RUN` und `inspect-run RUN` unterstützen den neuen
`BRUCE_GRID_STAGE_SWEEP_BUNDLE_V1`; die Bewertung bleibt `grid-evaluation`
ohne allgemeinen Beweisanspruch. Die gemeinsamen Optionen
`--serialization-cancellation`, `--verification`,
`--verification-cancellation`, `--max-replay-bytes`,
`--max-serialized-bytes` und `--max-session-checkpoints` sind zulässig.

V1 bietet keine freie Regel/Formel, Stage-Liste, Wiederholung, eigene
Zero-Zero-Politik, Resume-Quelle oder laufübergreifende Cachepersistenz.
Physikalische Blasenmodelle, NanoCell-, Worker-, GPU- oder Netzwerkausführung
sind kein Bestandteil. Belege: [Sweep-Verträge](../../tests/unit/grid_stage_sweep_tests.cpp),
[CLI-Workflows](../../tests/integration/test_cli.py),
[Bundlevertrag](formats.md#grid-stage-sweep-v1-evidenz).
Der unveränderte CLI-Beispielgenerator führt keinen Sweep aus; seine
generierten Beispiele belegen die bestehenden Szenarien, die genannten
Verträge und Workflows den Sweep.

## `grid-stage-verify` – feste Regel auf der endlichen Stage-Domäne

```text
bruce grid-stage-verify --from-sweep SWEEP.bruce [--output VERIFY.bruce]
```

**IMPLEMENTIERT UND BELEGT:** Verify V1 prüft ausschließlich die feste Regel
`100*(2^stage-1)` auf allen 58 expliziten Stages `0..57`. Die Quelle muss ein
vollständig replay-valider `BRUCE_GRID_STAGE_SWEEP_BUNDLE_V1` mit
`scaled-exponential-v1`, `COMPLETED`, `Completed`, ohne gestoppte Einträge
und mit genau 58 lückenlosen aufsteigenden Records sein. Zulässig sind
`exact` und `metric`, einschließlich gültiger Debug-/Cachevarianten.
Im Exact-Modus werden UInt64-Werte unabhängig neu berechnet und verglichen;
Metric vergleicht deren Bitlängen (Stage 0: 0, Stage 1: 7, Stage 57: 64).
Eine Bitlängengleichheit ist keine eigenständige Aussage über Wertgleichheit.

Der Report `grid-stage-verify-v1` enthält `formula`, `mode`, `sweep_digest`,
`claim_scope: "finite-uint64-scaled-exponential-stage-domain-0..57"`,
`verified_domain: "0..57"`, `checked_entries: 58`,
`status: "VERIFIED_ON_DOMAIN"`, `first_mismatch: null` und alle `records`.
Jeder Record nennt Stage, `expected` und `observed` als Dezimalstrings sowie
`matches`. Der Digest bindet die vollständigen ursprünglichen Sweepbytes.

Modular-, Teil-, falsch begrenzte, fremde und replay-ungültige Quellen
ergeben `InvalidSweepVerificationSource`, Exit 2 und kein Verify-Bundle.
Dateilese-/Größenfehler können bereits vorher auftreten. Nur `--from-sweep`
und `--output` sind zulässig; Verify übernimmt keine gemeinsamen
Sitzungsoptionen oder freie Modus-/Formelwahl. Der optionale Export verwendet
den bestehenden exklusiven Dateivertrag und überschreibt kein Ziel.

`replay VERIFY.bruce` rekonstruiert den eingebetteten Sweep und alle Vergleiche;
die ursprüngliche Quelldatei wird nicht benötigt. `inspect-run` und
`assess-run` prüfen ebenfalls Replay und erhalten Status sowie den genauen
endlichen Claim-Scope; Assessment meldet `proof_established: false`.

**IMPLEMENTIERT, ABER NICHT AKTUELL VERIFIZIERT:** Der defensive
`REFUTED`-Auswertungspfad besitzt keinen zulässigen aktuellen Sweep-V1-Eingang:
dessen obligatorisches Replay regeneriert bereits dieselbe feste Regel.
Die Darstellung eines ersten Unterschieds ist vorhanden, ein erfolgreicher
Refutationslauf samt Replay ist damit nicht nachgewiesen. Manipulierte
Sweepwerte werden auch mit neuem Hash bereits als Quelle abgewiesen.

Verify entdeckt keine Formel, beweist keinen Satz über unbegrenzte natürliche
Zahlen und erweitert weder Stage-Domäne noch Sweep-Resume, Debug-Traces,
NanoCell-, Worker-, GPU-, Netzwerk- oder physikalische Modelle.
Belege: [Verify-Verträge](../../tests/unit/grid_stage_verify_tests.cpp),
[CLI-Workflows](../../tests/integration/test_cli.py),
[Bundlevertrag](formats.md#grid-stage-verify-v1-evidenz) und
[Abnahmegrenzen](../superpowers/specs/2026-10-16-grid-stage-verify-v1-design.md).
Der unveränderte CLI-Beispielgenerator führt Verify nicht aus; seine
generierten Beispiele sind kein Nachweis dieses neuen Pfads.

## `grid-batch` – mehrere Pfade und begrenzter Cache

```text
bruce grid-batch --paths '2,6;4,3;2,6'
                 [--rule leaf-local-power|scaled-exponential-v1]
                 [--stage 0..57]
                 [--cache-bytes N] [--max-query-steps N]
                 [Grid-Modi und Sitzungsoptionen]
                 [--compatibility-family session] [--output DATEI]
```

`--paths` ersetzt `--path`. Es sind 1..1024 Pfade, getrennt durch `;`, mit insgesamt höchstens 262144 Bytes erlaubt. Leere Pfade sind ungültig. Alle verwenden dieselbe Regel, dieselben Grid-Modi und Grenzen. `--rule` hat dieselbe geschlossene Auswahl und denselben Standard wie bei `grid`.

**IMPLEMENTIERT UND BELEGT:** `--stage` hat dieselben Grenzen und Fehler wie
bei `grid` und gilt gemeinsam für alle Batch-Pfade; pro Anfrage gibt es keine
eigene Stage. Die explizite Stage ist Teil des Cache-Schlüssels, sodass dieselbe
Adresse bei verschiedenen Stufen getrennte Einträge und Werte erhält.
Unbegrenzte explizite Batches verwenden `BRUCE_GRID_BATCH_BUNDLE_V3`,
begrenzte explizite Batches `BRUCE_GRID_BATCH_BUNDLE_V4`, auch bei `COMPLETED`.
Die Headerwahl richtet sich nach der konfigurierten Ausführungsgrenze,
nicht nach dem Ergebnisstatus. Ohne `--stage` bleiben die bisherigen
unbegrenzten V1-/begrenzten V2-Bundles bytekompatibel; auch V2 kann
`COMPLETED` enthalten.

`--cache-bytes` ist 0..1048576, Standard 65536. 0 deaktiviert die Speicherung. Der Cache ist lokal zum Lauf, FIFO und auf höchstens 64 Einträge begrenzt. Gezählt werden kanonischer Schlüssel plus Ergebnisbundle, nicht gesamter RAM oder Verwaltungsobjekte. Gleicher Zahlenwert an anderer Adresse ist kein identischer Cacheeintrag.

Die Regelkennung ist Teil des Cache-Schlüssels und des Batch-Bundlefelds
`value_rule`; dieselbe Adresse unter verschiedenen Regeln teilt keinen
Cacheeintrag. Replay prüft die vollständige kanonische Evidenz einschließlich
Regel, Trace und Ergebnis. Eine neu berechnete Prüfsumme legitimiert keine
manipulierte Regel. [Cache-/Batch-Verträge](../../tests/unit/grid_cache_tests.cpp).

`--max-query-steps` ist optional, 0..1024. Ohne Angabe werden alle angegebenen Pfade verarbeitet. 0 erzeugt einen unvollständigen Lauf ohne bearbeitete Anfrage. Auch ein Cachetreffer zählt als Anfrage. Ein Teilbefund ist exportierbar und mit `grid-resume` fortsetzbar.

## `grid-resume` – unvollständigen Stapel fortsetzen

```text
bruce grid-resume --from-run PARTIAL.bruce --additional-queries N
                 [Sitzungsoptionen] [--compatibility-family session]
                 [--output NEU.bruce]
```

Beide fachlichen Optionen sind Pflicht. N liegt in 1..1024. Quelle muss ein replaybares, `INCOMPLETE`-Grid-Batch- oder Grid-Resume-Bundle sein. Abgeschlossene Quellen werden abgelehnt. Höchstens acht verschachtelte Fortsetzungsgenerationen sind erlaubt.

**IMPLEMENTIERT UND BELEGT:** Regel und optionale Stage werden ausschließlich
vom replayten Elternlauf geerbt; `grid-resume` besitzt weder `--rule` noch
`--stage`. Explizite Eltern führen zu `BRUCE_GRID_RESUME_BUNDLE_V2` mit
abgeleiteten Stage-Feldern, deren Übereinstimmung mit dem Elternlauf beim
Replay geprüft wird. Ohne explizite Eltern-Stage bleibt Resume V1
bytekompatibel. [Header und kanonische Felder](formats.md#grid-stage-evidenz).

Die alten Konfigurationen, Reihenfolge und Cacheparameter werden übernommen; dafür gibt es hier keine Änderungsoptionen. Der geprüfte bisherige Ablauf wird rekonstruiert, danach werden weitere Anfragen verarbeitet. `audit_evaluations` und `new_evaluations` trennen Rekonstruktionsaufwand und neue Auswertungen. Es ist keine RAM-Abbild-Wiederherstellung. Mehr angeforderte Schritte als verbleibende Pfade sind möglich; der Lauf endet nach dem letzten Pfad.

## `research` – gemeinsame Optionen

```text
bruce research --train TRAIN.csv [--holdout HOLD.csv] [Optionen]
               [--compatibility-family session]
bruce research --grid-train TRAIN.bruce [--grid-holdout HOLD.bruce] [Optionen]
               [--compatibility-family session]
```

Die Quellenformen dürfen nicht gemischt werden. Ohne `--strategy` gilt `affine`. Ohne Domäne ist reine CSV-affine Forschung `int64`; die anderen Standards stehen in der Strategietabelle.

| Option | Standard | Zulässigkeit / Wirkung |
| --- | --- | --- |
| `--train` | Pflicht bei CSV | 1..4096 Lernpaare |
| `--holdout` | nicht gesetzt | Zusätzliche disjunkte Prüfdaten; wenn angegeben, Datei nicht leer |
| `--strategy` | `affine` | Eine der zehn Familien unten |
| `--domain` | abhängig | `int64`, `bigint`, `rational` |
| `--planner` | abhängig | Ausschließlich zur Familie passende Planer |
| `--min`, `--max` | −8, 8; Ausdrucksfamilien 0, 1 | Inklusive ganzzahlige Grenzen für Parameterzähler/Konstanten; keine Grenze für Sample-X |
| `--max-candidates` | 100000 | 1..1000000 gestartete Kandidaten |
| `--max-operations` | 1000000 | 1..10000000 logische Recheneinheiten der Strategie |
| `--max-trace` | 100002 | 2..100002 Traceereignisse |
| `--max-results` | 128 | Int64: 1..4096; exakte Forschung: 1..128; Expression/Portfolio-Expression mit `rational-q-witness`: 1..256 |
| `--max-bits` | 4096 | Nur BigInt/rational, 1..16384 |
| `--max-denominator` | rational 8; Ausdrucksfamilien und BigInt 1 | 1..16; BigInt verlangt 1 |
| `--max-trace-bytes` | 1048576 | Nur BigInt/rational; 256..4194304 |
| `--max-power` | 4 bei Power/Portfolios | 1..8; nur Power/Portfolios |
| `--max-degree` | 3 | 0..8; nur Polynomial/Portfolios |
| `--max-nodes` | 7 | 1, 3, 5, 7 oder 9; nur Expression/Portfolio-Expression |
| `--operators` | `+-*/` | Nichtleere Auswahl eindeutiger Zeichen aus `+-*/^`; nur Ausdrucksfamilien |
| `--equivalence` | `syntax` bei Expression; `rational` bei Portfolio-Expression | Siehe Äquivalenz unten |
| `--expression-planner` | `grammar` | Nur Portfolio-Expression; `grammar`, `indexed-grammar`, `cached-grammar` |
| `--domain-cache` | `none` | `none` oder `equal-64`; siehe unten |
| `--experience` | keine | Snapshot ausschließlich zusammen mit `experience-order` |
| `--output` | keine | Replaybares Bundle, neuer Pfad |

Int64 verlangt `-1000000 <= min <= max <= 1000000`. Bei exakten Domänen darf die inklusive Breite `max-min+1` nur 1..4096 sein; Grenzen selbst müssen in die gewählte Bitgrenze passen. Rationale Parameter entstehen aus Zählern in diesem Intervall und Nennern 1..`max-denominator`, anschließend gekürzt und dedupliziert. `--max-denominator` beschränkt den Parametersuchraum, nicht allgemein die Nenner der eingegebenen Samples.

Ausdrucksfamilien verlangen zusätzlich `Breite * max-denominator <= 64`, rational-quadratic und portfolio-quadratic höchstens 32768 rohe Parameterslots. Polynome werden aus Daten abgeleitet; `min/max` begrenzen dabei zulässige Koeffizienten. Mehr Budget erweitert nicht automatisch die deklarierte Familie.

## Forschung: Strategie-/Planermatrix

| Strategie | Familie | Domäne standardmäßig / zulässig | Standardplaner; weitere |
| --- | --- | --- | --- |
| `affine` | a*x+b | int64; bigint, rational (Grid standardmäßig bigint) | `enumerate` |
| `power-affine` | a*x^p+b, p=1..max-power | bigint; rational | `enumerate`; `adaptive` |
| `polynomial` | Summe c_i*x^i, Grad 0..max-degree | bigint; rational | `interpolate` |
| `rational-linear` | (a*x+b)/(x+c) | rational ausschließlich | `solve` |
| `rational-quadratic` | (a*x+b)/(x²+c*x+d) | rational ausschließlich | `solve` |
| `expression` | Begrenzte Bäume aus x, Konstanten, binären Operatoren | rational ausschließlich | `grammar`; `indexed-grammar`, `cached-grammar` |
| `portfolio` | Power + Polynomial | bigint; rational | `portfolio` |
| `portfolio-rational` | Vorige + Rational-Linear | rational ausschließlich | `portfolio`; `adaptive-order`, `experience-order` |
| `portfolio-quadratic` | Vorige + Rational-Quadratic | rational ausschließlich | `portfolio`; `adaptive-order`, `experience-order` |
| `portfolio-expression` | Vorige + Expression | rational ausschließlich | `portfolio`; `adaptive-order`, `experience-order` |

`adaptive` entscheidet bei Power zwischen Ableitung und Enumeration. `interpolate` untersucht begrenzte Polynomgrade; `solve` löst die begrenzten Bruchfamilien. Es sind keine freien Formelsyntheseverfahren. Portfolios teilen Suchbudgets unter ihren Strategien. `adaptive-order` nutzt versionierte Datenheuristiken; `experience-order` ergänzt passende geprüfte Stimmen aus einem Snapshot. Das ändert die Suchreihenfolge und kann bei gleichem endlichem Budget andere Teilbefunde liefern.

Bei Ausdrucksbäumen zählt jeder Operand und jeder binäre Operator als Knoten. `^` ist nur mit literalem ganzzahligem Exponenten 2..8 zulässig; `x^x`, freie rationale Exponenten und negative Exponenten gehören nicht zu dieser Grammatik. Der Exponent muss im Konstantenvorrat verfügbar sein. `--max-power` steuert bei einem Portfolio dessen Power-Familie, nicht diese Ausdrucksexponenten; bei eigenständigem `expression` ist die Option verboten.

### Ausdrucksäquivalenz

| Wert | Bedeutung |
| --- | --- |
| `syntax` | Eigenständige Ausdruckssuche ohne algebraische Zusammenführung; im Portfolio nicht erlaubt |
| `rational` | Rationale Normalformen unter Erhalt von Ausschlussbedingungen |
| `rational-q` | Zusätzlich begrenzter Vergleich der rationalen Definitionsbereiche |
| `rational-q-modular` | Zusätzliche modulare Zertifikate für Definitionsbereichsvergleiche |
| `rational-q-witness` | Zusätzlich begrenzte Suche nach rationalen Unterscheidungszeugen; Resultatbudget höchstens 256 |

Nicht entschiedene Definitionsbereichsvergleiche dürfen nicht als bewiesene Gleichheit gelesen werden. `--domain-cache equal-64` ist ausschließlich mit `rational-q-modular` in einer Ausdrucksfamilie erlaubt; er speichert bewiesene Gleichheiten, höchstens 64 Einträge. Selbst ausdrücklich gesetztes `--domain-cache none` ist bei anderen Strategien bereits vom CLI-Parser untersagt.

`--experience` muss zu `experience-order` passen und umgekehrt. Ein Snapshot mit nichtpassendem Kontext kann 0 verwendete Einträge ergeben. Derselbe Lerninput zählt nicht als neue unabhängige Erfahrung. Keine automatische Regelpromotion, kein allgemeines Modelltraining.

## `evidence` – lokale Exact-Affine-Evidenzkette

```text
bruce evidence create --train FILE --holdout FILE --output FILE
  [--domain bigint|rational] [--min N] [--max N] [--max-bits N]
  [--max-denominator N] [--max-trace-bytes N] [--max-candidates N]
  [--max-operations N] [--max-trace N] [--max-results N]
bruce evidence replay CHAIN
```

`create` verlangt beide nichtleeren CSV-Quellen und einen neuen Ausgabepfad. Die Domäne ist standardmäßig `bigint`, wahlweise `rational`; nur die feste Exact-Route `strategy=affine`, `planner=enumerate` ist zulässig. Die optionalen Grenzen verwenden die bestehenden Exact-Affine-Standards und -Validierungen; andere Forschungs-, Sitzungs-, Strategie-, Planner-, Experience- oder Regeloptionen sind hier nicht zulässig. Die CSV-Dateien werden lokal begrenzt gelesen und ihre unveränderten Bytes in `BRUCE_EVIDENCE_CHAIN_V1` eingebettet. Der Export überschreibt keinen bestehenden Pfad. [Byteformat und Bindung](formats.md#exact-affine-evidenzkette).

Die feste Policy `affine-inverse-v1` und der Scope `short-circuit-training-then-holdout-v1` beschreiben den unveränderten Exact-Lauf. Jeder erreichte Kandidat wird zuerst an Training geprüft; die erste Kontradiktion beendet seine Prüfung. Die inverse Gegenprüfung läuft nur nach einer passenden Vorwärtsauswertung. Holdout beginnt nur nach bestandener Trainingsprüfung und wenn Resultat-/Budgetfluss es erreicht. Budgets können innerhalb eines Kandidaten stoppen; `INCOMPLETE` ist kein Abdeckungsnachweis.

`replay` nimmt genau einen lokalen Chain-Pfad und benötigt die ursprünglichen CSV-Dateien nicht. Es prüft die feste Policy, die eingebetteten Samples, das native Exact-Bundle und den Ergebnisabschnitt erneut; es ist kein allgemeiner `replay`-Auftrag und gehört nicht zu `ApplicationTask`. `declared-disjoint-v1` bezeichnet technisch geprüfte x-Disjunktheit plus lokale Deklaration, keine extern nachgewiesene Unabhängigkeit oder Datenherkunft. Kandidat, Score, `VERIFIED_ON_SAMPLES`, Digest und Replay sind kein allgemeiner mathematischer Beweis; die Kette öffnet kein Learn-, Publish- oder Regelaktivierungsgate. Weder Cloud/Veröffentlichung noch Strategy-Promotion, BSGS-, Private-Key- oder Mining-Behauptungen sind Teil dieses Pfads. [Ergebnisgrenzen](results-and-control.md#exact-affine-evidenzergebnis).

## Forschung mit Grid-Quellen

`--grid-train` und optional `--grid-holdout` erwarten vollständige exakte Grid-Batches oder vollständige Grid-Fortsetzungen, keine einzelnen `grid`-Bundles. Quellen werden replaygeprüft. Jede darf höchstens 2097152 Bytes haben.

`--grid-axis x|y` (Standard x) bestimmt den Sample-Eingang. Bei x variiert die Blattbasis, der Exponent bleibt fest; bei y variiert der Blattexponent, die Basis bleibt fest. Alle Pfade müssen denselben Präfix und dieselbe feste andere Koordinate besitzen. Spezifikation und Betriebsmodus müssen übereinstimmen. Gestoppte Blätter und doppelte/überlappende Achsenwerte sind nicht zulässig. `int64`-Forschung ist hier verboten.

`--max-total-operations N` ist nur hier erlaubt (1..10000000). Das spezielle Konto verrechnet Eingabebytes der Gridquellen, bei erfahrungsbasierter Planung auch Snapshotbytes, und danach Forschungsoperationen. Effektives Forschungsbudget: `min(max-operations, N - Importbytes)`. N muss größer als die Importbytes sein. Das ist ein explizites gemischtes Kostenmodell, keine Messung der echten CPU-Operationen des Imports.

## `compare` – rationale Polynombrüche vergleichen

```text
bruce compare --left-numerator 'A0,A1,...' --left-denominator 'B0,B1,...'
              --right-numerator 'C0,C1,...' --right-denominator 'D0,D1,...'
              [--domain-policy quadratic|modular|modular-witness]
              [--max-bits N] [--max-operations N] [--output DATEI]
              [Sitzungsoptionen] [--compatibility-family session]
```

Alle vier Koeffizientenlisten sind Pflicht, aufsteigend nach Potenz: `1,2,3` bedeutet 1+2x+3x². Koeffizienten sind exakte rationale Zahlen. Bis zu 65 Einträge pro Liste (Grad höchstens 64); Normalisierung kann zusätzliche Grenzen erreichen. Ein Nullpolynom im Nenner bedeutet leeren Definitionsbereich und wird entsprechend behandelt.

Standard: `quadratic`, 4096 Bits (1..16384), 1000000 Operationen (1..1000000). `quadratic` nutzt GCD und begrenzte rationale Wurzelanalyse, `modular` ergänzt Zertifikate über die Primzahlen 2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, `modular-witness` zusätzlich eine begrenzte Zeugenprüfung über gekürzte a/b mit a in −16..16 und b in 1..16. Ein nicht entscheidbarer Fall bleibt `UNDECIDED`; mehr Operatorbudget ist keine Zusage allgemeiner Entscheidbarkeit.

Wertgleichheit und Definitionsbereich werden getrennt berichtet. Beispiel: `(x²−1)/(x−1)` und `x+1` haben dieselbe gekürzte Formel, aber bei x=1 verschiedene Definitionsbereiche. Diese Unterscheidung bleibt erhalten.

## `compatibility` – numerische Auswahl mit Regeln

```text
bruce compatibility --rules RULES --candidates CANDIDATES.csv [Optionen]
                    [--compatibility-family session]
bruce compatibility --rules RULES --from-run RUN.bruce --at X [Optionen]
                    [--compatibility-family session]
```

Genau eine Kandidatenquelle. `--rules` ist immer Pflicht. `--at` ist bei `--from-run` Pflicht, sonst verboten; es ist ein Int64-Wert. Für Grid-Forschungsquellen zusätzlich 0..7. Forschungsquellen werden replaygeprüft und müssen abgeschlossen sein. Formeln werden bei X ausgewertet; Bruchwerte oder Ergebnisse außerhalb Int64 sind nicht projizierbar. Leere Kandidatenmengen und Polstellen sind keine erfolgreiche Auswahl.

| Option | Standard | Bereich |
| --- | --- | --- |
| `--max-candidates` | 4096 | 1..4096 |
| `--max-checks` | 1048576 | 1..1048576 Regelprüfungen |
| `--max-trace` | 65536 | 1..65536 Traceereignisse |
| `--output` | keine | Neuer Bundlepfad |

Die gemeinsamen Sitzungsoptionen und die optionale read-only Bestätigung
`--compatibility-family session` sind möglich. Die Bestätigung akzeptiert
ausschließlich `session` und wird direkt nach dem Optionsparsen, vor jedem
Datei-, Bundle- oder Storezugriff geprüft. Sie aktiviert kein Replay und kein
Budget; ohne `--verification replay` bleibt der native Standardmodus `none`.
CSV-, Exact- und Grid-Research verwenden dieselbe Bestätigung. Falsche oder
unbekannte Familienwerte werden vor dem Quellenlesen mit der bestehenden
Kompatibilitäts-Fehlerhülle abgewiesen. Die Bestätigung erzeugt keinen
Descriptor und verändert weder Ergebnis-/Fehlerbytes, Session- und
Checkpointsemantik noch Exportgrenzen. Der genaue Scope umfasst nur
`compare`, `grid`, `grid-batch`, `grid-resume`, `research` und diese generische
`compatibility`-Route; `compatibility-plan` und `compatibility-apply` bleiben
Transfer-Routen. Regel-/Kandidatendateien sind je auf 1048576 Bytes begrenzt.
Genaue Grammatiken und Auswahlsemantik stehen in [Dateiformate](formats.md).
Eine Evidence-ID in einer Regel ist ein Verweis, keine externe
Evidenzprüfung.

## `experience` – unveränderliche Erfahrungssnapshots

```text
bruce experience add --from-run RUN [--store ALT] --output NEU
                     [--compatibility-family transfer]
                     [--max-transfer-checkpoints N] [--transfer-cancellation none|checkpoints]
bruce experience inspect SNAPSHOT [--compatibility-family management]
                                  [--max-management-checkpoints N]
                                  [--management-cancellation none|checkpoints]
bruce experience revoke --store ALT --id ID --reason TEXT --output NEU
                     [--compatibility-family transfer]
                     [--max-transfer-checkpoints N] [--transfer-cancellation none|checkpoints]
bruce experience replace --store ALT --id ID --reason TEXT --from-run RUN --output NEU
                      [--compatibility-family transfer]
                      [--max-transfer-checkpoints N] [--transfer-cancellation none|checkpoints]
bruce experience plan-add --from-run RUN [--store ALT] [--max-source-bytes N]
                     [--max-store-bytes N] --output PLAN [--compatibility-family transfer]
                     [--max-transfer-checkpoints N] [--transfer-cancellation none|checkpoints]
bruce experience apply-add PLAN --from-run RUN [--store ALT] --output NEU
                     [--compatibility-family transfer]
                     [--max-transfer-checkpoints N] [--transfer-cancellation none|checkpoints]
```

`add` startet ohne `--store` einen neuen Snapshot oder erweitert einen alten. Zulässige Quellen sind unabhängige vollständige rationale Drei-/Vier-/Fünferportfolios mit Holdout und mindestens einer `VERIFIED_ON_SAMPLES`-Hypothese, auch eingebettet in Grid-Forschung. Erfahrungsgesteuerte Quellen dürfen nicht erneut ihre eigene Evidenzbasis bilden. Quellaudits prüfen ein begrenztes Operationsbudget von höchstens 1000000; Snapshot/Quellimporte höchstens 1048576 Bytes.

`inspect` prüft den Snapshot samt Kette und eingebetteter Evidenz und liefert JSON. Die optionale `--compatibility-family management`-Bestätigung prüft die bestehende Management-Familie vor dem Dateilesen und lässt Bericht, Fehlerhülle und Exit-Code bytegleich zum Aufruf ohne diese Option. `add`, `plan-add`, `apply-add`, `revoke` und `replace` akzeptieren zusätzlich die optionale, read-only Bestätigung `--compatibility-family transfer`. Sie akzeptiert ausschließlich `transfer`, prüft den Descriptor vor dem jeweiligen Quellen-, Plan- oder Storezugriff und fügt weder Descriptor noch neuen Wrapper in die Legacy-Antwort ein. Die fachlichen Prüfungen, Transfersteuerung, Snapshot-/Planbytes und exklusive Veröffentlichung bleiben unverändert. `revoke` deaktiviert einen aktiven Eintrag; `replace` deaktiviert ihn und fügt eine geprüfte Nachfolgequelle samt Korrekturverknüpfung ein. IDs kommen aus `records[].id`; keine erfundenen Positionsnummern verwenden.

Ein Korrekturgrund umfasst 1..256 Bytes, keine Zeilenumbrüche oder NUL-Zeichen. Höchstens 16 Ereignisse und acht historische Datensätze pro Snapshot; auch widerrufene/ersetzte Datensätze zählen zur historischen Grenze. Eine unveränderte Ersatzquelle und fremde doppelte Lerninputs werden zurückgewiesen.

Alle schreibenden Aktionen erfordern einen neuen `--output`-Pfad. Das kurze Hilfebeispiel `--store STORE ... --output STORE` ist kein In-place-Verfahren: Wenn beide denselben existierenden Pfad bezeichnen, scheitert der Export. Die CLI erzwingt bei revoke/replace `--store` nicht über `MissingOption`; ohne Quelle scheitert stattdessen die Snapshot-/ID-Validierung. Für sinnvolle Aufrufe ist die Angabe erforderlich.

Direktes `add`, `revoke`, `replace`, `plan-add` und `apply-add` akzeptieren die Transferoptionen `--max-transfer-checkpoints N` (UInt64, 0 zulässig) und `--transfer-cancellation none|checkpoints`. Bei Angabe mindestens einer Option umschließt `controlled-transfer-v1` den bisherigen Bericht mit dem Verbrauch; ohne diese Optionen bleiben direkter Bericht und Snapshotbytes unverändert. `inspect` erhält keine Transfer- oder Sitzungsoptionen und erwartet genau einen Positionsparameter. Die Transfer-Familienbestätigung akzeptiert ausschließlich den Wert `transfer`; `session`, `gate`, `management` und unbekannte Werte werden vor dem Datei-/Plan-/Storezugriff abgewiesen. Die Option ist auf die fünf mutierenden Experience-Routen und die beiden Compatibility-Transfer-Routen begrenzt. [Zählung, Veröffentlichung und Grenzen](transfer-control.md).

## `inspect-run`, `assess-run` und `replay`

Neu in Core 0.73: `bruce assess-run RUN.bruce` führt ebenfalls Replay aus und liefert den gemeinsamen Vertrag `application-assessment-v1`: Auftragstyp, Status, Stopgrund, `result_complete`, `claim_scope`, `proof_established` und `replay_checked`. Sampleforschung wird niemals zum allgemeinen Beweis hochgestuft. Ein Beweiskennzeichen beim Bruchvergleich bezieht sich nur auf die gelieferten Polynombrüche über Q. [Details und Beispiel](../implementation/153-result-assessment-usage.md). Genau ein Positionsparameter, keine Optionen.

```text
bruce inspect-run RUN.bruce
bruce replay RUN.bruce
```

Je genau ein Positionsparameter, keine weiteren Optionen. Beide lesen begrenzt, prüfen Bundle und Replay. `inspect-run` ist ausdrücklich keine billige Headerabfrage: Es rekonstruiert das Ergebnis und liefert die kompakte Zusammenfassung mit Typ, Status und Stopgrund. `replay` liefert das native Ergebnis-JSON; die äußere `execution`-Hülle des ursprünglichen CLI-Aufrufs wird nicht exportiert und erscheint hier nicht erneut.

Auch abgebrochene, aber gültig exportierte Läufe sind replaybar. Replay ist keine Fortsetzung; dafür gibt es nur den speziellen Grid-Fortsetzungspfad. Eine veränderte Prüfsumme oder abweichende rekonstruierte Rechnung scheitert. Mitgelieferte ältere Formate werden anhand ihres Version-Headers erkannt; unbekannte Versionen nicht automatisch migriert.

## `gate-run` – Forschungszulassung nach Replay

```text
bruce gate-run RUN.bruce [--compatibility-family gate] [--max-session-checkpoints N] [--max-replay-bytes N] [--cancellation none|checkpoints]
```

Genau ein Dateiparameter; optional `--compatibility-family gate`,
`--max-session-checkpoints N` und `--max-replay-bytes N`. Ohne die neue Option
bleibt der Legacy-Aufruf unverändert. Mit `--compatibility-family gate` wird
die bestehende Gate-Familie vor dem Einlesen bestätigt; die normale JSON-
Antwort, Fehlerhülle, Exit-Codes und Bytes bleiben identisch. Der Descriptor
wird nicht ausgegeben. Andere Familienwerte werden abgewiesen; die Option ist
nur für `gate-run` verfügbar. Die effektive Eingabegrenze ist min(N, 32 MiB),
standardmäßig 32 MiB. Überschreitung beim Lesen liefert `InputByteLimit` vor
Replay. [Details](../implementation/159-gate-input-budget-usage.md).
Zuerst wird der Lauf vollständig wiederhergestellt und geprüft. Fehler beim
Lesen oder Replay ergeben Exit 2. Eine reguläre Ablehnung im Gate ist ein
JSON-Befund mit Exit 0.

Ohne Budget bleibt die direkte Ausgabe erhalten. Mit `--max-session-checkpoints N` liefert `application-gates-budget-v1` die Felder `result` und `usage`. Replay und Quellenaudit teilen sich N Prüfpunkte unter REPLAYING. Budgetüberschreitung liefert Exit 2 ohne Teilbericht; weitere Grenzen siehe [Gate-Budget](../implementation/158-gate-budget-usage.md).

`application-gates-v1` enthält `kind`, `replay_checked`, `applicable`, `selection` und `experience_source`. Die beiden Entscheidungen enthalten `allowed` und `reason`. `selection` nutzt die gemeinsamen Forschungsregeln. `experience_source` nutzt die vorhandene Erfahrungsquellenprüfung einschließlich deren engeren Größen-, Format-, Unabhängigkeits- und Arbeitsgrenzen. Sie prüft keinen Zielspeicher. `learn_promotion`, `publish` und `general_proof` bleiben false.

Für andere Anwendungstypen als Int64-, exakte oder Grid-Forschung sind `applicable` und beide Zulassungen false (`UnsupportedApplicationKind`). Das ist keine Aussage über die Gültigkeit dieser Ergebnisse. [Details und Grenzen](../implementation/155-run-gates-usage.md).

Für `gate-run` aktiviert `--cancellation checkpoints` SIGINT/SIGTERM-Abbruch an vorhandenen Prüfpunkten. Standard ist `none`. Abbruch: `GateInspectionCancelled`, Exit 2, kein Teilbericht. Das Signal hat am Prüfpunkt Vorrang vor dem Budget. Dateilesen und maximale Signallatenz sind nicht garantiert steuerbar. [Details](../implementation/160-gate-signal-control-usage.md).

Seit Core 0.81 unterstützen auch einzelne Grid-Bundles die Gate-Prüfpunkt- und Signalsteuerung. Fachlich bleibt das Forschungsgate für sie nicht anwendbar. [Details](../implementation/161-single-grid-replay-control-usage.md).

## Lokale Regelprüfung

`rules-propose OLD NEW PROPOSAL` erstellt einen ungeprüften Vorschlag aus zwei gültigen Regeldateien. `rules-approve PROPOSAL CURRENT OUTPUT` zeigt beide Fassungen im Terminal und exportiert erst nach exakter Bestätigung. Alle drei Argumente sind Pflicht. Vollständige Referenz für jeden Parameter, Terminalverhalten, JSON-Felder, Paketformat, Fehler und ein durchgängiges Beispiel: [Lokale Regelprüfung](rule-review.md).

## Aktive Regeln und Journal

`rules-stage STORE NEW PROPOSAL` erstellt einen Vorschlag gegen die aktuelle Speichergeneration. `rules-activate STORE PROPOSAL` aktiviert nach exakter Terminalbestätigung und erneuter Basisprüfung. `rules-status STORE [--compatibility-family management] [--max-management-checkpoints N] [--management-cancellation none|checkpoints]` prüft und zeigt die Historie. Die Familienbestätigung akzeptiert ausschließlich `management`, validiert vor dem Storezugriff und verändert die Legacy-Ausgabe nicht.

`compatibility --rule-store STORE` verwendet die aktive Regelfassung anstelle von `--rules DATEI`; beide Optionen schließen sich aus. Alle übrigen Kompatibilitätsoptionen gelten weiter. [Vollständige Referenz: Parameter, Journal, Fehler, Beispiel und Vertrauensgrenzen](active-rules.md).

## Explizite Erfahrungsübergabe

`experience plan-add --from-run RUN [--store PREVIOUS] [--max-source-bytes N] [--max-store-bytes N] --output PLAN [--compatibility-family transfer]` prüft einen Import im Speicher und bindet seine Eingabebytes. Grenzen jeweils 0..1048576, Standard 1048576. Die optionale Familienbestätigung wird vor Quellen-/Basislesen validiert und lässt die Ausgabe- und Planbytes unverändert.

`experience apply-add PLAN --from-run RUN [--store PREVIOUS] --output NEW [--compatibility-family transfer]` prüft den Plan, beide Digests und den Import erneut und exportiert exklusiv. Die Familienbestätigung erfolgt vor Plan-/Quellen-/Basislesen. Keine Budgetüberschreibung bei Ausführung; keine Regelaktivierung. [Vollständige Referenz und Beispiel](experience-transfer.md).

## Geplante Z-Auswahl

`compatibility-plan --from-run SOURCE --rules RULES --at X [--max-source-bytes N] [--max-candidates N] [--max-checks N] [--max-trace N] --output PLAN [--compatibility-family transfer]` bindet Quelle, kanonische Regeln, Auswertungsstelle und Budgets. Die Familienbestätigung wird vor Quellen-/Regellesen geprüft.

`compatibility-apply PLAN --from-run SOURCE --rules RULES [--output RUN] [--compatibility-family transfer]` prüft Bindungen und fachliche Quelle erneut. Die Familienbestätigung wird vor Plan-/Quellen-/Regellesen geprüft. Bei beiden Befehlen ist `--rule-store STORE` alternativ zu `--rules` zulässig. Anwendung übernimmt Budgets ausschließlich aus dem Plan. [Standards, Grenzen, Status, Format und vollständiges Beispiel](compatibility-transfer.md).

## Übergabesteuerung ab Core 0.94

Alle sieben Transfer-Routen akzeptieren zusätzlich die optionale read-only Bestätigung `--compatibility-family transfer`: `experience add`, `experience revoke`, `experience replace`, `experience plan-add`, `experience apply-add`, `compatibility-plan` und `compatibility-apply`. Sie wird vor Datei-/Plan-/Storezugriff validiert, akzeptiert keinen anderen Familienwert und erzeugt keinen Descriptor oder neuen Ergebniswrapper. Die bestehende Transfersteuerung `--max-transfer-checkpoints N` (UInt64, 0 zulässig) und `--transfer-cancellation none|checkpoints` bleibt unabhängig kombinierbar; bei Angabe einer Steuerungsoption umschließt `controlled-transfer-v1` den bisherigen Bericht mit dem Verbrauch. [Vollständiger Vertrag und Grenzen](transfer-control.md).

Die beiden Planbefehle akzeptieren seit Core 0.95 zusätzlich `--max-transfer-checkpoints N` und `--transfer-cancellation none|checkpoints`; die [Transfersteuerung](transfer-control.md) beschreibt Ausgabehülle, Zählung und Grenzen.

## `controller-observability-v1` – lokale, lesende Controller-Evidenz

**IMPLEMENTIERT UND BELEGT:** Dieser Befehl projiziert ausschließlich bereits
vorhandene lokale V5- oder V6-Evidenz in die redigierte,
quellmarkierte Crosswalk-Ansicht. Er startet keine Worker, sendet kein Cancel
und publiziert keine Records. Außerhalb der expliziten Bundle-/Record-
Evidenzroute öffnet er keine zusätzlichen Stores, Sidecars, Reports, Specs oder
Replay-Eingaben.

```text
bruce controller-observability-v1 v5 BUNDLE
bruce controller-observability-v1 v6 RECORD
```

`v5` bindet die übergebene Bundle-Datei über den bestehenden sicheren
V5-Inspektionspfad an ihre Evidenz: den privaten Bundle-Report, die gebundenen
Request-Dateien und `BUNDLE.run`. Alle diese lokalen Leseschritte sind
read-only, no-follow und vor Bytezugriff als reguläre, begrenzte Dateien
geprüft; sie öffnen keine Stores, Sidecars, Reports außerhalb des Bundlepfads,
Specs oder Replays. `v6` liest allein den kanonischen V6-Record über einen
privaten descriptor-relativen No-Follow-Leser; Symlinks, Nicht-Regular-Files,
öffentliche Modi, fremde Eigentümer, Hardlinks, Übergröße und nichtkanonische
Bytes werden abgewiesen. Jede ungültige Evidenz ergibt ausschließlich
`{"contract":"controller-observability-v1","ok":false,"error":"INVALID_EVIDENCE"}`
auf stderr ohne Pfad- oder Betriebssystemfehler.

Erfolgreiche Ausgabe entsteht ausschließlich durch den bestehenden Crosswalk-
Renderer und enthält feste Quelltokens sowie geschlossene Enum-Fakten. Native
IDs, Pfade, URLs, PIDs, Payloads, Frames, Signaturen, Credentials und
Sidecar-Inhalte erscheinen nicht. Runtime-Orchestrator-Offline-, Store-,
Spec-, Report-, Sidecar- oder Replay-Argumente sind keine Schnittstelle dieses
Befehls.
