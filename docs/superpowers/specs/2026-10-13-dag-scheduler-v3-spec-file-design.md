# DAG-Scheduler V3 – begrenzte DAG-Spec-Datei

**Status: VORSCHLAG — freigegebener Entwurf, vor Implementierungsplanung zu prüfen.**

## Zweck und Abgrenzung

V3 stellt eine reproduzierbare, dateibasierte Schnittstelle für den vorhandenen
DAG-Scheduler V2 bereit:

```text
Spec-Datei
  -> strenger V3-Parser und strukturelle Validierung
  -> DagSchedulerSpecV2 + fester Callback-Katalog
  -> execute_dag_scheduler_v2
  -> stabiler JSON-Bericht ohne Rohdaten
```

V3 führt **keinen** zweiten Scheduler ein und ändert weder die Ausführungs-
noch die Budgetsemantik von V2. Insbesondere bleiben die V2-Grenzen für
Knoten, Abhängigkeiten, Payloads und Empfangsbudgets maßgeblich.

Nicht Teil von V3 sind freie Callback-Ausführung, Skripte, Persistenz,
Bundle-/Replay-Dateien, Inspector-Ausgaben, parallele Ausführung,
GPU-Verarbeitung oder Cockpit-Visualisierungen.

## Öffentliche CLI

Der einzige V3-Befehl lautet:

```text
bruce dag-scheduler-v3 execute SPEC
```

Bei falscher Argumentzahl schreibt der Befehl exakt folgende eine Zeile nach
`stderr` und liefert einen Fehlerstatus:

```json
{"error":"Usage: bruce dag-scheduler-v3 execute SPEC"}
```

Die Registrierung erfolgt wie bei V2 unbedingt im Haupt-CLI-Dispatcher; sie
hängt nicht von NanoCell oder Cockpit ab.

## Dateiformat

Eine Spec-Datei darf höchstens 65.536 Bytes groß sein und besteht aus
ASCII-Text ohne NUL-Byte mit LF-Zeilentrennern. Ein CR-Zeichen, insbesondere
CRLF, ist ungültig. Es gibt weder Kommentare noch Whitespace-Normalisierung,
noch unbekannte Felder. Eine abschließende LF-Zeile ist zulässig, aber nicht
verpflichtend.

Die erste Zeile ist exakt:

```text
BRUCE_DAG_SCHEDULER_SPEC_V3
```

Danach folgt zunächst der globale Block und anschließend mindestens ein
Knotenblock. Je zwei Blöcke trennt genau eine Leerzeile; führende, doppelte
oder abschließende Leerzeilen sind ungültig. Innerhalb eines Blocks sind
Leerzeilen unzulässig. Jede nichtleere Zeile hat exakt die Form
`schlüssel=wert`.

Der globale Block enthält genau einmal und in dieser Reihenfolge:

```text
dag-id=<V2-konforme-ID>
max-started-nodes=<positive-dezimale-Ganzzahl>
max-total-receipt-bytes=<positive-dezimale-Ganzzahl>
```

Jeder Knotenblock enthält genau einmal und in dieser Reihenfolge:

```text
node=<V2-konforme-ID>
depends=<kommagetrennte-IDs oder leer>
callback=<literal-v1|append-v1|join-v1>
request-hex=<kleingeschriebene-hexadezimale-Bytes oder leer>
```

`node`-Blöcke sind strikt lexikografisch nach `node` aufsteigend. Nichtleere
Abhängigkeitslisten sind ebenfalls strikt lexikografisch aufsteigend,
kommagetrennt und enthalten keine Duplikate. Damit ist dieselbe inhaltliche
DAG immer in einer kanonischen Dateireihenfolge beschrieben.

`request-hex` nutzt nur die Zeichen `0-9` und `a-f`, ohne Präfix. Die Zahl der
Zeichen ist gerade; ein leerer Wert kodiert einen leeren Byte-String. Ein
fehlerhafter Hex-Wert wird nicht normalisiert oder stillschweigend akzeptiert.

Folgende Datei beschreibt die vorgesehene vierknotige Referenz-DAG:

```text
BRUCE_DAG_SCHEDULER_SPEC_V3
dag-id=example
max-started-nodes=4
max-total-receipt-bytes=4096

node=a-source
depends=
callback=literal-v1
request-hex=616c706861

node=b-transform
depends=a-source
callback=append-v1
request-hex=3a62657461

node=c-independent
depends=
callback=literal-v1
request-hex=67616d6d61

node=d-join
depends=b-transform,c-independent
callback=join-v1
request-hex=7c
```

Sie ergibt die Payloads `alpha`, `alpha:beta`, `gamma` und
`alpha:beta|gamma`; diese Klartextwerte dienen nur der Erklärung und werden
nicht in den V3-Bericht geschrieben.

## Fester Callback-Katalog

Der Parser erstellt für jeden Knoten eine V2-Callback-Bindung aus einem festen,
im Code hinterlegten Katalog:

| Callback | Abhängigkeiten | Ergebniscode | Berechnung |
|---|---:|---|---|
| `literal-v1` | 0 | `LITERAL_READY` | dekodierte `request-hex`-Bytes |
| `append-v1` | 1 | `APPENDED_READY` | direkte Eingabe-Payload, gefolgt von Request-Bytes |
| `join-v1` | 2 bis 15 | `JOINED_READY` | direkte Eingabe-Payloads in V2-Reihenfolge, mit Request-Bytes als Trennzeichen |

Die Arity wird vor der Ausführung geprüft. Der V2-Kern übergibt Eingaben nur
von `READY`-Vorgängern; die Adapter müssen keine alternativen Statuswerte
interpretieren.

Vor jedem Zusammenfügen prüft der Adapter überlaufsicher, ob das Ergebnis die
V2-Payloadgrenze von 65.536 Bytes überschreiten würde. In diesem Fall endet die
Ausführung kontrolliert mit einem V3-spezifischen Fehler, bevor eine zu große
Payload erzeugt wird. V2 behält seine eigene Ergebnis- und Budgetprüfung als
unabhängige Schutzschicht bei.

## Validierung und Fehlergrenzen

Der V3-Parser prüft vor jedem Callback-Aufruf:

- maximale Dateigröße und LF-only-Textform,
- exakten Header,
- vollständige globale und Knotenblöcke in fester Feldreihenfolge,
- unbekannte oder doppelte Schlüssel,
- kanonische Knoten- und Abhängigkeitsreihenfolge,
- Hex-Syntax, Callback-Namen und Callback-Arity,
- die strukturellen V2-Anforderungen einschließlich existierender
  Abhängigkeiten und Azyklizität.

Parserfehler verwenden präzise, datenarme Kennungen, etwa
`InvalidDagSchedulerV3SpecHeader`, `InvalidDagSchedulerV3SpecLine`,
`DuplicateDagSchedulerV3SpecKey`, `UnknownDagSchedulerV3SpecKey`,
`InvalidDagSchedulerV3SpecOrder`, `InvalidDagSchedulerV3SpecHex`,
`InvalidDagSchedulerV3SpecCallback` und
`InvalidDagSchedulerV3SpecCallbackArity`. Fehlermeldungen dürfen weder
`request-hex`-Werte noch dekodierte Payloads wiedergeben.

Ein Parser-, Struktur-, Zyklus-, Callback- oder Budgetfehler erzeugt keinen
Teilbericht. Die transaktionale V2-Eigenschaft bleibt erhalten: Es wird kein
partieller Ausführungslauf als Ergebnis zurückgegeben.

## Erfolgsbericht

Ein erfolgreicher Lauf schreibt genau einen stabil formatierten JSON-Bericht.
Er verwendet den Vertrag `dag-scheduler-v3-cli-v1` und enthält mindestens:

```text
contract
action
spec_version
spec_digest
scheduler_version
dag_id
max_started_nodes
max_total_receipt_bytes
receipts
trace
```

Werte:

- `action` ist `execute`.
- `spec_version` ist `dag-scheduler-spec-v3`.
- `spec_digest` ist der SHA-256-Digest der vollständigen, unveränderten
  Dateibytes.
- `scheduler_version` ist wahrheitsgemäß `dag-scheduler-v2`.
- `receipts` und `trace` stammen aus der V2-Ausführung und bewahren deren
  deterministische Sortierung beziehungsweise Chronologie.

Der Bericht enthält keine Request-Bytes, keine Payload-Bytes und keine
Hex-Kodierungen dieser Daten. Er enthält nur die von V2 ohnehin vorgesehenen
Digests, Größen, Status- und Ergebniscode-Metadaten.

## Geplante Implementierungsorte

| Datei | Änderung |
|---|---|
| `apps/dag_scheduler_v3_cli.hpp` | Neuer strenger Parser, fester Callback-Katalog, Ausführung und JSON-Renderer |
| `apps/bruce_cli.cpp` | Unbedingte Registrierung von `dag-scheduler-v3` |
| `CMakeLists.txt` | V3-CLI-Integrationstest registrieren |
| `tests/integration/test_dag_scheduler_v3_cli.py` | Prozessnahe Format-, Fehler- und Stabilitätstests |
| `docs/cli/README.md` | V3 als verfügbarer CLI-Vertrag |
| `docs/cli/commands.md` | Befehl, Grenzen und Ergebnisvertrag |
| `docs/cli/formats.md` | Vollständige V3-Spec-Referenz |
| `docs/architecture/03-blueprint-consolidation.md` | V3 als Adapter über V2 einordnen |

`docs/cli/source-index.md`, `docs/cli/examples.json` und
`docs/cli/validation.md` werden nicht von Hand bearbeitet. Falls ihre
Aktualisierung erforderlich ist, erfolgt sie ausschließlich durch
`docs/cli/verify_examples.py` in einer vollständigen temporären Kopie unter
einem nativen Pfad.

## Abnahmefälle

Die Implementierung muss mindestens beweisen:

1. Die vierknotige Referenz-DAG liefert die erwarteten Status, Ergebniscode,
   Größen und Digests in deterministischer Reihenfolge.
2. Leere Payloads und NUL-Bytes funktionieren über `request-hex` ohne
   Rohdatenleck im Bericht.
3. Header, CRLF, unbekannte und doppelte Felder, fehlerhafte Reihenfolge,
   ungültiges, ungerades oder großgeschriebenes Hex, falsche Callback-Arity,
   übergroße Datei und Zyklus werden vor einem Callback-Aufruf abgewiesen.
4. Mehrfaches Ausführen derselben Bytes liefert byteidentische Berichte; weder
   Klartext-Payload noch Request-Hex sind darin enthalten.
5. V2-Kern, V2-CLI, V3-CLI sowie die vorhandenen NanoCell- und Cockpit-OFF-
   Prüfungen bleiben erfolgreich.
6. Die Dokumentation nennt V3 ausdrücklich einen Parser-/CLI-Adapter über V2,
   nicht einen neuen Scheduler.

Der vorhandene CLI-Dokumentationsgenerator wird zusätzlich in einer nativen
vollständigen temporären Kopie geprüft. Falls er V3 nicht ausführt, wird das
als bekannte Abdeckungsgrenze dokumentiert; die V3-Prozessintegrationstests
bleiben der Laufzeitbeleg.
