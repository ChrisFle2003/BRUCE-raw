# PhaseDagV1 – deterministischer Ausführungs- und Replayvertrag

## Status und Evidenz

`IMPLEMENTIERT UND BELEGT`: `ApplicationTask` besitzt acht native Varianten und `ExecutionSession` führt genau einen Lauf linear durch `CREATED → EXECUTING → SERIALIZING → REPLAYING → READY`. Nachweis: `include/bruce/application/application_task.hpp`, `include/bruce/application/execution_session.hpp`, `src/application/execution_session.cpp` und `docs/adr/012-checked-execution-phases.md`.

`IMPLEMENTIERT UND BELEGT`: `NanoCellExecutorV1` ist ein eigenständiger Einzelzell-Referenzpfad mit drei festen Techniken, eigenem Bundle und Replay. Er ist nicht in `ApplicationTask`, CLI oder `ExecutionSession` eingebunden. Nachweis: `include/bruce/nanocell/nanocell.hpp`, `src/nanocell/nanocell.cpp`, `tests/unit/nanocell_tests.cpp` und `docs/superpowers/specs/2026-09-27-nanocell-executor-v1-design.md`.

`DOKUMENTIERT / NICHT IM CODE BESTÄTIGT`: `BRUCE Affine Phase-DAG.md` beschreibt einen erheblich größeren Affine-, Provenienz-, Zyklen-, Relations- und Phasenentwurf. Die M7-Roadmap nennt Execution DAG, Semantic Graph und SCC-Verdichtung als Zielarchitektur. Nachweis: `BRUCE Affine Phase-DAG.md`, `docs/implementation/04-roadmap.md:374-387` und `docs/implementation/75-current-architecture-and-milestones.md:10-24`.

`VORSCHLAG`: Dieser Vertrag begrenzt eine erste `PhaseDagV1` auf die deterministische Orchestrierung einer kleinen, azyklischen Menge versionierter Phasenadapter. Er ist weder eine Erweiterung der vorhandenen linearen Sitzung noch ein Versuch, den gesamten Affine-Entwurf zu implementieren.

`NICHT TEIL DIESES PAKETS`: Änderungen an `ApplicationTask`, `ExecutionSession`, CLI, Sprache, Gates, Experience, semantische Zustandsgraphen, Zyklen/SCC, Affinität, Relationsextraktion, Formelgraphen, signierte oder gaußsche Phasen, Parallelität, GPU, Netzwerk, Datenbank, LLM und kryptografische Suche.

## Ziel und Erfolgskriterium

`PhaseDagV1` soll später beweisen können, dass mehrere **bereits separat definierte** Phasen in einer festen Abhängigkeitsreihenfolge ausgeführt und als Gesamtlauf reproduziert werden. Dabei bleiben:

- der fachliche Ausgabewert einer Phase,
- deren native Evidenz beziehungsweise Bundle,
- ihr Adaptervertrag und dessen Version,
- ihr Status sowie
- ihre Herkunft aus Vorgängerphasen

getrennt erhalten.

Ein gemeinsamer Ausgabewert zweier Phasen ist keine gemeinsame Provenienz. Ein `MATCHED` aus NanoCell ist nur ein lokaler Byte-/Kontextbefund. Er ist weder Wahrheit, allgemeiner Beweis noch eine Learn-/Publish-Freigabe und erhält durch Einordnung in eine DAG keinen stärkeren Status.

Erfolg von V1 bedeutet ausschließlich: Eine zulässige, kleine Ausführungs-DAG wird kanonisch validiert, in deterministischer Reihenfolge abgearbeitet und mit vollständigen Receipts replayt. Es bedeutet nicht, dass BRUCE einen allgemeinen Scheduler, eine semantische Graphengine oder eine echte Phase-Algebra besitzt.

## Abgrenzung: drei verschiedene Graphbegriffe

| Begriff | V1-Status | Bedeutung |
| --- | --- | --- |
| Ausführungs-DAG | **VORSCHLAG für V1** | Knoten ordnen konkrete Adapteraufrufe durch Datenabhängigkeiten; sie ist immer azyklisch. |
| Semantischer Zustandsgraph | nicht V1 | Mathematische Zustände und echte Zyklen. Ein gleicher Wert ist nicht automatisch gleiche Provenienz. |
| Affine/Formula-/Phasen-/Relationsgraphen | nicht V1 | Fachliche Modellierungen aus dem großen Konzept; sie benötigen eigene Werte-, Äquivalenz- und Beweisverträge. |

Damit darf ein zyklisches fachliches Modell niemals durch „erneuten Start“ in die V1-Ausführungs-DAG geschmuggelt werden. Ein Zyklus in den V1-Abhängigkeiten ist ein Validierungsfehler vor jeder Phase. Eine spätere semantische Zyklusbehandlung benötigt separat einen Zustandsgraph- und SCC-Vertrag.

## Verglichene Ansätze

| Ansatz | Vorteil | Risiko | Entscheidung |
| --- | --- | --- | --- |
| Bestehende `ExecutionSession` direkt zu einem Scheduler ausbauen | vorhandene Checked-Execution sichtbar nutzen | eine lineare Application-Session würde gleichzeitig Task-, Gate-, Fehler- und Bundle-Semantik verändern | nicht V1 |
| NanoCell-Adapter und DAG gemeinsam implementieren | sichtbarer End-to-End-Demoablauf | zwei neue Verträge wären ungetestet gekoppelt; NanoCell-Match könnte falsch als globale Entscheidung erscheinen | nicht V1 |
| Eigenständiger Phase-DAG-Referenzkern mit festen Adaptergrenzen | kleine Testfläche, klare Receipts und Replay, spätere Adapter bleiben austauschbar | zunächst keine CLI und kein NanoCell-DAG-Aufruf | **V1** |

## Datenvertrag V1

Alle unten genannten Versionen, Grenzen und Reihenfolgen sind Teil des zukünftigen V1-Vertrags. Erweiterungen erfordern eine neue Version; sie verändern kein V1-Bundle stillschweigend.

### Identitäten und Grenzen

```text
dag_version: "phase-dag-v1"
node_id: 1..64 ASCII-Zeichen [A-Za-z0-9_.-]
adapter_id: 1..64 ASCII-Zeichen [A-Za-z0-9_.-]
adapter_version: 1..64 ASCII-Zeichen [A-Za-z0-9_.-]
node_count: 1..16
dependency_count_per_node: 0..15
total_edge_count: 0..64
```

Node-IDs sind in einer DAG eindeutig. Jede Abhängigkeit referenziert exakt eine andere Node-ID; Selbstabhängigkeiten, doppelte Abhängigkeiten, unbekannte IDs, mehr als 64 Kanten und Zyklen sind Eingabefehler. Die kanonische Knotenreihenfolge ist aufsteigend nach byteweiser ASCII-Node-ID. Die kanonische Abhängigkeitsliste jedes Knotens ist ebenfalls aufsteigend nach dieser Ordnung.

### PhaseDagSpecV1

```text
dag_version: "phase-dag-v1"
dag_id: 1..64 ASCII-Zeichen [A-Za-z0-9_.-]
nodes: 1..16 PhaseNodeSpecV1, kanonisch nach node_id sortiert
max_started_nodes: UInt64 0..16
max_receipt_bytes: UInt64 1..262144
max_trace_entries: UInt64 2..512
```

`max_started_nodes` begrenzt ausschließlich neu gestartete Knoten. Ein Knoten, der wegen eines blockierten Vorgängers nie startet, verbraucht dieses Budget nicht. `max_receipt_bytes` begrenzt den gesamten kanonischen DAG-Receipt inklusive verschachtelter Evidenzreferenzen, nicht die interne Rechenzeit oder den Speicher eines Adapters. `max_trace_entries` ist mindestens zwei, weil jeder gültige Lauf wenigstens `VALIDATED` und `COMPLETED` protokolliert; native Adaptertraces bleiben deren eigener Vertrag.

### PhaseNodeSpecV1

```text
node_id
adapter_id
adapter_version
dependencies: kanonische Liste von node_id
request_bytes: 0..65536 rohe Bytes
request_digest: SHA-256(request_bytes), 64 Kleinbuchstaben-Hexzeichen
```

`request_bytes` gehören ausschließlich dem angegebenen Adapter. Die DAG interpretiert sie nicht. Ihre Digestbindung verhindert nur die Verwechslung von Request und Receipt; sie ist keine Authentifizierung. Ein Adapter darf nie über versteckte globale Eingaben, Dateipfade oder Netzwerkzustand verfügen. Solche Adapter sind V1-unzulässig.

### PhaseInputRefV1 und PhaseOutputRefV1

Ein Adapter erhält keine unstrukturierten Vorgängerobjekte, sondern eine kanonische, nach `node_id` sortierte Liste von `PhaseInputRefV1`:

```text
producer_node_id
producer_adapter_id
producer_adapter_version
producer_status
producer_output_digest
producer_evidence_digest
```

Ein erfolgreich gestarteter Adapter liefert genau ein `PhaseOutputRefV1`:

```text
output_version: "phase-output-v1"
status: COMPLETE | INCOMPLETE | REJECTED
output_bytes: 0..65536 rohe Bytes
output_digest: SHA-256(output_bytes)
evidence_bytes: 0..65536 rohe Bytes
evidence_digest: SHA-256(evidence_bytes)
claim_scope: adapter-definiert, 1..64 ASCII-Zeichen [A-Za-z0-9_.-]
```

`COMPLETE` bedeutet nur, dass der jeweilige Adaptervertrag vollständig endete. `INCOMPLETE` und `REJECTED` bleiben fachliche Adapterausgaben; sie sind nicht mit einem DAG-Fehler gleichzusetzen. Die spätere konkrete Abbildung eines NanoCell-Laufs muss dessen Status, `AnswerPoint` und `BRUCE_NANOCELL_RUN_V1` als Evidenz bewahren, darf aber aus `MATCHED` keinen allgemeinen Claim ableiten.

### PhaseReceiptV1 und PhaseDagRunV1

Für jeden Node entsteht genau ein Receipt:

```text
node_id
adapter_id, adapter_version
request_digest
input_refs
outcome: READY | BLOCKED | FAILED | CANCELLED
output_ref: nur bei READY
failure_code: nur bei FAILED | CANCELLED
```

`READY` bedeutet: Adapter wurde gestartet und hat einen `PhaseOutputRefV1` geliefert, auch wenn dessen eigener Status `INCOMPLETE` oder `REJECTED` lautet. `BLOCKED` bedeutet: mindestens ein Vorgänger hatte nicht `READY`; der Adapter wurde nicht aufgerufen. `FAILED` bedeutet: ein Vertrags-, Adapter- oder Evidenzfehler ohne Output-Ref. `CANCELLED` bedeutet: der globale DAG-Control stoppte vor dem Start dieses Knotens; ebenfalls ohne Output-Ref.

`PhaseDagRunV1` enthält Spezifikation, Receipts, kanonischen DAG-Trace und ein eigenes Bundle `BRUCE_PHASE_DAG_RUN_V1`. Ein DAG-Trace hat nur diese Ereignisse:

```text
VALIDATED(node_id optional)
STARTED(node_id)
READY(node_id)
BLOCKED(node_id)
FAILED(node_id)
CANCELLED(node_id)
COMPLETED
```

`COMPLETED` heißt: Der Scheduler hat seine deterministische Abarbeitung beendet. Es sagt nichts über die Anzahl `READY`, fachliche Gültigkeit oder Wahrheit der Outputs aus.

## Deterministische Ausführung

1. Dekodiere und validiere die kanonische Spezifikation vollständig, inklusive Requestdigests, Kantenlimits und Zyklusfreiheit. Bei Fehler: kein Receipt, kein Bundle.
2. Beginne mit allen abhängigkeitfreien Knoten, gewählt in kanonischer Node-ID-Reihenfolge.
3. Vor jedem Start prüfe `PhaseDagControlV1::cancelled`. Bei wahr: markiere den aktuellen startbereiten Knoten `CANCELLED`; alle späteren noch nicht entschiedenen Knoten werden ebenfalls `CANCELLED`. Bereits gestartete Adapter werden nicht unterbrochen.
4. Prüfe vor jedem Adapterstart `max_started_nodes`, `max_trace_entries` und die noch mögliche Receiptgröße. Bei Budgetmangel markiere den aktuellen startbereiten Knoten `CANCELLED` mit `failure_code=DagBudgetExhausted`; nachfolgende unentschiedene Knoten werden `CANCELLED`. Ein Budgetstopp ist kein Adapter-Output und darf nicht als `INCOMPLETE` umetikettiert werden.
5. Ohne zuvor ausgelösten globalen Control- oder Budgetstopp gilt: Hat ein Knoten einen Vorgänger mit `BLOCKED`, `FAILED` oder `CANCELLED`, markiere ihn `BLOCKED`, ohne Adapteraufruf. Ein Vorgänger mit `READY` ist zulässig, unabhängig vom eingebetteten Output-Status. Ein globaler Control- oder Budgetstopp aus Schritt 3 oder 4 hat Vorrang und markiert alle noch unentschiedenen Knoten `CANCELLED`.
6. Starte nur einen Adapter gleichzeitig. V1 ist streng seriell; Parallelität würde eine zusätzliche Start-, Fehler-, Ressourcen- und Traceordnung benötigen.
7. Ein Adapter erhält nur seinen unveränderlichen Node-Request und kanonische Input-Refs. Er darf weder DAG-Spezifikation noch Vorgängerreceipts verändern.
8. Nach `READY`, `FAILED`, `BLOCKED` oder `CANCELLED` wird der nächste entscheidungsfähige Knoten wieder in kanonischer Node-ID-Reihenfolge gewählt.
9. Erzeuge `COMPLETED` erst nach einer Entscheidung für jeden Knoten. Ein gültiger DAG-Run enthält damit exakt ein Receipt pro Node.

Die Startreihenfolge ist daher vollständig durch Spezifikation, Budgets, Control und Adapterresultate festgelegt. Ein Adapterfehler stoppt nicht implizit die gesamte DAG: Nur abhängige Nachfolger werden `BLOCKED`; unabhängige, später kanonisch startbereite Knoten dürfen weiterlaufen, solange kein globales Budget oder Control stoppt.

## Replay- und Evidenzvertrag

`BRUCE_PHASE_DAG_RUN_V1` speichert kanonisch length-prefix-kodiert:

```text
canonical dag spec bytes + spec SHA-256
canonical receipt list bytes + receipt-list SHA-256
canonical DAG trace bytes
outer SHA-256
```

Das äußere Framing folgt dem vorhandenen Muster `Header\n` + 64 Kleinbuchstaben-Hexzeichen + `\n` + Body. Alle variablen Daten bleiben rohe length-prefix-kodierte Bytes. Replay muss:

1. Header, Grenzen, äußeren Digest und kanonische Längen prüfen,
2. die Spezifikation erneut validieren,
3. alle Receipts und den DAG-Trace strikt dekodieren,
4. für jeden `READY`-Receipt Requestdigest, Input-Refs, Output-/Evidenzdigest und den geeigneten Adapter-Replay prüfen,
5. für jeden nicht-`READY`-Receipt die Abhängigkeits- und Control-/Budgetordnung gegen die deterministische Schedulerregel prüfen,
6. den vollständigen Lauf erneut konstruieren und byteidentisch rekodieren.

Ein gespeicherter Receipt mit gültiger Prüfsumme, aber falscher Adapterversion, fehlender Vorgängerprovenienz, veränderter Evidenz, falscher Startreihenfolge oder erfundenem `READY` wird abgelehnt. Das Bundle schreibt keine Dateien und speichert keine Experience.

## Adaptergrenze und NanoCell-Folgearbeit

`PhaseAdapterV1` ist eine künftig interne, statisch registrierte Schnittstelle. Dynamische Plugins sind nicht V1. Ein Adapter definiert mindestens:

```text
adapter_id, adapter_version
validate(node request, input refs)
execute(node request, input refs, adapter control) -> PhaseOutputRefV1
replay(node request, input refs, output/evidence bytes) -> PhaseOutputRefV1
```

Ein späterer `nanocell-phase-adapter-v1` ist erst zulässig, wenn ein separater NanoCell-Adaptervertrag festlegt:

- wie ein `NanoCellRequestV1` oder `BRUCE_NANOCELL_RUN_V1` als Node-Request repräsentiert wird,
- welche NanoCell-Outputs an Nachfolger weitergereicht werden dürfen,
- wie `MATCHED`, `NO_MATCH`, `CONTEXT_REJECTED` und `INCOMPLETE` exakt in `PhaseOutputRefV1` abgebildet werden,
- dass weder NanoCell-Match noch DAG-`READY` Application-Gates, Learn- oder Publishentscheidungen verändert.

Bis dahin darf PhaseDagV1 nur feste, reine Referenzadapter für Tests verwenden. Sie sind kein Ersatz für eine NanoCell- oder Application-Integration.

## Referenztests und Abnahme

Die spätere Implementierung muss mindestens diese deterministischen Fixtures enthalten:

| Fixture | Erwartung |
| --- | --- |
| Zwei unabhängige Quellen, dann Join | Quellen starten in ID-Reihenfolge; Join erhält beide Input-Refs kanonisch sortiert. |
| Fehlender/fehlerhafter Vorgänger und unabhängiger Knoten | Abhängiger Knoten wird `BLOCKED`; unabhängiger Knoten läuft weiter. |
| Adapter liefert `INCOMPLETE` | Receipt ist `READY` mit eingebettetem `INCOMPLETE`; zulässiger Nachfolger erhält diesen Status unverändert. |
| Zyklus `a → b → a` | Validierungsfehler vor jedem Adapteraufruf. |
| Budget vor Start und mitten im Run | Kein erfundener Adapteroutput; noch nicht entschiedene Knoten werden `CANCELLED`; alle abhängigen Folgen sind nachvollziehbar. |
| Globaler Controlabbruch | Check nur vor einem Node-Start, keine Unterbrechung eines laufenden Adapters. |
| Manipuliertes Bundle | Äußerer Digest, Specdigest, Receiptdigest, Input-Ref, Output, Evidenz, Node-Reihenfolge, Status, Trace und Restbytes werden abgelehnt, auch bei erneuerter äußerer Prüfsumme. |
| Neustart/Replay | Derselbe Spec-, Adapter- und Budgetzustand erzeugt byteidentisches Bundle. |

Messung wird getrennt für Spec, einzelne Requests, Input-Refs, Output-Refs, Receipts, DAG-Trace und Gesamtbundle berichtet. Diese Werte sind keine Gesamt-RAM-, Performance- oder Skalierungsbehauptung.

## Offene Entscheidungen nach diesem Vertrag

1. Der konkrete NanoCell-Adapter und seine spätere Aufnahme in `ApplicationTask`, Renderer, Gates und CLI.
2. Welche statischen Referenzadapter V1 implementiert und wie ihre Test-Requests versioniert werden.
3. Ob ein späterer DAG-Adapter unabhängige Application-Bundles konsumiert oder eine eigene gemeinsame Evidenzhülle erhält.
4. Ein eigener Vertrag für semantische Zustandsgraphen, Zyklen, SCC-Verdichtung und Provenienzretention.
5. Affine Regeln, Relationsextraktion, Formula DAG, signierte/gaußsche Phasen und alle kryptografischen Benchmarks.
6. Globale Zeit-, Speicher- und Threadgrenzen; V1 besitzt zunächst nur seine expliziten Node-, Trace- und Receiptgrenzen.
7. Parallelisierung und Determinismus bei mehreren gleichzeitig startbaren Knoten.

## Rückfallstrategie

Kann der Referenzkern nicht vollständig validiert, getestet oder replayt werden, wird kein Adapter in `ApplicationTask`, CLI oder Sprache aktiviert. Die bestehende lineare `ExecutionSession`, die NanoCell-Einzelbundles und alle vorhandenen Forschungsbundles bleiben unverändert und unabhängig nutzbar.

## Freigabegate

Erst nach fachlicher Prüfung dieser Spezifikation wird ein eigener Implementierungsplan erstellt. Er beginnt mit DAG-Validierungs- und Replaytests vor Produktionscode, implementiert zunächst ausschließlich feste Referenzadapter und endet mit einem frischen vollständigen Core-Testlauf. Ein NanoCell- oder Application-Adapter ist nicht automatisch Teil dieses Plans.
