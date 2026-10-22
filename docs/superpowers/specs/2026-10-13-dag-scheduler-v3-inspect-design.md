# DAG-Scheduler V3 – read-only Inspector

**Status: VORSCHLAG — freigegebener Entwurf, vor Implementierungsplanung zu prüfen.**

## Ziel

V3 erhält neben `execute` einen vollständig read-only Inspector:

```text
bruce dag-scheduler-v3 inspect SPEC
```

Er liest dieselbe begrenzte V3-Spec-Datei, prüft sie vollständig und zeigt die
statische, deterministische Kahn-Reihenfolge. Er startet keine Callbacks,
ruft `execute_dag_scheduler_v2` nicht auf und veröffentlicht keine Payloads,
Receipts oder Laufresultate.

## Öffentliche CLI

Erlaubt sind genau zwei Routen:

```text
bruce dag-scheduler-v3 execute SPEC
bruce dag-scheduler-v3 inspect SPEC
```

Fehlende, unbekannte, zusätzliche oder optionenartige Argumente ergeben Exit
2, leeres stdout und exakt:

```json
{"error":"Usage: bruce dag-scheduler-v3 execute|inspect SPEC"}
```

`execute` behält seine bestehende erfolgreiche Ausgabe unverändert. Der neue
Inspector nutzt einen eigenen Vertrag und verändert weder V2 noch die
Callback-Ausführung.

## Verarbeitung

```text
SPEC
  -> parse_spec()
  -> V3-Format- und Callback-Prüfung
  -> V2-Strukturvalidierung
  -> read-only Kahn-Planung
  -> JSON-Metadatenbericht
```

`parse_spec()` bleibt die alleinige Quelle für Bytegrenze, ASCII/LF-Regeln,
Header, Feldreihenfolge, Hex-Dekodierung, kanonische IDs/Abhängigkeiten,
Callback-Arity und alle V2-Strukturanforderungen. Der Inspector erhält nur
die bereits validierte `DagSchedulerSpecV2`.

Die Planung ist die lexikografische Kahn-Auswahl, die V2 bei ausschließlich
`READY`-Ergebnissen starten würde. Sie nutzt nur Knoten-IDs und
Abhängigkeiten und ruft keinen Callback auf. Der Wert ist eine statische
Ablaufvorschau, keine Vorhersage eines realen Laufs: Callbackfehler,
Payloadgrößen, Receipt-Budgets und Startbudget-Cancellation werden nicht
simuliert.

## Erfolgsbericht

Ein erfolgreicher Inspector schreibt einen stabilen JSON-Bericht mit einem
abschließenden LF:

```text
contract: "dag-scheduler-v3-inspect-v1"
action: "inspect"
spec_version: "dag-scheduler-spec-v3"
spec_digest: SHA-256 der unveränderten Dateibytes
scheduler_version: "dag-scheduler-v2"
dag_id
max_started_nodes
max_total_receipt_bytes
nodes
planned_start_order
```

`nodes` folgt der kanonischen Knotenreihenfolge der Spec und enthält nur:

```text
node_id
callback_id
dependencies
request_size
request_digest
```

`planned_start_order` enthält nur Knoten-IDs. Der Bericht enthält weder
Request-Bytes noch Payload-Bytes noch `request-hex`. IDs, Größen und Digests
bleiben sichtbare Metadaten.

## Fehlergrenzen

Alle Format-, Struktur-, Zyklus- und Eingabegrößenfehler werden vor einer
Inspector-Ausgabe abgewiesen. Es entsteht kein Teilbericht. Diagnostik darf
keine Request-Bytes oder Request-Hexwerte spiegeln.

## Tests und Dokumentation

Die Implementierung beweist mindestens:

1. Die kanonische Vierknoten-Spec liefert die Reihenfolge `a-source`,
   `b-transform`, `c-independent`, `d-join`.
2. `inspect` und `execute` verwenden dieselben Spec-Metadaten und denselben
   `spec_digest`; die bestehende Execute-Ausgabe bleibt bytegleich.
3. NUL- und leere Requests funktionieren über Hex, erscheinen jedoch nicht
   im Inspector-Bericht.
4. Header-, Separator-, Hex-, Arity-, Zyklus- und 65.537-Byte-Fehler liefern
   keinen Inspector-Bericht.
5. Wiederholte Inspector-Aufrufe sind byteidentisch; Standard- und
   NanoCell/Cockpit-OFF-Builds bleiben konsistent.
6. CLI-Handbuch, Befehlsreferenz, Formatreferenz, Architektur-Blueprint und
   Dokumentationstest beschreiben den Inspector als read-only und begrenzen
   ihn ausdrücklich gegen Ausführung, Bundles, Persistenz, Replay,
   Parallelität, GPU/Netzwerk und Cockpit.

Die geschützten generierten CLI-Artefakte werden nur durch den vorhandenen
Verifier in einer vollständigen nativen temporären Kopie aktualisiert. Falls
der Verifier den Inspector nicht ausführt, bleiben die Prozess- und
C++-Tests der Laufzeitbeleg.
