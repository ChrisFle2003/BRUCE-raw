# Phase-DAG V3: portable Payload-Kanten

**Status:** ENTWURF, ABSCHNITTSWEISE VOM NUTZER FREIGEGEBEN\
**Datum:** 2026-10-13\
**Geltungsbereich:** Ein separater, begrenzter und replaybarer Phase-DAG-V3-
Vertrag mit gerichteten Byte-Payloads.

## Ziel

Ein V3-Knoten soll den Ausgabepayload eines direkten Vorgängers als benannten
Eingabepayload erhalten können. Der gesamte Lauf muss ohne ursprüngliche
Spec- oder Kontextdateien portabel und deterministisch replaybar bleiben.
V2 transportiert nur Status- und Digest-Referenzen und bleibt unverändert.

Der erste produktive V3-Pfad besteht aus einem NanoCell-Erzeuger und einem
`payload-gate-v1`-Verbraucher. NanoCell veröffentlicht ein kleines,
versioniertes Ergebnisobjekt. Das Gate liest die tatsächlichen Eingabebytes,
prüft ihren Digest und kann selbst wieder einen Manifest-Payload erzeugen.

## Nicht-Ziele

- Änderung des V2-Formats, der V2-CLI oder der V2-Adapter;
- externe Dateiverweise, Blob-Speicher oder Netzwerkauflösung für Payloads;
- freie Adapter- oder Versionsauswahl;
- parallele oder verteilte Ausführung;
- Cockpit-Inspektion oder Cockpit-Ausführung von V3;
- freie Sprache, GPU-Ausführung oder ein allgemeiner Beweiser.

## Datenvertrag

V3 führt eigenständige Typen und Funktionen ein; V2-Typen werden weder
erweitert noch umgedeutet.

```text
PhaseOutputV3
├── output_bytes / output_digest
├── evidence_bytes / evidence_digest
└── payload_bytes / payload_digest

PhaseInputV3
├── producer_node_id
├── producer_adapter_id / producer_adapter_version
├── producer_status
├── producer_output_digest / producer_evidence_digest
└── producer_payload_bytes / producer_payload_digest
```

Der Scheduler erzeugt einen `PhaseInputV3` pro direkter Abhängigkeit, in der
kanonischen Reihenfolge der lexikographisch sortierten `depends`-Liste. Ein
Adapter erhält keine Payload eines indirekten Vorgängers.

Jeder nichtleere Payload ist auf 65536 Bytes begrenzt und zählt zusammen mit
Output und Evidence zum bestehenden V3-Receipt-Budget. Ein Adapteroutput ist
ungültig, wenn Payloaddigest und Payloadbytes nicht übereinstimmen. V3 prüft
dies beim Ausführen und beim Replay. Leere Payloads sind allgemein zulässig,
aber `payload-gate-v1` akzeptiert sie nicht als erfolgreiche Eingangsevidenz.

## Adapter

### `nanocell-phase-v3` / `v1`

Der Adapter verwendet die bereits begrenzte NanoCell-Anfrageform und dieselben
drei Referenztechniken wie V2. Zusätzlich gibt er einen kanonischen
`nanocell-phase-payload-v1` aus:

```text
payload-version
nanocell-status
final-state-present
final-state (nur wenn vorhanden)
```

Der Payload enthält weder Kontextbytes noch einen gesamten NanoCell-Lauf. Für
`CONTEXT_REJECTED` ist `final-state-present` falsch. Der V3-Phaseoutput bleibt
fachlich `COMPLETE`, `INCOMPLETE` oder `REJECTED` nach der bereits bestehenden
NanoCell-Statusabbildung.

### `payload-gate-v1` / `v1`

Der Gate-Request ist parameterlos und kanonisch. Ein Gate wird nur aufgerufen,
wenn der DAG-Kern für jeden direkten Vorgänger ein READY-Receipt besitzt.
Es akzeptiert genau dann mit `COMPLETE`, wenn mindestens ein Input vorliegt,
jeder Vorgängerstatus `COMPLETE` lautet, jeder Payload nichtleer ist und jeder
Payloaddigest zu den übergebenen Bytes passt. Andernfalls erzeugt es einen
gültigen `REJECTED`-Output. Ungültig geformte Input-Referenzen sind ein
Adapterfehler und führen zum FAILED-Receipt.

Ein erfolgreiches oder abgelehntes Gate erzeugt einen kanonischen
`payload-gate-payload-v1`-Manifest-Payload mit Entscheidung, Anzahl und den
geordneten Payloaddigests seiner Inputs. Damit kann ein weiteres Gate die
entstandene Aussage wieder als Payload prüfen. Evidence enthält die
kanonischen, nach Erzeuger-ID geordneten Input-Referenzen und ihre
Payloaddigests, aber keine Payload-Rohbytes.

## Laufzeit, Paket und Replay

`PhaseDagSpecV3`, `execute_phase_dag_v3`, `pack_phase_dag_run_v3` und
`replay_phase_dag_run_v3` sind neue, getrennte öffentliche V3-Schnittstellen.
Das Bundle besitzt einen eigenen Header `BRUCE_PHASE_DAG_RUN_V3` und eine
Payload-SHA-256. Die gespeicherten Receipts enthalten die Payloadbytes jedes
Outputs und die Payloadbytes aller Eingabereferenzen, damit Replay ohne externe
Speicherquelle ausgeführt werden kann.

Replay dekodiert nur kanonische V3-Bytes, validiert alle Digests, führt die
Spec mit dem festen V3-Katalog erneut aus und verlangt bytegleiche
Serialisierung. Ein V2-Bundle ist kein V3-Bundle und umgekehrt.

## CLI-Spec und Befehle

Bei aktivem NanoCell-Modul stellt die CLI zusätzlich bereit:

```text
bruce phase-dag-v3 execute SPEC --output BUNDLE
bruce phase-dag-v3 inspect BUNDLE
bruce phase-dag-v3 replay BUNDLE
```

Die V3-Text-Spec erhält einen eigenen Header `BRUCE_PHASE_DAG_SPEC_V3` und
behält globale DAG-Budgets bei. Ein NanoCell-V3-Knoten hat die bekannten
Core-, Kontext- und Technikfelder und bindet fest
`nanocell-phase-v3`/`v1`. Ein Gate-Knoten hat genau:

```text
node=gate
depends=a,b
adapter=payload-gate-v1
```

Die CLI akzeptiert weder weitere Adapter noch zusätzliche Knotenschlüssel.
`execute` veröffentlicht ein neues V3-Bundle ausschließlich und atomar nach
dem bestehenden Veröffentlichungsvertrag. `inspect` und `replay` lesen
höchstens 5 MiB, prüfen Replay und zeigen nur den Metadatenreport, niemals
Payload-Rohbytes. Das bestehende Cockpit bleibt V2-only und lehnt V3-Uploads
außerhalb seines aktuellen Formats ab.

## Fehlerbehandlung

- Ein nicht READY-Vorgänger blockiert seinen Nachfolger vor dem Adapterstart.
- Ein READY-Vorgänger mit `INCOMPLETE`, `REJECTED`, leerem oder nicht zum
  Digest passendem Payload liefert beim Gate kontrolliert `REJECTED`; eine
  strukturell ungültige Input-Referenz liefert FAILED.
- Übersteigt Output, Evidence oder Payload die V3-Grenzen, liefert der
  Scheduler einen FAILED-Receipt und blockiert abhängige Knoten.
- Manipulierte, nichtkanonische oder digest-inkonsistente Bundles werden durch
  `inspect` und `replay` mit Exit 2 abgelehnt.

## Tests und Akzeptanzkriterien

1. Ein NanoCell-V3-Knoten mit `COMPLETE`-Payload und ein nachfolgendes Gate
   ergeben zwei READY-Receipts mit COMPLETE-Gateoutput.
2. `INCOMPLETE`, `REJECTED`, leere und digest-inkonsistente Vorgängerpayloads
   ergeben den dokumentierten Gate-Befund; strukturell ungültige Inputs
   ergeben FAILED.
3. Zwei hintereinandergeschaltete Gates belegen, dass der Gate-Manifestpayload
   erneut konsumierbar ist.
4. Payloads mit 65536 Bytes werden akzeptiert, 65537 Bytes abgelehnt; das
   Receipt-Budget wird ebenfalls geprüft.
5. V3-Bundles replayen bytegleich; eine Einzelbyte-Manipulation an Payload,
   Payloaddigest oder Receipt wird abgelehnt.
6. CLI-Execute, Inspect und Replay verhalten sich portabel nach Löschen von
   Spec und Kontextdateien; ihre JSON-Reports enthalten keine Rohpayloads.
7. Bestehende V2- und Cockpit-Verträge bleiben unverändert und bestehen ihre
   vorhandenen Tests.
