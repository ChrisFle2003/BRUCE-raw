# Logic-Mod Artifact DAG V4 – Host-Budgets

**Status:** IMPLEMENTIERT UND BELEGT — Core und separate Run-/Replay-CLI.
Belege: [V4-Coreverträge](../../../tests/unit/logic_mod_artifact_dag_v4_tests.cpp)
und [reale CLI-Workflows](../../../tests/integration/test_logic_mod_artifact_dag_v4.py).
Der [CLI-Vertrag](../../cli/commands.md#logic-mod-artifact-dag-v4--lokale-hostbudgets-für-artifact-wellen)
und die [Formate](../../cli/formats.md#logic-mod-artifact-dag-v4-lokale-budgets-ohne-formatwechsel)
beschreiben den implementierten Umfang. Die folgenden Abschnitte bewahren
die Designentscheidung und präzisieren deren belegte Grenzen.

V4 ergänzt DAG V3 um lokale Hostgrenzen für Wellen. `run` akzeptiert optional
`--max-parallel-nodes 1..4`, `--max-total-wall-ms 1..60000`,
`--max-total-payload-bytes 0..8388608`, `--max-started-workers 1..16` und
`--cancel-after-ms 1..60000`; `replay` akzeptiert keine davon und startet
keinen Worker. Defaults bleiben V3-seriell, höchstens 16 Starts und ohne neue
globale Byte-/Zeitgrenze.

Vor jeder Welle prüft der Host monotone Laufzeit, gestartete Worker und Größe
der zu erzeugenden internen Input-Artifacts. Würde eine Welle ein Budget
überschreiten oder ist der Cancel-Timer erreicht, startet sie nicht. Bereits
gestartete Worker einer laufenden Welle werden vollständig abgewartet und
gereapt. Der Lauf endet kontrolliert als `DAG_GLOBAL_BUDGET_EXHAUSTED` oder
`DAG_HOST_CANCELLED`; frühere Artefakte dürfen bestehen bleiben, ein
Erfolgsrecord entsteht nicht.

Budgets sind lokale Steuerung und verändern weder JSON-Spec, DAG-ID, Record
noch Replay. V4 fügt keine Retries, keine erzwungenen Mid-Worker-Kills, keine
GPU-/Netzwerk-Ausführung und keine allgemeine Orchestrierung hinzu.

## Präzisierungen der implementierten Grenze

- Jede lokale Option darf einmal, als dezimales Flag/Wert-Paar ohne
  führende Nullen, Vorzeichen oder Leerraum, nach den festen Run-Argumenten
  stehen. Ihre Reihenfolge ist frei. Ungültige Optionen und jede Run-Option
  bei Replay scheitern vor Dateizugriff mit `InvalidLogicModDagArguments`.
- Zeit wird monoton ab Eintritt in den Core-Lauf einschließlich Preflight
  gemessen. Prüfung erfolgt ausschließlich vor Wellen; Cancellation hat dann
  Vorrang vor globalen Budgets. Eine erfolgreiche letzte Welle bleibt auch
  nach Timerablauf erfolgreich. Die Grenzen sind keine harten Laufzeitfristen.
- Das Bytebudget summiert vollständige interne Input-Dokumente je
  Empfängerknoten über alle Wellen, einschließlich Hülle und direkter
  Vorgängerergebnisse. Es misst weder Prozessspeicher noch alle Storebytes.
  Wurzeln benötigen null interne Payloadbytes; Grenze 0 ist dafür zulässig.
  Byte- und Startgrenzen erlauben exakte Gleichheit.
- Eine abgewiesene Welle wird nicht verkleinert und veröffentlicht keines
  ihrer Input Artifacts. Vorherige Wellen bleiben erhalten. Kontrollierter
  Stopp ergibt Exit 2, `accepted:false`, `dag_id:null`; `node_order` enthält
  die gesamte logische Ordnung, `nodes` die abgeschlossenen Wellen.
- Knotenversagen bleibt nach vollständig abgewarteter Welle
  `DAG_NODE_FAILED`. Bereits gestartete Worker werden auch bei Ausnahmen
  abgewartet und gereapt. Einzelbudgets und Worker-/FD-/Seccomp-Rechte
  bleiben unverändert; es gibt keinen Rollback.
- Erfolgsrecords behalten den V2-Marker `BRUCE_LOGIC_MOD_ARTIFACT_DAG_V2`
  und sind bei identischen deterministischen Workertranskripten mit V3
  bytegleich. V4-JSON verwendet `contract:"logic-mod-artifact-dag-v4"`.
  Lokale Grenzen, Messungen, Pfade und Payloadbytes sind keine JSON-Ausgabe.
  Der unveränderte Beispielverifier führt diese V4-Workflows nicht aus.
