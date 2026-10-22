# BRUCE Artifact Payload V1.4 — External Local Inputs

**Status:** FREIGEGEBENE SPEZIFIKATION — Umsetzung implementiert und belegt

## Ziel

V1.4 erweitert den expliziten lokalen V1.3-Payloadpfad um kontrollierte
Eingaben aus bereits vorhandenen Artifact-Store-Manifests. Ein DAG-Knoten darf
dadurch einen direkten Payload von außerhalb des aktuellen Laufs konsumieren,
ohne dass Bruce seine Store-, DAG- oder Worker-Autorität abgibt.

V1.3 bleibt unverändert: Wird keine V1.4-Sidecar-Datei angegeben, gilt exakt
der bestehende In-Run-Fluss mit automatisch veröffentlichten Vorgänger-
Manifesten.

## Nichtziele

V1.4 implementiert nicht:

- Remote- oder Netzwerk-Stores,
- URLs, Provider-Auswahl oder Downloads,
- freie Code-Plugins oder dynamische Worker,
- Änderungen am bestehenden Worker-Frame,
- Änderungen an V2-, V3-, V4- oder V5-Verträgen,
- automatische Garbage Collection, Retention oder Reparatur,
- mehrere Outputobjekte pro Worker-Attempt,
- freie Sprache, allgemeines Beweisen, GPU-Ausführung oder große
  übergreifende Runtime-Orchestrierung.

Diese Punkte bleiben **DOKUMENTIERT / KONZEPT**. :codex-annotation{index="1"}

## Öffentliche Auswahl

Der bestehende Befehl erhält eine ausdrücklich versionierte Option:

```text
bruce dag-scheduler-v5 payload SPEC --output BUNDLE --store STORE \
  --external-inputs INPUTS
```

`--external-inputs` darf höchstens einmal vorkommen. Ohne diese Option wird
kein externer Inputpfad aktiviert. Die Option ist nur für die Route
`dag-scheduler-v5 payload` gültig; `execute`, `inspect`, `publish` und alle
anderen CLI-Routen lehnen sie ab.

`INPUTS` ist eine Bruce-eigene, owner-only JSON-Datei. Sie wird vor dem
ersten Workerstart vollständig gelesen, syntaktisch und semantisch geprüft
und danach nicht verändert. Ihre Bytes gehören zur V1.4-Eingabebindung und
werden in den öffentlichen Report nur über einen Digest referenziert.

## Sidecar-Format V1.4

Die kanonische JSON-Struktur ist:

```json
{
  "contract": "bruce-dag-scheduler-v5-external-inputs-v14",
  "bindings": [
    {
      "node_id": "consumer",
      "inputs": [
        {
          "manifest_id": "m-source-1",
          "object_index": 0,
          "sha256": "<64 lowercase hex characters>",
          "size_bytes": 12,
          "source_run_id": "run-source",
          "source_node_id": "producer",
          "source_attempt_id": "a-1"
        }
      ]
    }
  ]
}
```

Die tatsächliche Implementierung akzeptiert nur die dokumentierte Feldreihen-
folge und keine zusätzlichen Felder. Strings sind ASCII-Identifier oder
kanonische lowercase SHA-256-Digests; Zahlen sind dezimale, nicht-negative
Integerwerte ohne führende Nullen. `manifest_id` bleibt opak und darf keine
Pfadsyntax, Slashes, Backslashes oder `..`-Segmente enthalten.

`inputs` wird in der angegebenen Reihenfolge an den Node gebunden. Ein Node
darf höchstens 15 Payloadobjekte aus der Kombination seiner direkten
In-Run-Abhängigkeiten und externen Bindungen referenzieren; der Request belegt
den ersten der unveränderten 16 Worker-Inputplätze. Eine Sidecar-Bindung darf
keine doppelte Objektbindung enthalten.

## Bindungsregeln

Bruce validiert vor dem Lauf:

1. Die Sidecar-Datei ist eine reguläre, owner-only Datei mit genau einem
   Hardlink, ohne Symlink und innerhalb der bestehenden Dateigrenze.
2. Jeder `node_id` existiert genau einmal im V3-DAG-Spec.
3. Jeder Node besitzt höchstens eine Sidecar-Bindung.
4. Ein externer Input darf nur an einen Node gebunden werden, dessen
   Callback-Arity die zusätzliche Eingabe zulässt.
5. Das Manifest wird aus dem angegebenen lokalen Store gelesen und muss
   kanonisch sowie `status=READY` sein. Manifest und Objekt müssen in genau
   diesem explizit ausgewählten Store gefunden und verifiziert werden; ein
   anderer Store oder eine alternative Quelle darf nicht herangezogen werden.
6. `source_run_id`, `source_node_id` und `source_attempt_id` müssen exakt den
   Manifestfeldern entsprechen.
7. `object_index`, Digest und Größe müssen exakt mit dem Manifestobjekt
   übereinstimmen. Das Objekt wird vor Materialisierung vollständig verifiziert.
8. Der kumulierte Request- und Payloadumfang bleibt innerhalb
   `max_input_bytes`; `max_wall_ms` gilt je Attempt end-to-end.

Die Sidecar-Provenienz autorisiert keinen indirekten DAG-Edge-Ausdruck. Ein
externer Input ist eine explizite Quelle des Zielknotens; Bruce darf keine
zusätzlichen indirekten Vorgänger daraus ableiten und darf keine Sidecar-
Bindung automatisch in den `depends=`-Block des Specs umschreiben.

## Laufzeitablauf

```text
CLI und V3-Spec prüfen
        ↓
Sidecar-Datei kanonisch prüfen
        ↓
lokalen Artifact Store öffnen
        ↓
externe READY-Manifeste und Objekte verifizieren
        ↓
Payloads bounded in private Attempt-Dateien materialisieren
        ↓
bestehende WorkerRequest mit relativen Referenzen erzeugen
        ↓
Worker unter bestehenden Budgets ausführen
        ↓
Output prüfen, als Objekt + READY-Manifest publizieren
        ↓
direkte In-Run-Nachfolger freigeben
```

Die externen Eingaben werden vor dem Start des jeweiligen Attempts
materialisiert. Bei einem späteren Fehler werden alle für diesen Attempt
angelegten privaten Eingabedateien entfernt. Externe Eingaben werden nicht
in den Store kopiert oder neu veröffentlicht; nur Worker-Ausgaben des
aktuellen Laufs erhalten neue READY-Manifeste.

## Fehlervertrag

Öffentliche Fehler bleiben metadata-only und verwenden stabile V1.4-Codes:

| Fehler | Bedeutung | Exit |
|---|---|---:|
| `EXTERNAL_INPUTS_INVALID` | Sidecar-Syntax, Feldreihenfolge oder Vertrag ungültig | 2 |
| `EXTERNAL_INPUT_BINDING_INVALID` | unbekannter/duplizierter Node oder unzulässige Arity | 2 |
| `EXTERNAL_MANIFEST_INVALID` | Manifest fehlt, ist nicht READY oder Provenienz passt nicht | 1 |
| `EXTERNAL_OBJECT_INVALID` | Index, Digest, Größe oder Store-Verifikation passen nicht | 1 |
| `EXTERNAL_INPUT_LIMIT_EXCEEDED` | Objektanzahl, Inputbytes oder Dateigrenze überschritten | 1 |
| `EXTERNAL_INPUT_TIMEOUT` | Attempt-Deadline vor Workerstart überschritten | 1 |
| `EXTERNAL_INPUT_PERMISSION` | unsichere Objekt- oder Attempt-Berechtigungen während des Laufs | 1 |

Ein Fehler in einer externen Bindung setzt den betroffenen Node auf
`FAILED`; abhängige In-Run-Nachfolger werden `BLOCKED`. Externe Referenz-,
Manifest-, Integritäts-, Rechte- und Budgetfehler werden niemals automatisch
wiederholt. Nur die vorhandene explizite `--retry N`-Option kann die bereits
definierten temporären Workerfehler wiederholen; jeder Versuch erhält eine
neue `attempt_id`.

Sidecar-Syntax-, Feldreihenfolge-, Vertrags-, Node- und Arityfehler werden als
Vorabprüfung vor der Laufinitialisierung mit Exit `2` abgewiesen. Fehler, die
erst beim Lesen oder Verifizieren eines konkreten Manifests, Objekts oder
Attempts festgestellt werden, erzeugen den betroffenen Node-Zustand und den
stabilen Exit `1`.

## Berichte und Vertraulichkeit

Der Reportvertrag wird auf `dag-scheduler-v5-payload-v14` versioniert und
behält die metadata-only Struktur von V1.3. Im äußeren Report steht
`external_inputs_digest` direkt nach `budget` und vor `status`; bei einem Lauf
ohne Sidecar ist der Wert `null`. Pro Receipt folgen nach den bestehenden
Usage-Feldern `external_input_count` und `external_input_bytes`. Ergänzt
werden damit nur:

- `external_inputs_digest` auf Laufebene,
- pro Receipt `external_input_count` und `external_input_bytes`.

Nicht ausgegeben werden Sidecar-Bytes, Manifest-IDs, Quell-Provenienz,
private Pfade, Storepfade, Payloadbytes, PIDs, `errno`-Texte oder
Kommandozeilen. Der Sidecar-Digest bindet die Auswahl, authentifiziert aber
keinen menschlichen Autor.

## Kompatibilität

- V1.3 ohne `--external-inputs` bleibt byte- und verhaltenskompatibel.
- V1.2 Resolve bleibt metadata-only und wird nicht zum Payloadleser.
- V2, V3, V4 und bestehende V5-Routen werden nicht erweitert.
- Der Worker-Frame und das 16er-Gesamtinputlimit bleiben unverändert.
- Bestehende Defaults und Maximalwerte bleiben autoritativ.

## Abnahmekriterien

V1.4 ist erst abnahmebereit, wenn reproduzierbare Tests belegen:

1. eine gültige Sidecar-Bindung für einen einzelnen Node;
2. mehrere geordnete externe Inputs und eine In-Run-Nachfolgerkante;
3. fehlende, nicht-READY, provenance-falsche und digest-falsche Manifeste;
4. unsichere Sidecar-Dateien, Pfadsyntax und unbekannte Nodes;
5. 15 gültige Payloadobjekte und Ablehnung des 16. Objekts;
6. Inputbyte-, Deadline- und Berechtigungsfehler ohne private Teilreste;
7. explizites Retry nur für bestehende Workerfehler;
8. unveränderte V1.3-, V2-, V3-, V4- und V5-Verträge;
9. Reports ohne Payloadbytes, private Pfade oder Quell-Manifeste.

## Offene Entscheidungen vor Implementierung

Keine. Der Sidecar-Name, CLI-Option, JSON-Feldreihenfolge, Provenienzfelder,
15er Objektgrenze, Fehlerklassen, Reportzusätze und Kompatibilitätsgrenzen
sind für die Implementierungsplanung festgelegt.
