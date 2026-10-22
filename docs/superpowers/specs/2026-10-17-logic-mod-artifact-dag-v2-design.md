# Logic-Mod Artifact DAG V2

**Status:** IMPLEMENTIERT, ABER NICHT AKTUELL VERIFIZIERT — Task-3-Abnahme läuft.

## Ziel

Logic-Mod Artifact DAG V2 führt eine lokale, signierte, begrenzte Menge von
Logic-Mods aus einer JSON-Spec seriell und deterministisch aus. Jeder Knoten
veröffentlicht ausschließlich über den Host ein Result Artifact. Direkte
Vorgänger werden für Nichtwurzelknoten als kanonisches, versiegeltes FD-6-
Payload zusammengeführt. Die Erweiterung ersetzt weder Logic-Mod Artifact
Chain V1 noch Phase-DAG V2/V3.

## Öffentliche Schnittstelle

```text
bruce logic-mod-artifact-dag-v2 run SPEC_JSON --store LOCAL_STORE --record DAG_RECORD
bruce logic-mod-artifact-dag-v2 replay SPEC_JSON --store LOCAL_STORE --record DAG_RECORD
```

`run` akzeptiert genau diese Reihenfolge. `replay` startet keinen Worker und
akzeptiert keine Ausgabeoption. Unbekannte, leere, doppelte, umsortierte oder
zusätzliche Werte werden vor Datei- oder Storezugriff abgewiesen.

## Spec-Grenzen

Die kanonische JSON-Spec beschreibt 1–16 Knoten und ist auf 65536 Bytes begrenzt.
Die endgültigen Pflichtfelder, Typen, Budgetdefaults und Digestregeln stehen im
[CLI-Formatvertrag](../../cli/formats.md#logic-mod-artifact-dag-v2-spec-payload-record-und-json).
Die oberste Ebene enthält genau `schema_version:2` und `nodes`; Knoten enthalten
`id`, `package`, `trust_store`, `input`, `predecessors`, `output_manifest` sowie
bei Nichtwurzeln `input_manifest` und optional das vollständige `budgets`-Objekt.
Unbekannte und doppelte Schlüssel, auch nach Escape-Decodierung, sind unzulässig.
Der kanonische Digest sortiert Schlüssel und Knoten, normalisiert Inputs, setzt
Defaultbudgets ein und bindet weiterhin die Paket-/Truststorepfadstrings.

Jeder Knoten enthält eine eindeutige ID, explizite Paket- und Truststore-Referenzen, Textinput,
ein Ausgabe-Manifest und eine Liste direkter Vorgänger. Nichtwurzelknoten
enthalten zusätzlich ein ausschließlich hostseitig verwendetes Input-Manifest
für ihr zusammengeführtes Vorgänger-Payload. IDs und
Vorgängerlisten sind eindeutig; Vorgängerlisten sind nach ID sortiert.

Vor dem ersten Workerstart validiert der Host die gesamte Spec: JSON-Form,
IDs, Kanten, Selbstkanten, Zyklen, deterministische topologische Reihenfolge,
Paket-/Trust-/ELF-Vorabprüfung, Capability-Mengen, Textnormalisierung,
Budgets und paarweise verschiedene Ausgabe- und Input-Manifeste. Wurzelknoten benötigen
genau `{artifact.output.v1, worker.execute}`. Nichtwurzelknoten benötigen
genau `{artifact.input.v1, artifact.output.v1, worker.execute}`.

## Datenfluss und Ausführung

Knoten laufen seriell in stabiler topologischer Reihenfolge, bei Gleichstand
nach Knoten-ID. Ein Wurzelknoten erhält kein FD-6-Payload. Vor einem
Nichtwurzelknoten löst der Host jedes Result Artifact seiner direkten
Vorgänger frisch aus dem lokalen Store auf und prüft dessen READY-Identität.
Er baut daraus dieses kanonische UTF-8-Dokument und stellt nur dessen
versiegelten Snapshot auf FD 6 bereit:

```text
BRUCE_LOGIC_MOD_DAG_INPUT_V2\n
N:2 N:Vorgängeranzahl
je direktem Vorgänger: N:node_id N:manifest_id N:sha256 N:size_bytes N:codec N:result_bytes
\n
```

`N:BYTES` bezeichnet jeweils ein dezimales Byte-Längenpräfix; zwischen den
Feldern stehen tatsächlich keine Leerzeichen oder zusätzlichen Zeilenumbrüche.
Die Vorgänger sind nach ID sortiert, die Gesamtgrenze beträgt 1048576 Bytes.

Der Host publiziert dieses abgeleitete Payload als ein privates internes
READY-Artifact in das explizite Input-Manifest des Knotens und löst genau
dessen Objekt 0 frisch über den bestehenden Payload-Capability-V1-Vertrag auf.
Das interne Artifact enthält keine Hostpfade. Worker erhalten keinen Storezugriff, keine
Paketpfade und keine neue FD-/Seccomp-Berechtigung. Jedes erfolgreiche
Knotenergebnis wird nach dessen eigenem Manifest als bestehendes Result
Artifact V1 publiziert.

Scheitert ein Knoten, endet die gesamte serielle Ausführung sofort; auch
weitere unabhängige Knoten starten nicht. Frühere Result Artifacts und interne
Input-Artifacts können bestehen bleiben. Es gibt weder
Retry, Parallelität, dynamische Knotenerzeugung, externe Stores, Netzwerk,
Streams, freie Codecs noch allgemeine Orchestrierung.

## Record und Replay

Nach vollständigem Erfolg schreibt der Host genau einen auf 8388608 Bytes
begrenzten privaten DAG-Record, atomar ohne Ersetzen und mit Modus 0600. Er
bindet den Digest der kanonischen Spec, die Reihenfolge, pro
Knoten den unveränderten Logic-Mod-Run-Record und dessen publizierte
Artifact-Identität sowie für Nichtwurzelknoten die verwendeten
Vorgängeridentitäten. Der Record enthält keine Hostpfade, Workerdiagnostik
oder separat kopierten Artifact-Payload; er bindet aber die Identität jedes
internen Input-Artifacts. Eingebettete bestehende Run-Records
behalten jedoch ihre normierten Inputs, Ergebnisse und Response-Frames.

Replay startet keinen Worker. Es prüft Spec-Digest, aktuelle
Paketautorisierung, jeden eingebetteten Run-Record und jedes frisch aus dem
lokalen Store gelöste Result Artifact und interne Input-Artifact. Jede
Abweichung endet kontrolliert als
`DAG_REPLAY_MISMATCH`.

Spec- und Preflight-Ablehnungen erfolgen vor jeder Storemutation und erzeugen
weder Store noch Record. Nach erfolgreichem Preflight kann die bestehende
Worker-Vorbereitung bereits den Store anlegen. Ein anschließender Setupfehler,
etwa bei `socketpair` vor dem ersten Workerstart, kann deshalb einen Store
ohne DAG-Record hinterlassen. Ein Fehler bei der abschließenden
Record-Publikation kann alle Result Artifacts hinterlassen, aber keinen
erfolgreichen DAG-Record.

## Nachweise

Die [Unitverträge](../../../tests/unit/logic_mod_artifact_dag_v2_tests.cpp) prüfen
Graphordnung, deterministische direkte Vorgängerbytes, Vorabprüfung eines
späten signierten Pakets mit Nicht-ELF-Workerbytes, Nachfolgerstopp, kanonische
Records und workerfreie Replayabweichungen. Die
[Real-Binary-Workflows](../../../tests/integration/test_logic_mod_artifact_dag_v2.py)
prüfen einen vierknotigen Diamond mit mehreren Vorgängern, exakte öffentliche
JSON/Artifactbytes, kanonischen Spec-Digest, striktes JSON/Argumente,
Dateisicherheit, vollständigen Preflight ohne Start/Storemutation, sofortigen
Fehlerstopp ohne Record sowie Result-/Input-/Recordmanipulation bei null
Workerstarts im Replay. Die abschließende Abnahme protokolliert aktuelle
Testzahlen und Umgebungsgrenzen in
`runs/logic-mod-artifact-dag-v2/reports/final-review.md`. Die DAG-V2-Fixtures
verwenden für die negative ELF-Prüfung literale Nicht-ELF-Bytes; eine eigene
Regression mit strukturell beschädigten nativen ELF-Dateien wird in diesen
DAG-V2-Tests nicht unabhängig wiederholt.
