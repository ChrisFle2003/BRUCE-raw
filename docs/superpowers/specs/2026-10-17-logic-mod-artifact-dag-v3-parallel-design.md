# Logic-Mod Artifact DAG V3 – Begrenzte Parallelität

**Status:** IMPLEMENTIERT UND BELEGT — durch die unten genannten Core- und
Real-Binary-Verträge; dieses Dokument bleibt ein Design-/Abnahmeprotokoll.

## Ziel

DAG V3 erweitert die bestehende serielle Artifact DAG V2 um begrenzte
physische Parallelität für voneinander unabhängige Knoten. Fachliche Ordnung,
Spec-Digest, DAG-ID, Record und Replay bleiben unabhängig von der gewählten
Parallelgrenze deterministisch, sofern die Worker identische deterministische
Transkripte liefern. V3 verwendet die V2-Spec (`schema_version:2`), Payloads,
Digestpräfixe und das kanonische V2-Recordformat unverändert.

## Öffentliche Schnittstelle

```text
bruce logic-mod-artifact-dag-v3 run SPEC_JSON --store LOCAL_STORE --record DAG_RECORD \
  [--max-parallel-nodes 1..4]
bruce logic-mod-artifact-dag-v3 replay SPEC_JSON --store LOCAL_STORE --record DAG_RECORD
```

Der Standard für `run` ist `--max-parallel-nodes 1`. Der Wert ist eine lokale
Laufzeitsteuerung und darf weder in die JSON-Spec noch in Digest, DAG-ID oder
Record eingehen. `replay` nimmt keine Paralleloption an und startet keinen
Worker.

## Wellenmodell

Der Host bestimmt bereite Knoten in der bestehenden stabilen topologischen
V2-Reihenfolge (jeweils kleinste bereite ID). Er startet höchstens `N`
voneinander unabhängige Knoten als eine
physische Welle und wartet die gesamte Welle ab. Ergebnisse werden erst danach
übernommen und in kanonischer logischer V2-Reihenfolge gehalten; diese muss
nicht der physischen Start-/Abschlussfolge entsprechen. Ein erst dadurch bereiter
Nachfolger startet ausschließlich in einer folgenden Welle.

Jeder Worker behält eigene Autorisierung, Budgets, unterschiedliche
Store-Manifeste und versiegelte FD-6-Payloads. Der lokale Store ist gemeinsam,
Worker erhalten keinen Storezugriff und keine gemeinsame beschreibbare
Workerressource. Es gibt keine gemeinsame Workerinstanz und keine neuen
Worker-, FD- oder Seccomp-Berechtigungen.

## Fehlervertrag

Schlägt ein Knoten in einer Welle fehl, werden alle bereits gestarteten Knoten
der Welle vollständig abgewartet und gereapt. Danach startet keine neue Welle.
Erfolgreiche Knoten dieser Welle dürfen ihre Result Artifacts behalten;
nicht gestartete Knoten und Nachfolger bleiben aus. Ein DAG-Record entsteht
weiterhin ausschließlich bei vollständig erfolgreichem Lauf.
Auch bei einer Laufzeitausnahme wartet der Host alle bereits gestarteten
Worker ab, bevor er den Fehler zurückgibt.

## Nachweise

Die [Coreverträge](../../../tests/unit/logic_mod_artifact_dag_v3_tests.cpp)
und [Real-Binary-Workflows](../../../tests/integration/test_logic_mod_artifact_dag_v3.py)
belegen bytegleiche Records deterministischer Worker für Grenzen 1 bis 4,
echte Workerüberlappung, Wellenbarrieren für Nachfolger, Fehlerstopp nach einer
Welle, Reaping aller gestarteten Worker, frühe Ablehnung ungültiger Grenzwerte
und unverändertes workerfreies Replay. Der Core prüft zusätzlich das
vollständige Abwarten bei einer Publikationsausnahme während einer Welle.
Die CLI prüft ungültige Grenzen gegen eine beobachtete FIFO-Spec vor deren
Öffnen. Die bestehenden V2-Verträge bleiben eigenständige Regressionstests.
Der unveränderte CLI-Beispielverifier führt V3 nicht aus und ist kein
V3-Laufbeleg. Aktuelle Befehle und Formate stehen im
[CLI-Handbuch](../../cli/commands.md#logic-mod-artifact-dag-v3--begrenzte-parallele-artifact-wellen)
und im [Formatvertrag](../../cli/formats.md#logic-mod-artifact-dag-v3-kompatible-formate).

## Grenzen

V3 führt keine Retry-Logik, faire globale Ressourcenplanung, Prioritäten,
dynamische Knoten, externe Stores, Netzwerk, GPU-Ausführung oder allgemeine
Runtime-Orchestrierung ein.
