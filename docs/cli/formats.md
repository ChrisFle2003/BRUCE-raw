# Dateiformate und Zahlendarstellung

[Handbuch](README.md) · [Befehle](commands.md)

## Gemeinsame Mod-Lifecycle-JSON-Verträge

**IMPLEMENTIERT UND BELEGT:** `mods inspect` liefert
`mod-package-inspection-v2`, `mods verify` bei erfolgreicher Vertrauensprüfung
`mod-package-verification-v2` mit zusätzlichem `verified: true`. Beide enthalten
`kind` (`data` oder `logic`), `mod_id`, `version`, den SHA-256-
`package_digest` und die kanonisch sortierten `capabilities`. Inspect ist kein
Signatur- oder Vertrauensbeleg.

`mods list`, `mods status` und erfolgreiche Lebenszyklusmutationen liefern
`bruce-mod-catalog-v2` mit dem Array `entries`. Jeder Eintrag enthält in fester
Reihenfolge `id`, `version`, `origin`, `kind`, `build_state`,
`lifecycle_state`, `package_digest` und `capabilities`. Die drei optionalen Built-ins haben
`origin: "builtin"`, `kind: "builtin"`, Buildzustand `available` oder
`not_built` sowie null für Lifecycle und Paketdigest. Installierte Data-/Logic-
Versionen haben `origin: "installed"`, ihren Kindwert, null als Buildzustand,
einen Lifecycle aus `installed_inactive`, `active`, `disabled`, `revoked` und
den vollständigen Paketdigest. Ein OFF-Build entfernt den Built-in-Eintrag
nicht.

`mods run` liefert bei akzeptierter Logic-Ausführung
`installed-logic-mod-run-v1` mit `mod_id`, `version`, `package_digest`,
`worker_sha256`, `template_id`, `accepted`, `status`, `result`, `claim_scope`,
`validator`, `binding_identity`, `request_digest` und `response_digest`. Pfade,
Workerbytes, stderr und Sandboxdiagnosen sind nicht Teil des Erfolgsvertrags.

Der bisherige `route-report-v1` bleibt ohne `--mod-details` bytegleich.
`route-report-v2` ersetzt `candidate_addresses` durch `candidates` mit
`address`, `template_id` und `owner`, ergänzt `selected_owner` und trägt in
`active_packages` den Mod-Kindwert. Ein Owner enthält `id`, `origin`, `kind`,
`version` und `package_digest`; beim Core-Tree-Besitzer sind Version und Digest
null, obwohl Core selbst kein Katalogeintrag ist. Der
Report ist kanonisch, höchstens 1 MiB einschließlich finalem LF und wird beim
Replay vollständig neu berechnet und byteweise verglichen.

Details zu Zustand, Sperren und Rollback stehen im
[Local Mod Lifecycle V2](../contracts/10-local-mod-lifecycle-v2.md).

## Logic-Mod Plugin V1: Paket und Laufrecord

**IMPLEMENTIERT UND BELEGT:** `BRUCE_LOGIC_MOD_PLUGIN_V1\n` ist ein separater
signierter Umschlag; bestehende Runtime-Data-Mod-Bytes werden unverändert
eingebettet. Das gesamte Paket ist auf 1 MiB begrenzt. Nach dem Header folgen
kanonisch längenpräfixierte Namen/Inhalte in genau dieser Reihenfolge:

1. `data.brucemod`: vollständiges signiertes Runtime-Data-Mod-Paket.
2. `logic.json`: kanonisches sortiertes JSON mit exakt `capabilities`,
   `template_ids`, `worker_path`, `worker_sha256`.
3. Genau ein Worker mit dem Namen aus `worker_path`.
4. `signature.ed25519`: 128 kleine Hexzeichen über alle vorangehenden
   Umschlagbytes; danach genau ein abschließendes LF.

Beide Signaturen werden mit dem expliziten Truststore geprüft; die äußere
verwendet den `signing_key_id` des eingebetteten Manifests. Der Paket-SHA-256
bindet den vollständigen Umschlag einschließlich Signaturen. Owner-Scope,
Mount, Version und Mod-ID stammen aus dem eingebetteten signierten Manifest.
Logic-Capabilities sind exakt eine dieser streng sortierten Mengen:
`["worker.execute"]`, `["artifact.input.v1","worker.execute"]`,
`["artifact.output.v1","worker.execute"]` oder
`["artifact.input.v1","artifact.output.v1","worker.execute"]`.
Andere Capability-Mengen werden abgelehnt. Template-IDs sind sortiert,
eindeutig, nicht leer und identisch zur eingebetteten Tree-Templatemenge.
Jedes Template benötigt einen zugehörigen signierten Routing-Golden-Case.
`worker_path` ist ein einzelner sicherer Dateiname, kein externer Pfad und kein
reservierter Paketmember. Der Worker-SHA-256 bindet seine genauen Bytes.

Das extrahierte Paketverzeichnis muss absolut und owner-only sein; seine
Pfadkomponenten werden ohne Symlinkverfolgung geöffnet. Der Worker ist eine
reguläre Datei mit genau einem Hardlink, Eigentümer ist der effektive Benutzer,
Modus 0500 oder 0700 ohne Sonderbits. Prüfungen erfolgen bei Verifikation,
Installation, Aktivierung und frisch bei Bindung. Der Runner verwendet nur
die unveränderlichen Bindungsbytes und öffnet den Workerpfad nicht erneut.
Linux-, Seccomp-Notify-, Einzel-exec- und Budgetvoraussetzungen stehen im
[Sitzungsvertrag](commands.md#logic-mod-plugin-v1--explizite-isolierte-sitzung).

Private Protokollfelder haben die Form `N:BYTES`, wobei `N` die kanonische
dezimale Byteanzahl ohne führende Nullen ist. Request und Antwort sind selbst
je ein solches Feld; zusätzliche Bytes und Felder führen zur Ablehnung.

| Nutzdatenheader | Nachfolgende längenpräfixierte Felder in Reihenfolge |
| --- | --- |
| `BRUCE_LOGIC_REQUEST_V1\n` | Bindungsidentität, Requestdigest, normalisierte Eingabe, Budgetentscheidung, optionaler Payloadanhang |
| `BRUCE_LOGIC_RESULT_V1\n` | identische Bindungsidentität, identischer Requestdigest, `candidate`, Codec, Ergebnisbytes, Ergebnis-SHA-256, optionaler Trace-SHA-256 |

Bindungsidentität enthält Mod-ID, Version, Paketdigest, Workerdigest,
Template-ID, Capability und Registrydigest. Der Registrydigest bindet die
sortierten Paketidentitäten und Zustände, den verifizierten Truststore und
Golden-Case-Tree-Identitäten. Die Budgetentscheidung enthält vier
längenpräfixierte Dezimalwerte: Wandzeit, Speicher, Eingabe, Output.
Requestdigest ist SHA-256 von `BRUCE_LOGIC_REQUEST_DIGEST_V1\n` und den
längenpräfixierten Feldern Identität, Eingabe, Budgetentscheidung sowie dem
gegebenenfalls vorhandenen Payloadanhang. Ohne Payload bleibt der bisherige
V1-Request bytegleich und besitzt keinen Anhang. Mit Payload folgen exakt
`1`, Manifest-ID, kanonischer Objektindex, Objekt-SHA-256, Objektgröße und
`artifact-bytes-v1`, jeweils längenpräfixiert. Der Anhang bindet eine
Identität, nie Store- oder andere Pfade. Der optionale Trace-Digest ist leer
oder besteht aus genau 64 kleinen Hexzeichen; er belegt keine überprüfte
Traceausführung. Ergebnisdigest und unabhängiger Hostvalidator müssen passen;
nur `rational-v1` für `math.rational.power.v1` wird akzeptiert. Felder und
Antwort sind höchstens 65536 Bytes, zusätzlich gilt das engere Outputbudget
einschließlich stdout/stderr.

Ein lokaler Record beginnt mit `BRUCE_LOGIC_RUN_V1\n`, dann folgen exakt:
Akzeptanz (`1`/`0`), Status, Ergebnis, Hostvalidator, Host-Claim-Scope,
Bindungsidentität, normalisierte Eingabe, Budgetentscheidung, Requestdigest,
SHA-256 des gesamten Antwortframes und dessen vollständige Bytes, jeweils
längenpräfixiert. Ohne Input und Output endet der Record dort und bleibt
bytegleich. Ein Payloadrecord fügt dieselben sechs Felder wie der Requestanhang
an. Nur nach erfolgreicher Ergebnisveröffentlichung folgen danach (oder ohne
Input direkt nach dem bisherigen Präfix) exakt sieben längenpräfixierte Felder:
`output.v1`, `1`, `output_manifest_id`, `output_object_index`, `output_sha256`,
`output_size_bytes`, `output_codec`. Objektindex ist `0`, Codec fest
`logic-mod-result-text-v1`, Größe höchstens 16384 Bytes. Ohne Ausgabe gibt es
keinen zusätzlichen Discriminator und kein Null-Präsenzfeld; alte inputfreie
und Input-only-Records bleiben bytegleich. Ausgabeidentität enthält keinen
Storepfad und verändert weder Requestframe noch Requestdigest. Die
Lesebegrenzung ist 131072 Bytes. Fehlerrecords enthalten keine akzeptierten
Ergebnisse; bei Outputüberschreitung wird nur der bereits innerhalb des Budgets
gelesene Präfix behalten. Replay akzeptiert nur einen vollständig erneut
hostvalidierten, byteidentischen Erfolgsrecord unter aktueller aktiver Bindung.
Bei Payload verlangt es dieselbe explizite Auswahl, löst das lokale Objekt
erneut auf und prüft dessen gebundene Identität erneut. Es ist
Transkriptprüfung ohne Workerstart und ohne signierte Aussage über historische
Ausführung. Records enthalten Eingabe und Antwortbytes sowie gegebenenfalls
Payloadmetadaten; die Sitzungs-JSON-Ausgabe enthält nur das beschriebene
Ergebnis und Digests.

### Result Artifact V1

**IMPLEMENTIERT UND BELEGT:** Der Host veröffentlicht ausschließlich die
unveränderten UTF-8-Bytes des akzeptierten `result` als ein lokales Objekt,
höchstens 16384 Bytes. Das kanonische Manifest hat
`run_id=node_id=attempt_id=MANIFEST_ID`, `status=READY`,
`result_code=ACCEPTED` und genau ein Objekt mit
`media_type=logic-mod-result-text-v1`, `origin=logic-mod-result-v1`.
Die bestehende Artifact-Store-Atomizität und Idempotenz gelten unverändert;
ein abweichendes Manifest wird nicht überschrieben. Die feste Codecbezeichnung
ist Manifest-Medientyp und Recordmetadatum, keine freie Workeroption.
Replay verlangt passende explizite Outputkonfiguration, öffnet ausschließlich
einen vorhandenen Store frisch und prüft das vollständige feste Manifest,
Objektidentität und gespeicherte Bytes über Storeverifikation sowie SHA-256
und Länge gegen das unabhängig hostvalidierte Ergebnis. Es startet keinen
Worker. Fehlende, korrupte, nicht-READY oder anders kodierte Artefakte scheitern
kontrolliert. Storepfade und Ausgaberechte gelangen nicht in das Workerprotokoll.
V1 unterstützt weder Mehrfachausgaben, Streams, freie Codecs, Überschreiben,
Garbage-Collection, Remote-Stores, Wiederholungen, direkte Plugin-zu-Plugin-Übergabe noch
parallele Plugin-Ausführung.

### Logic-Mod Result Inspector V1 JSON

**IMPLEMENTIERT UND BELEGT:** Der eigenständige
[`logic-mod-result-inspect`](commands.md#logic-mod-result-inspect--lokales-ergebnis-schreibgeschützt-prüfen)
verwendet den vorhandenen kanonischen Laufrecordparser und den vorhandenen
hostseitigen Result-Artifact-V1-Verifier. Er erzeugt kein neues Recordformat.
Sein Report ist ein einzelnes JSON-Objekt mit genau diesen Schlüsseln in
Reihenfolge, gefolgt von einem LF:

| Feld | Typ und Bedeutung |
| --- | --- |
| `contract` | String, fest `logic-mod-result-inspect-v1` |
| `action` | String, fest `inspect` |
| `status` | Einer der unten aufgeführten Statusstrings |
| `accepted` | Boolean aus dem Record; keine frische Autorisierung |
| `input_present` | Boolean: Record enthält Inputmetadaten; keine Prüfung des Inputartefakts |
| `output_present` | Boolean: Record enthält Outputmetadaten |
| `binding_identity` | Kleingeschriebener SHA-256 des rohen Bindungsfelds, niemals dessen Inhalt; bei ungültigem Record null |
| `output_manifest_id` | String aus dem Record oder null |
| `output_sha256` | String aus dem Record oder null |
| `output_size_bytes` | JSON-Ganzzahl aus dem Record oder null |
| `output_codec` | String aus dem Record, fest `logic-mod-result-text-v1`, oder null |
| `replay_readiness` | `READY` bei `status=READY`, `NO_OUTPUT` bei `status=NO_OUTPUT`, sonst `NOT_READY` |

Alle vier Outputfelder sind null, wenn `output_present=false`. Bei einem
parsergültigen Outputrecord bleiben sie auch bei fehlgeschlagener
Storeprüfung sichtbar; ihre Präsenz ist keine Verifikation. Bei
`INVALID_RECORD` sind alle drei Booleans false und Bindungs-/Outputmetadaten
null; es werden keine Teilparsergebnisse ausgegeben. Der Bindungsdigest ist
eine stabile Korrelation des opaken Legacyfelds, keine Autorisierung.

| `status` | Bedingung |
| --- | --- |
| `READY` | Vorhandenes READY-Manifest und vollständige Result-Artifact-V1-Verifikation erfolgreich |
| `NO_OUTPUT` | Gültiger Legacy-/Input-only-Record ohne Output, auch bei `accepted=false`; kein Storezugriff |
| `MISSING` | Vorhandene Store-API meldet fehlendes Manifest oder Objekt |
| `NOT_READY` | Kanonisch gelesenes Manifest trägt einen anderen Status als READY |
| `IDENTITY_MISMATCH` | Result-Verifier meldet Identitätsabweichung oder korrupte Objektbytes, etwa bei abweichendem Codec oder Ergebnisdigest |
| `STORE_ERROR` | Store nicht zu öffnen (auch fehlender Store), ungültiger Storetyp, ungültiges Manifest oder sonstiger Store-/Verifikationsfehler |
| `INVALID_RECORD` | Recorddatei abgewiesen/unlesbar oder kanonischer Recordparser schlägt fehl |

Die Klassifikation nutzt bestehende Fehlercodes, keine Pfad-/errno-Heuristik.
Insbesondere ist ein fehlender Store `STORE_ERROR`, kein `MISSING`.
Ein nicht-READY-Manifest wird bereits vor der vollständigen Ergebnisprüfung
als `NOT_READY` eingeordnet. Nur nach erfolgreichem Result-Verifier entsteht
`READY`; `replay_readiness` führt selbst kein Replay aus. Es gibt keine
Registryautorisierung, Inputverifikation, Workerstarts, Publikation oder
Reparatur. Lokale Pfade, Payloadbytes, Ergebnistext, rohe Bindungsbytes,
Antwortframes und Workerdiagnostik gehören nicht zum Report.
Belege: [Inspectorverträge](../../tests/unit/logic_mod_result_inspector_v1_tests.cpp)
und [CLI-Workflows](../../tests/integration/test_logic_mod_result_inspector_v1.py).

### Payload Capability V1

**IMPLEMENTIERT UND BELEGT:** Der optionale Payloadpfad übergibt genau ein
vollständig verifiziertes lokales Artifact-Store-V1.2-Objekt. Akzeptiert wird
nur ein kanonisches `READY`-Manifest mit vorhandenem Objekt am gewählten Index;
Digest, Größe und Objektbytes werden geprüft. Die Grenze beträgt exakt
1048576 Bytes: 1048576 Bytes sind zulässig, 1048577 Bytes nicht. Der Host
kopiert die verifizierten Bytes in eine mit Write/Grow/Shrink/Seal-Schutz
versiegelte, read-only Memfd und reicht nur diese Momentaufnahme weiter.

Der Worker erhält die Memfd ausschließlich auf FD 6. Seccomp gestattet dafür
`read`, `fstat`, `lseek` und `close`; `write(6, ...)` und ein file-backed
`mmap(..., 6, ...)` werden beendet. Nach dem Start bestehen weder
Storezugriff noch Payloadpfad, weitere Eingabedeskriptoren, Netzwerk-,
Dateisystem-, Shell- oder Unterprozesszugriff. Der Host löst zuerst über eine
frische `artifact.input.v1`-Bindung auf und fordert unmittelbar vor dem Spawn
erneut Input- und `worker.execute`-Bindungen an; Package-, Worker-, Template-
und Registryidentität müssen übereinstimmen.

V1 erweitert keine mehreren Eingaben, Schreibzugriff, Streams, Remote-Fetch
während eines Laufs, Worker-Storezugriff, freien Codecs, Pfadübergabe oder
Plugin-zu-Plugin-Datenfluss. Belege: [Resolververträge](../../tests/unit/logic_mod_payload_v1_tests.cpp),
[FD-6-Workerverträge](../../tests/unit/logic_mod_payload_worker_v1_tests.cpp)
und [reale Payload-CLI-Workflows](../../tests/integration/test_logic_mod_payload_v1.py).

Belege: [Paketgrenze](../../src/mod/logic_mod_plugin_v1.cpp),
[Umschlagprüfung](../../src/mod/runtime_package.cpp),
[Frame-/Replayprüfung](../../src/mod/logic_mod_worker_v1.cpp).
Es gibt keine dauerhafte Logic-Registrydatei, keine externen Payloadreferenzen
und keinen allgemeinen ausführbaren Pluginvertrag.

## Logic-Mod Artifact Chain V1: Record und JSON

**IMPLEMENTIERT UND BELEGT:** Der separate private Chain-Record beginnt mit
`BRUCE_LOGIC_MOD_ARTIFACT_CHAIN_V1\n`. Danach folgen exakt fünf
längenpräfixierte Felder `N:BYTES` in dieser Reihenfolge und ein abschließendes
LF; die gesamte Datei ist auf 524288 Bytes begrenzt:

1. Chain-ID: 64 kleine Hexzeichen.
2. Unveränderter vollständiger `BRUCE_LOGIC_RUN_V1`-Record von A.
3. Unveränderter vollständiger `BRUCE_LOGIC_RUN_V1`-Record von B.
4. Kanonische Result-Artifact-Identität von A.
5. Kanonische Result-Artifact-Identität von B.

Jede Result-Artifact-Identität beginnt mit
`BRUCE_LOGIC_RESULT_ARTIFACT_V1\n`, danach stehen Manifest-ID, Objektindex,
SHA-256, Größe und Codec als fünf längenpräfixierte Felder, gefolgt von LF.
Index ist stets `0`, Codec `logic-mod-result-text-v1`, Größe höchstens 16384
Bytes. Beide Manifeste sind verschieden. Nur vollständig akzeptierte
A-/B-Records sind zulässig: A hat keinen Payloadinput und beide haben eine
Ausgabe; B's Input bindet exakt A's Manifest, Index 0, Digest und Größe mit
Inputcodec `artifact-bytes-v1`. Eingebettete und äußere Ausgabeidentitäten
müssen übereinstimmen. Nichtkanonische Längen/Zahlen, zusätzliche Felder,
Restbytes, unzulässige Identitäten und überschrittene Grenzen scheitern;
Decode und erneute kanonische Serialisierung müssen bytegleich sein.

Die Chain-ID ist SHA-256 von `BRUCE_LOGIC_MOD_ARTIFACT_CHAIN_ID_V1\n` und
sechs längenpräfixierten Feldern: A-Bindungsidentität, B-Bindungsidentität,
normalisierte A-Eingabe, normalisierte B-Eingabe, A-Ausgabe-Manifest-ID,
B-Ausgabe-Manifest-ID. Sie bindet diese Auswahl deterministisch ohne Pfade
oder Zufall; sie ist keine Signatur und keine unabhängige Attestation.

Die eingebetteten Run-Records enthalten ihre normalisierten Texteingaben,
Ergebnisse und vollständigen Antwortframes einschließlich ihrer Bindungen
und Budgets. Der Chain-Record ist daher ein vertraulicher Klartextcontainer,
keine reine Metadatenansicht. Er enthält keine Hostpfade, Workerdiagnostik
oder gesonderte Kopie der übertragenen Artifact-Payloadbytes. Bestehende
Single-Plugin-, Payload- und Result-Artifact-Recordbytes ändern sich nicht.
Dateipublikation, aktuelle A/B-Autorisierung und workerfreies Replay stehen
im [Chain-Befehl](commands.md#logic-mod-chain-v1--zwei-serielle-artifact-schritte).
Replay prüft die eingebetteten Records sowie beide Artefakte frisch aus dem
lokalen Store; ein formal gültiger Chain-Record allein belegt kein Replay.

Die öffentliche JSON-Zeile hat die feste Feldreihenfolge:

| Feld | Typ / Bedeutung |
| --- | --- |
| `contract` | String, fest `logic-mod-artifact-chain-v1` |
| `action` | `run` oder `replay` |
| `accepted` | Boolean |
| `status` | `ACCEPTED`, `CHAIN_FIRST_FAILED` oder `CHAIN_SECOND_FAILED` |
| `chain_id` | 64 kleine Hexzeichen nach vollständigem Erfolg, sonst null |
| `first` | A-Ausgabeobjekt oder null |
| `second` | B-Ausgabeobjekt oder null |

Ein Ausgabeobjekt enthält in dieser Reihenfolge `manifest_id` (String),
`sha256` (64 kleine Hexzeichen), `size_bytes` (Ganzzahl) und `codec` (fest
`logic-mod-result-text-v1`). Der feste Index 0 ist implizit. Weder diese
JSON-Ausgabe noch die festen CLI-Fehler enthalten rohe Eingaben, Ergebnisse,
Payloads, Antwortframes, Hostpfade oder Workerdiagnostik. Belege:
[Record-/Replayverträge](../../tests/unit/logic_mod_artifact_chain_v1_tests.cpp)
und [Real-Binary-Verträge](../../tests/integration/test_logic_mod_artifact_chain_v1.py).

## Logic-Mod Artifact DAG V2: Spec, Payload, Record und JSON

**IMPLEMENTIERT UND BELEGT:** Die [V2-CLI](commands.md#logic-mod-artifact-dag-v2--begrenzter-serieller-artifact-dag)
liest genau ein JSON-Objekt mit `schema_version:2` und `nodes` (1–16 Objekte).
Dies ist ein strukturelles Beispiel; Paket- und Truststoredateien müssen
vorhanden und passend signiert sein:

```json
{
  "schema_version": 2,
  "nodes": [
    {
      "id": "a",
      "package": "root.brucemod",
      "trust_store": "trust.txt",
      "input": "3/4 hoch 2",
      "predecessors": [],
      "output_manifest": "a-out"
    },
    {
      "id": "b",
      "package": "child.brucemod",
      "trust_store": "trust.txt",
      "input": "1/2 hoch 2",
      "predecessors": ["a"],
      "input_manifest": "b-input",
      "output_manifest": "b-out",
      "budgets": {
        "wall_ms": 250,
        "memory_bytes": 67108864,
        "input_bytes": 4096,
        "output_bytes": 16384
      }
    }
  ]
}
```

| Knotenfeld | Exakter Vertrag |
| --- | --- |
| `id` | Pflichtstring, 1–64 Zeichen aus `[A-Za-z0-9_-]`, global eindeutig |
| `package`, `trust_store` | Pflichtstrings, jeweils 1–4096 Bytes, nicht nur Leerraum; explizite Dateipfade |
| `input` | Pflichtstring; Rohbytes müssen ins Inputbudget passen, anschließend bestehende Textnormalisierung |
| `predecessors` | Pflichtarray mit 0–15 verschiedenen vorhandenen Knoten-IDs, lexikographisch sortiert; keine Selbstkante/Zyklen |
| `output_manifest` | Pflichtstring, gültige Manifest-ID |
| `input_manifest` | Genau bei Nichtwurzelknoten Pflichtstring; bei Wurzeln verboten |
| `budgets` | Optionales Objekt; wenn vorhanden, sind alle vier unten genannten Ganzzahlen Pflicht |

Manifest-IDs haben 1–128 ASCII-Zeichen, beginnen alphanumerisch und enthalten
sonst nur alphanumerische Zeichen, `_`, `-`, `.`. Sämtliche Output- und internen
Input-Manifest-IDs des Graphen müssen paarweise verschieden sein.
Budgets sind vorzeichenlose dezimale Ganzzahlen ohne führende Nullen,
Bruchteil oder Exponentialnotation. Grenzen/Defaults:

| Budget | Bereich | Default bei fehlendem Objekt |
| --- | --- | --- |
| `wall_ms` | 1–5000 | 250 |
| `memory_bytes` | 8388608–268435456 | 67108864 |
| `input_bytes` | 1–4096 | 4096 |
| `output_bytes` | 1–65536 | 16384 |

Auf allen Ebenen sind unbekannte und doppelte Schlüssel unzulässig; JSON-
Escapes werden vor dem Duplikatvergleich decodiert. Die Spec ist auf 65536
Bytes begrenzt. Ihr Digest ist SHA-256 über
`BRUCE_LOGIC_MOD_DAG_SPEC_V2\n` plus kanonisches JSON: Objektschlüssel
lexikographisch sortiert, Knoten nach `id` sortiert, Input normalisiert und
alle Defaultbudgets explizit eingesetzt. Kein Whitespace wird zwischen
JSON-Werten eingefügt. Schlüssel-/Knotenreihenfolge und explizite Defaultbudgets
ändern den Digest nicht. Paket-/Truststorepfadstrings bleiben dagegen Teil
des Digests, auch wenn Record und öffentliche JSON keine Pfade enthalten.

Das interne Inputdokument beginnt mit `BRUCE_LOGIC_MOD_DAG_INPUT_V2\n`.
Danach folgen längenpräfixierte Felder `N:BYTES` ohne Trennzeichen:
Schemawert `2`, Vorgängeranzahl und für jeden direkten Vorgänger in ID-Reihenfolge
jeweils Knoten-ID, Manifest-ID, SHA-256, dezimale Ergebnisgröße, Codec und
kanonische Ergebnisbytes. Ein LF schließt das Dokument ab; die Gesamtgrenze
ist 1048576 Bytes. Der Host publiziert es als READY/ACCEPTED-Manifest mit
`run_id = node_id = attempt_id = input_manifest`, genau einem Objekt an Index 0,
Codec und Rolle `logic-mod-dag-input-v2`. Über FD 6 erhält der Worker dessen
versiegelte Bytes unter dem bestehenden Inputcodec `artifact-bytes-v1`.

Der private Record beginnt mit `BRUCE_LOGIC_MOD_ARTIFACT_DAG_V2\n` und ist
auf 8388608 Bytes begrenzt. Anschließend stehen als `N:BYTES`:

1. DAG-ID, Spec-Digest und dezimale Knotenzahl.
2. Pro Knoten in stabiler topologischer Reihenfolge: Knoten-ID,
   Vorgängeranzahl und jede Vorgänger-ID, vollständiger unveränderter
   Run-V1-Record, kanonische Result-Artifact-Identität und Input-Präsenz `0`/`1`.
3. Bei Input-Präsenz `1` unmittelbar die kanonische interne Input-Identität.

Ein LF beendet den Record. Artifact-Identitäten verwenden jeweils die Hülle
`BRUCE_LOGIC_RESULT_ARTIFACT_V1\n` mit fünf `N:BYTES`-Feldern Manifest-ID,
Objektindex `0`, SHA-256, Größe, Codec und abschließendem LF. Resultcodecs
sind `logic-mod-result-text-v1`, interne Inputcodecs `logic-mod-dag-input-v2`.
Die DAG-ID ist SHA-256 über `BRUCE_LOGIC_MOD_ARTIFACT_DAG_ID_V2\n` und den
Recordkörper ab Spec-Digest einschließlich aller Knotendaten, ohne äußere
DAG-ID und abschließendes äußeres LF. Sie ist keine Signatur.
Nichtkanonische Zahlen/Längen, Restbytes, falsche Reihenfolge, uneinheitliche
Run-/Artifactbindungen oder überschrittene Grenzen werden verworfen; Lesen
und kanonische Neu-Serialisierung müssen bytegleich sein. Eingebettete Runs
behalten normalisierte Inputs, Ergebnisse und Response-Frames. Zusätzliche
Payloadkopien, Hostpfade und Workerdiagnostik werden nicht gespeichert.

Die öffentliche JSON-Zeile hat diese feste Reihenfolge:

| Feld | Typ / Bedeutung |
| --- | --- |
| `contract` | String, fest `logic-mod-artifact-dag-v2` |
| `action` | `run` oder `replay` |
| `accepted` | Boolean |
| `status` | `ACCEPTED` oder `DAG_NODE_FAILED` |
| `dag_id` | 64 kleine Hexzeichen bei Erfolg, sonst null |
| `spec_digest` | 64 kleine Hexzeichen |
| `node_order` | Gesamte deterministische topologische Reihenfolge als ID-Array |
| `nodes` | Bisher gestartete Knoten in Ausführungsreihenfolge |

Jeder Knoteneintrag hat genau `id`, `result_artifact`, `internal_input_artifact`
in dieser Reihenfolge. Artifactobjekte haben genau `manifest_id`, `sha256`,
`size_bytes`, `codec`; der feste Objektindex 0 bleibt implizit. Nicht publizierte
Resultate und fehlende interne Inputs sind null. Bei einem gescheiterten
Nichtwurzelknoten kann sein bereits publiziertes internes Inputobjekt
vorhanden sein. Weder diese JSON noch feste CLI-Fehler enthalten Pfade,
Eingaben, Ergebnisse, Payloads, Response-Frames oder Workerdiagnostik.
Belege: [Coreverträge](../../tests/unit/logic_mod_artifact_dag_v2_tests.cpp),
[Real-Binary-Verträge](../../tests/integration/test_logic_mod_artifact_dag_v2.py).

## Logic-Mod Artifact DAG V3: kompatible Formate

**IMPLEMENTIERT UND BELEGT:** Die [V3-CLI](commands.md#logic-mod-artifact-dag-v3--begrenzte-parallele-artifact-wellen)
verwendet die [V2-Spec, Payloads und Records](#logic-mod-artifact-dag-v2-spec-payload-record-und-json)
unverändert: `schema_version:2`, Spec-Digestpräfix
`BRUCE_LOGIC_MOD_DAG_SPEC_V2\n`, Inputmarker
`BRUCE_LOGIC_MOD_DAG_INPUT_V2\n`, Recordmarker
`BRUCE_LOGIC_MOD_ARTIFACT_DAG_V2\n` und DAG-ID-Präfix
`BRUCE_LOGIC_MOD_ARTIFACT_DAG_ID_V2\n`. Grenzen, kanonische Reihenfolge,
eingebettete Run-V1-Records und private Dateirechte bleiben gleich.
Die lokale Run-Option `--max-parallel-nodes 1..4` ist kein JSON-Feld;
ein zusätzliches Spec-Feld `max_parallel_nodes` wird abgewiesen.
Die Grenze und physische Wellen-/Zeitinformationen werden nicht serialisiert
und gehen nicht in Spec-Digest oder DAG-ID ein. Bei gleichen deterministischen
Workertranskripten bleiben Records über alle vier Grenzen bytegleich,
einschließlich des seriellen V2-Formats. V3-Replay prüft dieses Format ohne
Worker und ohne Paralleloption.

Die öffentliche JSON hat dieselben Felder und dieselbe Feldreihenfolge wie
V2, mit festem `contract:"logic-mod-artifact-dag-v3"`. `node_order` ist
weiterhin die gesamte stabile topologische Ordnung; `nodes` enthält nur
gestartete Knoten, ebenfalls in dieser logischen Ordnung. Nach einem
kontrollierten Fehlschlag umfasst es die vollständig abgewartete Welle,
einschließlich erfolgreicher Peers. Physische Start-/Abschlussreihenfolge,
Parallelgrenze, Pfade, Eingaben, Ergebnisse und Payloadbytes werden nicht
ausgegeben. Der unveränderte Record ist ein vertraulicher Klartextcontainer;
die Metadatenfilterung ist keine Verschlüsselung oder Ausführungsattestation.
Belege: [Coreverträge](../../tests/unit/logic_mod_artifact_dag_v3_tests.cpp),
[Real-Binary-Verträge](../../tests/integration/test_logic_mod_artifact_dag_v3.py).

## Logic-Mod Artifact DAG V4: lokale Budgets ohne Formatwechsel

**IMPLEMENTIERT UND BELEGT:** Die [V4-CLI](commands.md#logic-mod-artifact-dag-v4--lokale-hostbudgets-für-artifact-wellen)
verwendet unverändert die [V2-Spec, Payloads und Records](#logic-mod-artifact-dag-v2-spec-payload-record-und-json)
und damit dieselben Formate wie V3: `schema_version:2`, Spec-Digestpräfix
`BRUCE_LOGIC_MOD_DAG_SPEC_V2\n`, Inputmarker
`BRUCE_LOGIC_MOD_DAG_INPUT_V2\n`, Recordmarker
`BRUCE_LOGIC_MOD_ARTIFACT_DAG_V2\n` und DAG-ID-Präfix
`BRUCE_LOGIC_MOD_ARTIFACT_DAG_ID_V2\n`. Es gibt keine neue V4-Recordhülle.

Die fünf lokalen Run-Optionen `--max-parallel-nodes`, `--max-total-wall-ms`,
`--max-total-payload-bytes`, `--max-started-workers` und `--cancel-after-ms`
sind keine JSON-Spec-Felder; zusätzliche Felder wie `max_started_workers`
werden abgewiesen. Hostgrenzen, Timer, Startzähler und Wellenzeiten werden
weder serialisiert noch in Spec-Digest oder DAG-ID aufgenommen. Bei gleichen
deterministischen Workertranskripten bleiben erfolgreiche Records bytegleich
mit V2/V3; Replay prüft sie schreibgeschützt ohne Worker und ohne Run-Optionen.

Die öffentliche JSON behält V2-Felder und Feldreihenfolge mit festem
`contract:"logic-mod-artifact-dag-v4"`. `node_order` enthält die gesamte stabile
topologische Ordnung; `nodes` enthält abgeschlossene Wellen in logischer
Reihenfolge. Ein kontrollierter Budget-/Cancel-Stopp hat `accepted:false`,
`dag_id:null` und `status:"DAG_GLOBAL_BUDGET_EXHAUSTED"` beziehungsweise
`status:"DAG_HOST_CANCELLED"`; er erzeugt keinen Erfolgsrecord. Die abgewiesene
Welle veröffentlicht keine internen Input Artifacts. Frühere Artefakte bleiben
bestehen. Das Bytebudget zählt kumulativ vollständige interne Input-Dokumente
je Empfänger, nicht sämtliche gespeicherten oder im Prozess gehaltenen Bytes.

Pfade, Eingaben, Ergebnisse, rohe Payloadbytes und lokale Hostkontrollen
erscheinen nicht in der Metadaten-JSON. Die unveränderten privaten Records
enthalten vertrauliche Klartextdaten; Metadatenfilterung bietet keine
Verschlüsselung oder Ausführungsattestation. Belege:
[Coreverträge](../../tests/unit/logic_mod_artifact_dag_v4_tests.cpp),
[Real-Binary-Verträge](../../tests/integration/test_logic_mod_artifact_dag_v4.py).

## Logic-Mod Artifact DAG V5: Remote-Restore ohne Formatwechsel

**IMPLEMENTIERT UND BELEGT:** Die [V5-CLI](commands.md#logic-mod-artifact-dag-v5--hostseitiger-remote-restore-erwarteter-vorgänger)
verwendet exakt die V2-Formate, die V3 und V4 schon beibehalten:
`schema_version:2`, Spec-Digestpräfix `BRUCE_LOGIC_MOD_DAG_SPEC_V2\n`,
Inputmarker `BRUCE_LOGIC_MOD_DAG_INPUT_V2\n`, Recordmarker
`BRUCE_LOGIC_MOD_ARTIFACT_DAG_V2\n` und DAG-ID-Präfix
`BRUCE_LOGIC_MOD_ARTIFACT_DAG_ID_V2\n`. Es gibt weder eine V5-Recordhülle
noch ein neues Payload-, Worker- oder lokales Artifact-Format.

`--remote-restore HTTPS_BASE` ist keine Spec-Eigenschaft. Die URL, der
Restoreversuch, die physischen HTTP-Übertragungen und V4-Mirror-Envelopebytes
werden weder serialisiert noch in Spec-Digest, DAG-ID, internen Payload oder
öffentliche Metadaten aufgenommen. Deshalb bleiben erfolgreiche Records bei
gleichen deterministischen Workertranskripten bytegleich mit V2, V3 und V4.
Die öffentliche JSON verwendet allein den Kennzeichner
`contract:"logic-mod-artifact-dag-v5"` bei unveränderter Feldreihenfolge und
denselben metadata-only Grenzen.

Die erwartete Vorgängeridentität kommt ausschließlich aus der bereits
kanonisch gebundenen DAG-Ausgabe. V5 darf nur die bestehende V4-Route
`/manifests/<MANIFEST_ID>.mirror` und deren referenzierte Objektpfade nutzen;
deren Envelope-, TLS-, Digest-, Größen- und Publikationsregeln bleiben
vollständig maßgeblich. Nach Restore wird das lokal publizierte V1-Manifest
gegen die DAG-Bindung erneut geprüft. Ein Remote-Envelope kann damit keine
Spec, Record, Payloadreferenz oder Ergebnisidentität ändern.

Replay nimmt keine Remoteoption an und bleibt offline, workerfrei und
schreibgeschützt. Belege: [V5-Core](../../src/mod/logic_mod_artifact_dag_v5.cpp),
[V4-Restore](../../src/artifact/artifact_store_v4.cpp),
[V5-Core-Verträge](../../tests/unit/logic_mod_artifact_dag_v5_tests.cpp) und
[V5-CLI-Workflows](../../tests/integration/test_logic_mod_artifact_dag_v5_cli.py).

## Logic-Mod Artifact DAG V6: Supervision ohne Formatwechsel

**IMPLEMENTIERT UND BELEGT:** V6 verwendet für Spec, Spec-Digest, internen
Input, DAG-ID und Erfolgsrecord unverändert die V2-Formate, die V3–V5 bereits
bewahren. `--report`, die hostseitige Attempt-Telemetrie, V4-Budgets und ein
V5-Restore sind Laufkontrollen und verändern keine kanonischen Recordbytes.
Bei gleichen deterministischen Workertranskripten sind erfolgreiche V2–V6-
Records bytegleich; V6-Replay bleibt allein ein lokaler, schreibgeschützter
V2-Record-Check und liest kein Sidecar.

Der nur lokale Diagnosesatz liegt unter
`REPORT.run/control/supervision-v6.jsonl`. Er ist kein neues DAG- oder
Payloadformat. Seine Datei ist owner-only (`0600`) unter owner-only
Verzeichnissen (`0700`), wird descriptor-relativ ohne Symlink-Following
geöffnet und besteht aus kanonischen JSONL-Ereignissen mit global steigender
`seq`. Ein Ereignis enthält nur Schema, opaque Run-/Attempt-/Node-ID,
Ereignis, Status, optionalen terminalen Grund, Dauer/Grenzen und optionale
Peakwerte samt Verfügbarkeit. Verboten sind Pfade, PID/PGID, Befehlszeilen,
Frames, Payloadbytes, URLs, Signaturen, Truststoreinhalt und Geheimnisse.

`peak_memory_availability` lautet genau `OBSERVED`, `UNAVAILABLE` oder
`NOT_APPLICABLE`. Nur `OBSERVED` darf `peak_memory_bytes` tragen; fehlende
Messung wird nicht als Nullwert serialisiert. Die Spur ist diagnose-only:
ein abgelehnter, syntaktisch abgeschnittener oder manipuliert erscheinender Satz kann kein
Ergebnis akzeptieren, keinen Replay beeinflussen und keinen Cleanup auslösen.
Siehe [V6-CLI](commands.md#logic-mod-artifact-dag-v6--linux-lokale-hostsupervision-mit-diagnose-sidecar),
[Sidecar-Implementierung](../../src/mod/logic_mod_supervision_log_v6.cpp) und
[Sidecar-Verträge](../../tests/unit/logic_mod_supervision_log_v6_tests.cpp).

## Runtime Orchestrator V1: foreground JSONL ohne neues Laufformat

**IMPLEMENTIERT UND BELEGT:** `runtime-orchestrator-v1 serve` verwendet nur
einen begrenzten lokalen JSONL-Steuerkanal. Es erzeugt kein neues Spec-,
Artifact-, Payload-, Sidecar- oder kanonisches Recordformat. Jede
nichtleere stdin-Zeile ist genau ein kanonisches JSON-Objekt ohne
Leerraumvarianten; jede Zeile erhält genau eine kanonische JSON-Antwort.
Eine Zeile ist auf 65536 Bytes begrenzt. Übergroße Zeilen werden bis zum
nächsten Zeilentrenner verworfen und ergeben genau `INVALID_COMMAND`; eine
abschließende nichtleere EOF-Zeile wird noch als ein Befehl verarbeitet.

Die zulässigen Requestformen sind:

```json
{"action":"status","run_id":"orv1-1"}
{"action":"cancel","run_id":"orv1-1"}
{"action":"admission-status","run_id":"orv1-1"}
```

und für `submit` ein Objekt mit exakt `action`, `spec`, `store`, `record`,
`report`, `max_parallel_nodes`, `max_total_wall_ms`,
`max_total_payload_bytes`, `max_started_workers`,
`reservation_max_parallel_nodes`, `reservation_max_total_wall_ms`,
`reservation_max_total_payload_bytes` und
`reservation_max_started_workers`; optional sind nur `cancel_after_ms` und
`remote_restore`. Die vier `reservation_max_*`-Werte müssen den jeweiligen
V6-Admissionwerten exakt entsprechen. Ungültige Struktur, doppelte Felder,
unbekannte Felder, nichtkanonische Darstellung und unzulässige Bereiche
führen zu der geschlossenen Antwort `INVALID_COMMAND`, bevor übergebene
Dateipfade geöffnet werden. Wenn die Queue schon voll ist, liefert ein
`submit` `QUEUE_FULL` ebenfalls vor jedem Plan- oder Dateizugriff.

`admission-status` akzeptiert exakt `action` und `run_id`. Seine erfolgreiche
Antwort trägt statt der normalen `status`-Projektion ein `admission_status`
Objekt mit Schema `admission-diagnostics-v1`, fester Quelle
`runtime-orchestrator-v1`, geschlossenem Zulassungszustand/-grund, Sequenzen
und Reservation. Sie enthält keine native `run_id`, keine Planfelder und
keine V6-/Sidecar-Daten. Eine nicht gefundene, syntaktisch gültige Kennung
ergibt `RUN_NOT_FOUND`; die Abfrage führt keinen Dateizugriff aus.

Die Pfad- und URL-Felder gehören ausschließlich zum kontrollierten
`submit`-Eingabeobjekt; sie werden nie in einer öffentlichen Antwort
projiziert. Erfolgsantworten enthalten den Vertrag `runtime-orchestrator-v1`, `ok`, die
Aktion sowie eine redigierte Statusprojektion. Diese enthält nur `run_id`,
`enqueue_seq`, `submit_seq`, optionale `start_seq`/`finish_seq`, `state`,
`reason`, die vier Reservationwerte, optionale V6-Abschlussfelder und eine
opaque `sidecar_ref`. Fehler sind ausschließlich `INVALID_COMMAND`,
`UNKNOWN_ACTION`, `QUEUE_FULL` oder `RUN_NOT_FOUND`. Öffentliche Antworten
und Fehler enthalten keine lokalen Pfade, URL, PID/PGID, Credentials,
Payload/Frames, Signaturen oder Sidecar-Rohtelemetrie. Die opaque
`sidecar_ref` ist kein Pfad und löst niemals einen Lese-, Reparatur- oder
Replayvorgang aus.

Ein `submit` behält die bestehenden V6-Formate und die bestehende
Recordpublikation vollständig bei. Erfolg eines V6-Runs kann daher weiterhin
allein im kanonischen V2–V6-Record belegt werden; Orchestratorstatus ist nur
flüchtiges Betriebsmetadatum und nach Hostende nicht rekonstruierbar.

## Zahlen

CLI-Ganzzahlen sind dezimal, ohne Leerzeichen, `+`, Tausendertrennzeichen, Hexpräfix oder Exponentialnotation. Negative Zahlen sind nur bei vorzeichenbehafteten Werten zulässig. UInt64 reicht von 0 bis 18446744073709551615, Int64 von −9223372036854775808 bis 9223372036854775807. Adressberechnungen können schon vor der Eingabeobergrenze überlaufen.

`bigint` akzeptiert dezimale ganze Zahlen. `rational` zusätzlich `Zähler/Nenner`, zum Beispiel `-3/4` oder `6/-8`. Nenner 0 ist ungültig. Brüche werden gekürzt, das Vorzeichen wird normalisiert; `2/2` wird als `1` ausgegeben. Dezimalzahlen wie `0.5` sind kein unterstütztes Eingabeformat: `1/2` verwenden. Die Bitgrenze gilt auch für die zunächst eingelesenen Komponenten, nicht erst nach dem Kürzen. Übermäßig lange Texte werden vor großen GMP-Allokationen abgewiesen.

Große und exakte Werte erscheinen überwiegend als JSON-Strings. Int64-Forschung gibt beispielsweise ihre Koeffizienten dagegen als JSON-Zahlen aus. Verbraucher dürfen deshalb weder pauschal Float-Konvertierung noch einen einheitlichen Typ für alle numerischen Felder voraussetzen. [Beispielausgaben](examples.json) zeigen die tatsächlichen Typen.

## CSV-Forschungspaare

```csv
x,y
0,3
1,5
4,11
```

`x` ist Eingabe, `y` erwartete Ausgabe. Erste Zeile exakt `x,y`, danach genau zwei numerische Felder pro Zeile. Keine Anführungszeichen, Kommentare, Leerzeilen, Spaltennamenübersetzungen oder BOM. LF und CRLF sind unterstützt; ein abschließender Zeilenumbruch ist zulässig. Eine zusätzliche leere Datenzeile ist ungültig.

Eine angegebene Datei muss 1..4096 Paare enthalten. Holdout weglassen, wenn keiner existiert; kein leeres Holdout-CSV angeben. Jede Datei ist höchstens 1048576 Bytes groß. Exakte Forschung prüft zusätzlich die zusammengezählte normalisierte Sampletextgröße gegen 1048576 Bytes. Alle X-Werte müssen sowohl innerhalb der Dateien als auch über Training und Holdout hinweg eindeutig sein. Exakte Brüche werden vor dem Vergleich normalisiert: `1/2` und `2/4` sind derselbe X-Wert.

Rationales Beispiel:

```csv
x,y
0,1/2
1/2,3/4
1,1
```

Training bestimmt Kandidaten; Holdout prüft sie an zusätzlichen Inputs. Die fachliche Unabhängigkeit der Daten kann BRUCE nicht allein durch disjunkte X-Werte garantieren.

## Exact-Affine-Evidenzkette

`bruce evidence create` speichert ein eigenes, SHA-256-gebundenes `BRUCE_EVIDENCE_CHAIN_V1`-Bundle. Header und feste Felder stehen in genau dieser Reihenfolge; `N` ist jeweils eine dezimale Byteanzahl, keine Zeilenzahl:

```text
BRUCE_EVIDENCE_CHAIN_V1
<SHA-256 des vollständigen Bodys>
contract affine-evidence-chain-v1
independence declared-disjoint-v1
countercheck_policy affine-inverse-v1
countercheck_scope short-circuit-training-then-holdout-v1
training_csv_bytes <N>
<genau N ursprüngliche Trainings-CSV-Bytes>
holdout_csv_bytes <N>
<genau N ursprüngliche Holdout-CSV-Bytes>
native_bundle_bytes <N>
<genau N Bytes eines BRUCE_EXACT_BUNDLE_V1>
RESULT
<kanonisches Ergebnis-JSON>
```

Die beiden CSV-Blöcke enthalten die ursprünglichen Bytes einschließlich ihrer Zeilenenden, nicht normalisierte oder neu formatierte CSVs. Beide Eingabedateien sind auf je 1048576 Bytes begrenzt; die Chain unterliegt zusätzlich der allgemeinen Bundlegrenze von 33554432 Bytes. Der eingebettete Native-Task muss die Exact-Route `affine`/`enumerate` in `bigint` oder `rational` und genau die aus den CSVs geparsten Trainings-/Holdout-Samples enthalten. Die x-Werte müssen auch dateiübergreifend eindeutig sein.

`declared-disjoint-v1` bedeutet eine lokale Deklaration zusammen mit technisch geprüfter x-Disjunktheit. Es belegt weder externe Datenherkunft noch fachliche oder statistische Unabhängigkeit. `countercheck_scope short-circuit-training-then-holdout-v1` beschreibt den tatsächlichen kurzschließenden Kontrollfluss, keine Vollabdeckung aller Kandidaten oder Samples. Jeder erreichte Kandidat wird zuerst an Training geprüft; die erste Kontradiktion beendet seine Prüfung. Die inverse Gegenprüfung läuft nur nach einer passenden Vorwärtsauswertung. Holdout beginnt nur nach bestandener Trainingsprüfung und wenn Resultat-/Budgetfluss es erreicht. Budgets können innerhalb eines Kandidaten stoppen; `INCOMPLETE` ist kein Abdeckungsnachweis. Status, Stopgrund, Zähler und Trace dokumentieren den tatsächlichen Verlauf; wegen Budgets nicht enumerierte Kandidaten wurden nicht geprüft. `bruce evidence replay` liest nur diese lokale Chain, prüft Header, Digest, feste Felder, Längen, CSV-/Native-Bindung sowie das rekonstruierte Ergebnis und gibt kanonisches JSON zurück. Digest und Replay sind Integritäts-/Reproduktionskontrollen, kein allgemeiner mathematischer Beweis und keine externe Prüfung. Die Chain ist kein `ApplicationTask`-Bundle für `bruce replay`, `inspect-run`, `assess-run` oder `gate-run`.

## Sprachentwurf und Sprachlauf

`language interpret` akzeptiert genau zwei externe CSV-Pfade, `--train` und `--holdout`. Jede Datei wird vor der Interpretation mit der bestehenden Grenze von 1048576 Bytes gelesen und als Originalbytes samt SHA-256-Digest im Entwurf gebunden. Für den Satz `Untersuche @train mit @holdout auf rationale Formeln.` heißen die internen Aliase `train` und `holdout`. Für `Untersuche @left und @right auf rationale Formeln.` werden dieselben beiden Dateipfade in dieser Reihenfolge als `left` und `right` gebunden. Andere Aliasse oder aus Text abgeleitete Dateipfade gibt es nicht. Die Mini-Grammatik normalisiert ASCII-Großschreibung und ASCII-Whitespace; andere Formulierungen sind nicht unterstützt. Eingabetext muss gültiges UTF-8 sein.

Der erste Satz ergibt `UNDERSTOOD` mit genau einer kanonischen Alternative `rational-portfolio-v1`. Der zweite ergibt `AMBIGUOUS` mit `train=left;holdout=right` und `train=right;holdout=left`. Erst `language select` trägt eine der beiden IDs in einen neuen Entwurf ein; ohne diese Wahl verweigert `execute` den Lauf. Nicht unterstützter Text wird ohne Entwurfsexport abgewiesen. Die JSON-Statuslabels sind großgeschrieben; die internen Statusfelder im Dateiformat sind eigene kanonische Werte.

`BRUCE_LANGUAGE_DRAFT_V1` ist ein SHA-256-gebundenes, längenpräfixiertes kanonisches Byteformat. Es enthält Grammatikversion, Originaltext, normalisierten Text, beide ursprünglichen CSV-Bytes und Digests, Anfragegrenzen, Status, Alternativen und gegebenenfalls die ausgewählte ID. `select` und `execute` nehmen nur dieses Format an; Text- oder JSON-Dateien gelten nicht als Entwurf. `BRUCE_LANGUAGE_RUN_V1` bindet den vollständigen Entwurf, Rollen-Digests, ein natives Core-Bundle mit Digest, die Sprachphasen, Erklärung und die gerenderte Core-Evidenz. Neue `execute`-Exporte schreiben darüber `BRUCE_MOD_RUN_V1`: ein SHA-256-umschlossenes längenpräfixiertes Format mit Manifest `language`/`language-mod-v1`/`bruce-mod-api-v1`, beiden Codeckennungen, Entwurf und innerem V1-Lauf samt einzelnen Inhaltsdigests. `language replay` prüft Manifest, Codecs, Digests, Entwurf-zu-Lauf-Bindung und das native Core-Replay. Direkte historische `BRUCE_LANGUAGE_RUN_V1`-Dateien bleiben ein read-only Kompatibilitätsweg. Es schreibt keine neue Datei; die gespeicherten Laufbytes bleiben unverändert.

Die Formatdecoder begrenzen ihren Payload auf 128 MiB zuzüglich Header/Digest. Die CLI verwendet den bestehenden begrenzten Leser und den exklusiven Export mit einer engeren 32-MiB-Dateigrenze; der eingebettete native Core-Bundlepfad ist ebenfalls auf 32 MiB begrenzt. Das ist eine Bytegrenze, keine allgemeine Prozessspeicher- oder Laufzeitgarantie. Die direkte Ausgabe `language-execution-v1` beziehungsweise `language-replay-v1` umschließt unter `result` den `language-run-v1`-Befund; darin bleibt `core` die geprüfte native Sitzung. Ein natives `INCOMPLETE` bleibt `INCOMPLETE` und wird nicht als Beweis oder Gate-Freigabe umgedeutet.

## NanoCell-Kontext und Bundle

NanoCell-`--core` ist ausnahmsweise **hexadezimal**: genau 16 Ziffern für acht Bytes, ohne `0x`, Zwischenräume oder Trennzeichen. Beispiel: `1020a54050607080` mit `--active-index 2` wählt `a5`. Andere CLI-Ganzzahlen bleiben dezimal. `--context-file` wird als rohe Bytefolge mit höchstens 4096 Bytes gelesen, nicht als Pfadbezeichnung oder JSON; ein abschließender Zeilenumbruch verändert den SHA-256-Digest. Für `masked-match-v1` und `rotate-match-v1` lautet der Referenzinhalt exakt `nanocell-reference-context-v1`, für `context-conflict-v1` exakt `nanocell-reference-context-conflict-v1`.

Mit `--output` entsteht ausschließlich ein exklusiv veröffentlichter `BRUCE_MOD_RUN_V1`-Umschlag. Er enthält das Manifest `nanocell`/`nanocell-mod-v1`/`bruce-mod-api-v1`, die kanonische NanoCell-Anfrage mit Digest sowie den unveränderten inneren `BRUCE_NANOCELL_RUN_V1` samt Digest. Die Endung ist frei wählbar (`.nanocell` ist lesbar); die Datei ist weder JSON noch ein allgemeines `.bruce`-Anwendungsbundle. `bruce nanocell replay DATEI` akzeptiert neue äußere Umschläge bis 12 KiB und prüft Manifest, Codecs, Digests, kanonische Anfrage-zu-Run-Bindung und erneute Ausführung. Direkte historische `BRUCE_NANOCELL_RUN_V1`-Dateien bleiben read-only kompatibel und behalten ihre 5703-Byte-Grenze. Der NanoCell-Pfad gehört nicht zu `bruce replay`, `inspect-run`, `assess-run` oder `gate-run`. Innerer und äußerer Lauf enthalten Kontextbytes; nicht mit öffentlichen Daten teilen, wenn der Kontext vertraulich ist.

## DAG-Scheduler V5 Run-Baum und Metadatenbericht

**IMPLEMENTIERT UND BELEGT:** Der experimentelle V5-Aufruf lautet:

```text
bruce dag-scheduler-v5 execute SPEC --output BUNDLE
  [--retry N] [--max-wall-ms N] [--max-input-bytes N]
  [--max-output-bytes N] [--max-operations N]
  [--max-memory-bytes N] [--workers N]
```

`SPEC` ist bytegenau die [V3-Spec-Datei](#dag-scheduler-v3-spec-datei).
V5 speichert die ursprünglichen Requestbytes im privaten Run-Baum, aber nicht
im öffentlichen Bericht. Für ein Bundle `BUNDLE` entsteht während der
Ausführung dieser lokale, besitzerbeschränkte Baum:

```text
BUNDLE.run/
├── inputs/<node-id>.bin
├── attempts/<attempt-id>/output/result.bin
└── control/run-log.jsonl
```

Das V3-Feld `max-started-nodes` bleibt auch im V5-Lauf maßgeblich. Es
reserviert die ersten Positionen der kanonischen geplanten Reihenfolge;
Retries verbrauchen eine bereits reservierte Position nicht erneut. Wird eine
Position wegen erschöpftem Startbudget abgewiesen, werden ihre abhängigen
Positionen `BLOCKED`; ein späterer unabhängiger Knoten übernimmt diesen Platz
nicht.

`inputs/` enthält die vom Parent freigegebenen Requestdateien. Jeder Versuch
schreibt sein Payload-Artefakt ausschließlich in seinen eigenen
`attempts/<attempt-id>/output/result.bin`-Bereich. Der Child sendet im
Result-Frame nur Status, Resultcode, Digest, Größe, Nutzung und die opake
Referenz `result`; Payloadbytes überschreiten den Result-Frame nicht. Bruce
prüft beim Empfang die relative Referenz, Dateigröße und SHA-256 erneut. Die
öffentliche Report-Referenz ist davon getrennt und lautet bei akzeptierten
Knoten beispielsweise `artifact/a`.

### V5-Report

Der veröffentlichte Inhalt von `BUNDLE` und die stdout-Ausgabe sind derselbe
JSON-Metadatenbericht unter `dag-scheduler-v5-cli-v1`. Bei mehreren Knoten
stehen die Receipts in der kanonischen Spec-Reihenfolge; pro Receipt werden
`node_id`, `callback_id`, `request_digest`, `attempt_id`, Status, Fehlercode,
Resultcode, öffentliche Referenz oder `null`, Payload-Digest, Payloadgröße,
Nutzung und die vollständige `attempts`-Historie ausgegeben. Die Nutzung
enthält `wall_ms`, `input_bytes`, `output_bytes`, `operations` und
`peak_memory_bytes`. Bei einem terminalen Vorgänger werden abhängige Knoten
als `BLOCKED` berichtet; unabhängige Knoten dürfen ihren Lauf beenden.

Die fünf Ressourcenbudgets werden je Versuch im Bericht wiederholt:

| Feld | Standard | Maximum |
| --- | ---: | ---: |
| `max_wall_ms` | `30000` | `600000` |
| `max_input_bytes` | `65536` | `1048576` |
| `max_output_bytes` | `65536` | `5242880` |
| `max_operations` | `1000000` | `10000000` |
| `max_memory_bytes` | `268435456` | `2147483648` |

`retry` hat Standard `0` und Maximum `3`; es zählt zusätzliche Versuche und
wird nur durch die ausdrückliche Option `--retry N` aktiviert. `workers` hat
Standard `1` und Maximum `4`. Die feste Supervisor-Grace beträgt `1000 ms`
und ist kein gespeichertes oder änderbares CLI-Budget. Der öffentliche
Host-Abbruch ist der separate Aufruf `bruce dag-scheduler-v5 cancel BUNDLE`.
Er setzt den privaten Marker `BUNDLE.run/control/cancel.request` mit den
exakten Bytes `BRUCE_WORKER_CANCEL_V1\n`, atomar, besitzerbeschränkt und
symlinkgeschützt. Der Linux-Supervisor bleibt die einzige Durchsetzungsgrenze:
Bruce fordert für alle aktiven Handles `request_cancel` an und verwendet die
feste Grace, Eskalation und Prozessgruppenbereinigung unverändert.

### JSONL-Run-Log

`control/run-log.jsonl` ist eine von Bruce geschriebene, append-only
Übergabespur. Jede Zeile ist ein kanonisches JSON-Objekt mit Schema, Run-ID,
Sequenznummer, Zeitstempel, Ereignis und Status. Artefaktereignisse ergänzen
nur Digest, Bytegröße, Nutzung und eine kontrollierte relative
Attempt-Referenz. Die V5-Läufe verwenden, abhängig vom Ergebnis, die
Ereignisse `RUN_CREATED`, `ATTEMPT_STARTED`, `STATUS`,
`CANCEL_REQUESTED`, `PROCESS_EXITED`, `RESULT_RECEIVED`,
`RESULT_REJECTED`, `ARTIFACT_ACCEPTED` und `RUN_FINISHED`. Nach beobachtetem
Cancel werden anschließend projizierte `PROCESS_EXITED` und `RESULT_RECEIVED`
mit `CANCELLED` geschrieben. Ein schon projiziertes, noch nicht angenommenes
oder abgelehntes Ergebnis darf durch genau ein nachfolgendes
`CANCEL_REQUESTED/CANCELLED` abgeschlossen werden; es werden keine zweiten
Ergebnisereignisse erzeugt. Die Artefaktmetadaten werden in der effektiven
Inspector-Projektion entfernt. Ein solcher Attempt schreibt kein
`ARTIFACT_ACCEPTED`.

`RESULT_REJECTED` erlaubt ausschließlich `OUTPUT_LIMIT_EXCEEDED`, `TIMEOUT`,
`ARTIFACT_REFERENCE_INVALID`, `ARTIFACT_MANIFEST_INVALID`,
`ARTIFACT_DIGEST_MISMATCH` und `ARTIFACT_STORE_VIOLATION`, nachdem ein
kanonisches `RESULT_RECEIVED/READY` mit Artefakt vorliegt und bevor eine
Annahme, ein Post-Result-Cancel oder eine Ablehnung entschieden wurde. Es
enthält keine Artefakt-/Usage-Felder. Der effektive Inspectorstatus ist FAILED;
Artefaktmetadaten entfallen, die gemessene Usage bleibt erhalten. TIMEOUT lässt
eine explizite Retryentscheidung oder einmaligen Cancel zu; andere Gründe
schließen die Attempt-Serie terminal. Ein Post-Result-Cancel schließt auch die
TIMEOUT-Serie und verhindert einen späteren Neustart. Doppelte Entscheidungen
oder spätere Annahme sind Protokollfehler. Ohne gestarteten Attempt ist nur
FAILED/CANCELLED als usage-freier `RUN_FINISHED` gültig; vorhandene
Worker-Läufe dürfen gemessene Abschluss-Usage behalten. Ein Worker darf
diese Datei nicht schreiben. Unvollständige Zeilen, unbekannte Ereignisse,
nicht monotone Sequenzen und Digest-/Größenwidersprüche werden als
Protokollfehler behandelt.

Der eingebaute Child-Dienst verarbeitet `STATUS_QUERY` und `CANCEL` vor und
nach dem synchronen festen Adapteraufruf. Während dieses Adapteraufrufs wird
der Kontrollkanal nicht zwischen einzelnen fachlichen Operationen bedient;
Timeout und harte Prozessgruppenbeendigung bleiben eine Supervisor-Aufgabe.
Der öffentliche Scheduler beobachtet den Host-Marker vor dem Dispatch und
während der Live-Überwachung. Nach der Beobachtung startet er keine neuen
Attempts und erzeugt keine Retries; aktive Attempts werden genau einmal als
`CANCEL_REQUESTED` geloggt und alle übrigen Receipts als `CANCELLED`
abgeschlossen.

Der Log ist eine private Diagnose- und Recovery-Spur, kein dauerhafter
Artifact Store. Dauerhafte content-addressed Speicherung, ein externer
Payloadspeicher und Netzwerktransport sind für diesen V5-Vertrag nicht
implementiert (**DOKUMENTIERT / KONZEPT**).

### V5-Host-Abbruch V1 und Cancel-Report

**IMPLEMENTIERT UND BELEGT:** Die Route akzeptiert ausschließlich
`bruce dag-scheduler-v5 cancel BUNDLE`. Syntaxfehler (unter anderem ein
`.run`-Ziel, Optionen oder zusätzliche Argumente) liefern Exit 2 und den
kompakten `PROTOCOL_ERROR`-Vertrag. Ein fehlender, unsicherer oder beschädigter
Run-Root/Marker liefert Exit 1 und
`{"error":"CANCEL_TARGET_UNAVAILABLE"}`. Der BUNDLE-Report und der private
Run-Log werden bei dieser Route nicht neu geschrieben; nur der owner-only
Marker wird angelegt.

Die einzige erfolgreiche Antwort ist metadata-only und hat feste Feldreihenfolge:

```json
{"contract":"dag-scheduler-v5-cancel-v1","action":"cancel","status":"REQUESTED"}
```

`status` ist `REQUESTED`, wenn der Marker neu angelegt wurde,
`ALREADY_REQUESTED`, wenn er bereits korrekt vorhanden ist, oder
`ALREADY_FINISHED`, wenn der gebundene Lauf bereits `RUN_FINISHED` besitzt.
Eine zweite Anfrage ist damit idempotent und erzeugt keinen zweiten
Kontrollmarker. Der Marker selbst ist kein JSON- oder Worker-Frame:

```text
BUNDLE.run/control/cancel.request
BRUCE_WORKER_CANCEL_V1\n
```

Wird der Marker im laufenden Lauf beobachtet, friert Bruce den Dispatch ein,
fordert für jeden aktiven Attempt in derselben Scheduler-Iteration
`request_cancel` an und schreibt genau ein `CANCEL_REQUESTED` pro Attempt.
Anschließend projizierte `PROCESS_EXITED` und `RESULT_RECEIVED` tragen
`CANCELLED`; für schon projizierte Ergebnisse gilt die Post-Result-Entscheidung
oben. Ein abgebrochener Attempt erhält keine `ARTIFACT_ACCEPTED`-Zeile.
Nicht gestartete Knoten haben `attempt_id:null`, keine Artefakt-/Payloadreferenz
und Status `CANCELLED`. Schon gestartete Knoten behalten ihre Attempt-ID,
Nutzung und Historie, auch zwischen zwei Retries. `RUN_FINISHED` und der Top-Level-Report tragen `CANCELLED`, der
Execute-Aufruf endet mit Exit 1. `inspect BUNDLE` meldet einen vollständigen
solchen Lauf als `VALID` mit `run_status:"CANCELLED"`; `publish` lehnt ihn ab.

Die Beobachtung definiert die Race-Ordnung: Vor der Markerbeobachtung
akzeptierte Receipts bleiben akzeptiert; ein noch nicht übernommenes Ergebnis
wird nach beobachteter Anfrage als `CANCELLED` übernommen. Kein öffentlicher
Status enthält PIDs, private Pfade, Kommandozeilen, Payloadbytes oder rohe
Betriebssystemfehler.

### V5-Inspect-Bericht

**IMPLEMENTIERT UND BELEGT:** `bruce dag-scheduler-v5 inspect BUNDLE` liest
die veröffentlichte Execute-Datei und ausschließlich den gebundenen Seitensatz
`BUNDLE.run/`. Der BUNDLE-Inhalt muss ein kanonischer
`dag-scheduler-v5-cli-v1`-Execute-Bericht sein; sein `action` muss `execute`
bleiben. Die private JSONL-Datei wird über `control/run-log.jsonl` gelesen.
Ein anderer Run-Root, eine kopierte BUNDLE-Datei ohne passenden Seitensatz,
Symlinks, Root-Escapes, Mehrfach-Hardlinks oder unsichere Dateirechte sind
keine gültige alternative Eingabe.

Der öffentliche Bericht ist ein kanonisches JSON-Objekt mit finalem LF:

```json
{"contract":"dag-scheduler-v5-inspect-v1","action":"inspect",
 "status":"VALID","run_finished":true,"run_status":"READY",
 "run_id":"run-id","last_seq":8,
 "attempts":[{"attempt_id":"a-1","statuses":["STARTED","READY"],
 "accepted":true,"event_count":5,"digest":"<sha256>",
 "size_bytes":12,"usage":{"wall_ms":1,"input_bytes":12,
 "output_bytes":12,"operations":1,"peak_memory_bytes":0},
 "artifact_verified":true}]}
```

Die Platzhalter im Beispiel stehen für Werte aus dem geprüften Lauf; Payload-
und Requestbytes werden nicht ausgegeben. `attempts` wird nach der ersten
beobachteten Attempt-Startsequenz geordnet. Pro Attempt sind nur
`attempt_id`, `statuses`, `accepted`, `event_count`, Digest, `size_bytes`,
Usage und `artifact_verified` erlaubt. Usage bleibt auf
`wall_ms`, `input_bytes`, `output_bytes`, `operations` und
`peak_memory_bytes` begrenzt.

Für die Einstufung gelten diese Ergebnisse:

| Zustand | stdout | stderr | Exit |
| --- | --- | --- | ---: |
| vollständiger Lauf mit `RUN_FINISHED` | Inspect-Bericht, `status=VALID` | leer | 0 |
| strukturell gültiger Lauf ohne `RUN_FINISHED` | Inspect-Bericht, `status=INCOMPLETE` | leer | 1 |
| beschädigte, abgeschnittene oder unsichere Daten | leer | `{"error":"PROTOCOL_ERROR"}` plus LF | 2 |
| falsche CLI-Syntax | leer | bestehende V5-Usage-Ausgabe | 2 |

Der Inspector verlangt eine abschließende LF je JSONL-Zeile, monotone
Sequenznummern, die einmalige `RUN_CREATED`-Initialisierung, ein abschließendes
`RUN_FINISHED` am Dateiende für `VALID` sowie gültige per-Attempt-Zustände.
`RESULT_RECEIVED` und `ARTIFACT_ACCEPTED` müssen dieselbe kontrollierte
Referenz, Größe und SHA-256-Prüfsumme binden. Eine abschließende
typisierte `RESULT_REJECTED`- oder Post-Result-Cancelentscheidung
verhindert die Annahme und entfernt die effektiven Artefaktmetadaten, ohne die
ursprüngliche Ergebniszeile zu verändern; Referenzkomponenten werden
descriptor-relativ mit `O_NOFOLLOW` geprüft. Ein fehlendes `RUN_FINISHED` ist
nur dann `INCOMPLETE`, wenn der bisherige Verlauf vollständig und gültig ist.
Teilobjekte, fehlende LF, unbekannte Ereignisse, nichtmonotone Sequenzen,
Digest-/Größenabweichungen, Symlinks und zusätzliche Hardlinks ergeben
`PROTOCOL_ERROR`.

Der BUNDLE-Parser akzeptiert nur die festgelegte Feldreihenfolge und meldet
unbekannte, doppelte, zusätzliche oder manipulierte Werte als
`PROTOCOL_ERROR`. JSON-Verschachtelung ist auf 64 Ebenen begrenzt; Retry,
Workerzahl, Budgets, Usage, Status-/Resultcodes und Attempt-Historie werden
gegen den beobachteten V5-Lauf und die festen V5-Grenzen geprüft. `inspect`
setzt keinen Lauf fort, führt keinen Replay aus, repariert keine Dateien und
lädt keine Payload. Der Bericht ist eine metadata-only Diagnose- und
Integritätsansicht, kein Artifact Store und kein allgemeiner Recovery-
Executor.

Belege: [Inspector-Modell](../../include/bruce/worker/worker_run_inspector_v1.hpp),
[Run-Inspector](../../src/worker/worker_run_inspector_v1.cpp),
[V5-CLI](../../apps/dag_scheduler_v5_cli.hpp),
[Unit-Verträge](../../tests/unit/worker_run_inspector_v1_tests.cpp),
[V5-Inspector-Workflow](../../tests/integration/test_dag_scheduler_v5_inspector.py).

### V5-Execute-Handoff-Lineage V1

**IMPLEMENTIERT UND BELEGT:** `execute SPEC --output BUNDLE --handoff-v1`
schreibt zusätzlich `BUNDLE.handoff-v1.json`; `inspect BUNDLE --handoff-v1`
prüft diese Datei mit `BUNDLE` und `BUNDLE.run/`. Das Sidecar besteht aus genau
einem kanonischen JSON-Objekt in einer Zeile mit abschließendem LF und dem
Vertrag `bruce-execute-handoff-v1`. Schlüssel und Arrayinhalte folgen dieser
festen Reihenfolge:

```text
contract="bruce-execute-handoff-v1"
run_id
spec_digest
report_digest
nodes[]: node_id, callback_id, request_digest, dependencies[]
attempts[]: consumer_node_id, consumer_attempt_id, inputs[]
inputs[]: input_index, producer_node_id, producer_attempt_id,
          reference, sha256, size_bytes
```

`spec_digest` bindet die ursprünglichen Specbytes, `report_digest` die exakten
Legacy-BUNDLE-Bytes. `nodes[]` folgt der geprüften Spec-Reihenfolge und
bewahrt die Reihenfolge der direkten Abhängigkeiten. `attempts[]` folgt den
dauerhaft geloggten `ATTEMPT_STARTED`-Ereignissen; jeder gestartete Versuch,
einschließlich fehlgeschlagenem Retry, erhält genau einen unveränderlichen
Eintrag. Eine Preflight-ID ohne Workerstart erhält keinen. `input_index`
zählt die direkten Abhängigkeiten ab 0 in Spec-Reihenfolge. Das Request selbst
gehört nicht zu `inputs[]`, und ein Root-Versuch hat `inputs:[]`. Jeder Input
bindet den akzeptierten Producer-Versuch, seine kontrollierte Referenz, den
SHA-256-Digest und die exakte Bytegröße. Nach beobachtetem Host-Abbruch wird
kein neuer Attempt gestartet oder erfasst.

Der read-only Prüfer gleicht Sidecar, kanonischen Execute-Bericht,
`control/run-log.jsonl`, Requests und referenzierte private Eingabe- und
Producerdateien ab. Seine öffentliche Ausgabe ist eine metadata-only
JSON-Zeile mit finalem LF, exakt in dieser Feldreihenfolge:

```json
{"contract":"dag-scheduler-v5-handoff-inspect-v1","action":"inspect","status":"VALID","run_status":"READY","run_id":"run-id","node_count":2,"attempt_count":2,"binding_count":1,"verified_input_bytes":1,"lineage_scope":"LOCAL_CONSISTENCY"}
```

Die Zähler und Bytezahl sind Werte des geprüften Laufs. `VALID`/Exit 0
bedeutet lokale Konsistenz eines vollständig geloggten Laufs; `run_status`
kann auch `FAILED`, `BLOCKED` oder `CANCELLED` sein. Nur ein strukturell
konsistenter Verlauf ohne `RUN_FINISHED` ist `INCOMPLETE`/Exit 1 mit
`run_status=INCOMPLETE`. Syntax-, Schema-, Digest-, Größen-, Identitäts-,
Rechte- und Mutationsfehler liefern leeres stdout,
`{"error":"PROTOCOL_ERROR"}` plus LF auf stderr und Exit 2. Ein
Sidecar-Encode-, Schreib- oder Adoptionsfehler beim Execute liefert statt des
Berichts leeres stdout, `{"error":"HANDOFF_RECORD_INVALID"}` plus LF auf
stderr und Exit 2; der bereits erzeugte Legacy-Bestand bleibt zur Diagnose.

Die Sidecardatei ist owner-only `0600`, private Verzeichnisse `0700`. Die
begrenzten Reads und Writes verwenden descriptor-relative
`O_NOFOLLOW|O_CLOEXEC`-Zugriffe, Einzellink-/Eigentümerprüfung und
Identitätsabgleich. Das ist kein unveränderlicher Snapshot und keine
Authentizitäts- oder Herkunftsprüfung. Der Prüfer startet keinen Worker,
repariert, publiziert, replayt oder schreibt nichts; ein Lesezugriff kann
systembedingt `atime` ändern. Das WorkerRequest-Frame bleibt unverändert.
Der separate `payload`-Pfad verwendet
[Payload Flow V2](#dag-scheduler-v5-payload-flow-v2). Freie Sprache,
allgemeines Beweisen, GPU-/Netzwerkausführung, freie Plugins, externe
Stores, Recovery/Resume und breite Runtime-Orchestrierung bleiben
**DOKUMENTIERT / KONZEPT**.

Belege: [Codec-Verträge](../../tests/unit/execute_handoff_lineage_v1_tests.cpp),
[Inspector-Verträge](../../tests/unit/execute_handoff_inspector_v1_tests.cpp)
und [reale Workflows](../../tests/integration/test_dag_scheduler_v5_handoff.py).

### V5-Artifact-Publish-Bericht und Manifestbindung

**IMPLEMENTIERT UND BELEGT:** Die lokale Importbrücke ist ein eigener,
read-only geprüfter CLI-Schritt:

```text
bruce dag-scheduler-v5 publish BUNDLE --store STORE
```

Vor jedem Store-Zugriff prüft die Route den kanonischen Execute-Bericht und
`BUNDLE.run/` über den V5-Inspector. Publish akzeptiert nur einen vollständig
beendeten `RUN_FINISHED`-Lauf mit `VALID`/`READY` und einem verifizierten
`ARTIFACT_ACCEPTED` für jeden Knoten. Die erwartete Knotenmenge und ihre
Reihenfolge werden aus den privaten `inputs/<node-id>.bin`-Einträgen gebunden;
die Ausgabe bleibt von diesen privaten Pfaden getrennt.

Der Bericht verwendet eine feste JSON-Feldreihenfolge und ein abschließendes
LF:

```json
{"contract":"dag-scheduler-v5-publish-v1","action":"publish","status":"PUBLISHED","run_id":"run-id","manifests":[{"node_id":"a","attempt_id":"a-1","reference":"artifact/a","manifest_id":"v5-<sha256>","sha256":"<64-hex>","size_bytes":12}]}
```

Die äußeren Felder sind exakt `contract`, `action`, `status`, `run_id`,
`manifests`. Jeder Eintrag besitzt exakt und in dieser Reihenfolge
`node_id`, `attempt_id`, `reference`, `manifest_id`, `sha256`, `size_bytes`.
`reference` bleibt die kontrollierte öffentliche V5-Referenz
`artifact/<node>`. Der Bericht enthält keine Payloadbytes, keine absoluten
oder privaten Pfade, keine `attempts/...`-Referenzen und keine
Betriebssystemfehler.

Für jeden Eintrag importiert `put_file` die an Digest und Größe gebundene
Ergebnisdatei und veröffentlicht danach genau ein kanonisches
`BRUCE_ARTIFACT_MANIFEST_V1`:

```text
run_id=<run-id>
node_id=<node-id>
attempt_id=<attempt-id>
status=READY
result_code=<adapter-result>
object_0_sha256=<64-hex>
object_0_size_bytes=<size>
object_0_origin=artifact/<node>
```

Die vollständige Manifestreihenfolge enthält zusätzlich den festen Header,
`object_count=1` und `object_0_media_type=-`. `manifest_id` ist
`v5-` plus SHA-256 über die kanonischen Bindungszeilen
`BRUCE_DAG_SCHEDULER_V5_PUBLISH_V1`, `run_id`, `node_id` und `attempt_id`.
Die Wiederholung desselben Laufs ist idempotent. Ein bereits vorhandenes,
abweichendes oder beschädigtes Objekt/Manifest wird nicht überschrieben.

| Zustand | stdout | stderr | Exit |
| --- | --- | --- | ---: |
| `RUN_FINISHED`, `VALID`, `READY`, alle Knoten akzeptiert | Publish-Bericht | leer | 0 |
| unvollständiger, fehlgeschlagener, blockierter oder abgebrochener Lauf | leer | `{"error":"PROTOCOL_ERROR"}` plus LF | 2 |
| Manipulation, fehlende Datei, Symlink, Hardlink, Root-Escape, falsche Rechte oder Konflikt | leer | `{"error":"PROTOCOL_ERROR"}` plus LF | 2 |
| falsche Syntax, `.run` als BUNDLE oder zusätzliche Option | leer | `{"error":"PROTOCOL_ERROR"}` plus LF | 2 |

Die Vorprüfung ist vollständig, aber die anschließenden mehreren Store-
Schreibvorgänge sind nicht transaktional. Ein späterer Fehler kann deshalb
bereits immutable publizierte, noch nicht in einem Manifest referenzierte
Objekte zurücklassen. V1.1 löscht nichts und bietet kein Rollback und keine
Garbage-Collection. Die Route ändert weder Execute-Bericht noch Run-Log,
startet keinen Worker, ruft keinen Adapter auf und lädt keine Payload zurück.

Belege: [V5-Publish-CLI](../../apps/dag_scheduler_v5_cli.hpp),
[C++-Verträge](../../tests/unit/dag_scheduler_v5_cli_tests.cpp),
[Workflows](../../tests/integration/test_dag_scheduler_v5_publish.py),
[Manifestcodec](../../include/bruce/artifact/artifact_manifest_v1.hpp) und
[Design-Spezifikation](../../docs/superpowers/specs/2026-10-15-worker-artifact-publish-v1-design.md).

## DAG-Scheduler V5 Payload Flow V1.3

**IMPLEMENTIERT UND BELEGT:** Der V1.3-Payloadmodus hat eine eigene
metadata-only Route:

```text
bruce dag-scheduler-v5 payload SPEC --output BUNDLE --store STORE
  [--retry N] [--max-wall-ms N] [--max-input-bytes N]
  [--max-output-bytes N] [--max-operations N]
  [--max-memory-bytes N] [--workers N]
```

Der lokale V1.3-/V1.4-Pfad nutzt seit Stage 3D den gemeinsamen Batch-Host,
einschließlich lokaler externer Eingaben und expliziter Retries. Die bestehenden
öffentlichen Berichtsfelder und Worker-V1-Frames bleiben unverändert; nur der
private Lifecycle verwendet die oben beschriebenen Host-Entscheidungen.
Vorbereitungsfehler können ohne Worker-Attempt FAILED enden. Die absolute
Attempt-Deadline umfasst Vorbereitung bis Annahme mit Prüfungen vor/nach
synchroner Store-Veröffentlichung, jedoch ohne harte Unterbrechung einzelner
Dateisystemaufrufe. Publizierte, anschließend abgelehnte oder abgebrochene
Store-Einträge bleiben immutable; erst Host-Annahme macht ihre Referenzen
zu akzeptierten Ergebnissen. Stream V1.5, Flow V2 und Handoff V1 behalten ihre
eigenen Pfade. Beleg:
[Stage-3D-Workflows](../../tests/integration/test_common_runtime_host_v2_stage3d.py).

Der Payloadbericht beginnt bei einem Mehrknotenlauf mit der festen Folge
`contract`, `action`, `spec_digest`, `dag_id`, `retry`, `workers`, `budget`,
`status`, `receipts`. `contract` ist
`dag-scheduler-v5-payload-v13`, `action` ist `payload`; `status` ist
`READY` oder `FAILED`. Jeder Receipt bindet in dieser Reihenfolge
`node_id`, `callback_id`, `request_digest`, `attempt_id`, `status`,
`failure_code`, `result_code`, `reference`, `payload_digest`, `payload_size`,
`usage` und `attempts`. Bei einem Einzelnknotenlauf wird der bestehende
Einzelbericht mit `node_id`, `adapter_id` und denselben Usage-/Attempt-Feldern
verwendet. Die öffentliche `reference` ist nur eine opake Bruce-Referenz wie
`artifact/a` oder `null`.

`ArtifactInputRefV13` ist eine interne, versionierte Metadatenbindung:

| Feld | Format und Bedeutung |
| --- | --- |
| `manifest_id` | opake kanonische Manifest-ID, kein Dateipfad |
| `object_index` | nullbasierter Index des Objekts im Manifest |
| `sha256` | erwarteter SHA-256-Digest als lowercase 64-hex |
| `size_bytes` | exakt erwartete Bytegröße |

Die Referenz wird nur aus einer direkten DAG-Kante erzeugt. Das kanonische
Manifest muss `status=READY` tragen; `run_id`, Vorgänger-`node_id`,
`attempt_id`, Objektindex, Digest und Größe müssen zusammenpassen. Das Objekt
wird vor der Materialisierung vollständig nach den bestehenden Store-Regeln
geprüft. `origin` bleibt Metadatum. Es wird nie als Pfad, URL oder Provider-
Auswahl interpretiert.

Bruce materialisiert die direkte Vorgängerfolge bounded in neue owner-only
Attempt-Dateien. Die Quelle wird descriptor-relativ mit `O_NOFOLLOW` gelesen;
die Zieldatei wird nach dem Stream nochmals auf exakte Größe und Digest geprüft.
Die WorkerRequest enthält danach nur relative private Eingabereferenzen,
Digest, Größe und die bestehenden Workerlimits. Storepfade, Manifest-IDs,
Payloadbytes, PIDs und Betriebssystemdiagnosen bleiben außerhalb des Workers
und des öffentlichen Berichts.

| Budget/Grenze | Standard | Maximum |
| --- | ---: | ---: |
| `max_wall_ms` (Resolve bis Publikation) | `30000` | `600000` |
| `max_input_bytes` (Request plus direkte Objekte) | `65536` | `1048576` |
| `max_output_bytes` | `65536` | `5242880` |
| `max_operations` | `1000000` | `10000000` |
| `max_memory_bytes` | `268435456` | `2147483648` |
| `retry` (zusätzliche Versuche) | `0` | `3` |
| `workers` | `1` | `4` |
| direkte Payloadobjekte je Node | `0` | `15` |

Das V1.3-Limit beträgt 15 direkte Payloadobjekte, weil der unveränderte
Worker-Vertrag höchstens 16 Gesamtinputs akzeptiert und der Request selbst
einen Platz belegt. `max_wall_ms` ist eine End-to-End-Deadline je Attempt;
V1.3 führt kein separates Resolve- oder Host-I/O-Budget ein.

Jeder akzeptierte READY-Output wird vor dem Freischalten direkter Nachfolger
als Artifact-Objekt und kanonisches `READY`-Manifest publiziert. Ein Fehler
bei Referenz, Manifest, Integrität, Store, Berechtigung, Materialisierung oder
Inputlimit erzeugt keinen partiellen READY-Zustand: der Node wird `FAILED`,
abhängige Nodes werden `BLOCKED`, und private Teilinputs werden entfernt.
Nur `PROCESS_CRASHED`, `TIMEOUT` und der bestehende temporäre
`PROTOCOL_ERROR`-Workerfehler sind mit explizitem `--retry N` wiederholbar.
Jeder neue Versuch erhält eine neue `attempt_id`; Artefakt-, Rechte- und
Referenzfehler werden nicht automatisch wiederholt.

Die Route verändert die bestehenden `execute`, `inspect`, `publish`,
Artifact-Store-Resolve- und V2/V3/V4-Dateiformate nicht. Freie externe
Freie oder ungeprüfte Manifestreferenzen außerhalb des kontrollierten
V1.4-Sidecars, Remote-/Netzwerk-/GPU-Ausführung, freie Code-Plugins, freie
Sprache, allgemeines Beweisen und eine übergreifende Runtime-
Orchestrierung sind **DOKUMENTIERT / KONZEPT**.

Belege: [V1.3-CLI](../../apps/dag_scheduler_v5_cli.hpp),
[Payload-Typen](../../include/bruce/artifact/artifact_payload_v1.hpp),
[Store-Materialisierung](../../include/bruce/artifact/artifact_store_v1.hpp),
[Payload-Unit-Tests](../../tests/unit/artifact_payload_v1_tests.cpp),
[Payload-Workflow](../../tests/integration/test_dag_scheduler_v5_payload.py).

## DAG-Scheduler V5 Payload Flow V1.4: externe lokale Eingaben

**IMPLEMENTIERT UND BELEGT:** Die V1.4-Erweiterung aktiviert sich ausschließlich
über die additive Option:

```text
bruce dag-scheduler-v5 payload SPEC --output BUNDLE --store STORE \
  --external-inputs INPUTS
```

Ohne diese Option bleibt der Berichtvertrag
`dag-scheduler-v5-payload-v13` unverändert. Mit ihr lautet der Vertrag
`dag-scheduler-v5-payload-v14`; `action` bleibt `payload`. V1.4 ergänzt nach
`budget` das Laufzeitfeld `external_inputs_digest`. Jeder Receipt ergänzt nach
`usage` `external_input_count` und `external_input_bytes`. Der Digest ist der
SHA-256-Wert der exakt gelesenen Sidecar-Bytes; er enthält keine Payloadbytes.

`INPUTS` muss die kanonische, owner-only JSON-Datei
`bruce-dag-scheduler-v5-external-inputs-v14` sein. Es sind ausschließlich die
folgenden Felder und die folgende Reihenfolge erlaubt:

```json
{"contract":"bruce-dag-scheduler-v5-external-inputs-v14","bindings":[{"node_id":"merge","inputs":[{"manifest_id":"v5-source-001","object_index":0,"sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef","size_bytes":42,"source_run_id":"run-source","source_node_id":"source","source_attempt_id":"attempt-1"}]}]}
```

Eine `binding` enthält `node_id` und eine geordnete `inputs`-Liste. Jede
`input` enthält `manifest_id`, `object_index`, lowercase-64-hex `sha256`,
`size_bytes`, `source_run_id`, `source_node_id` und `source_attempt_id`.
IDs sind ASCII-Identifier ohne Pfadtrenner oder `..`; unbekannte, doppelte
oder ausgelassene Felder, Whitespace und alternative JSON-Escapes sind
ungültig. Sidecar-Datei, Manifest und Objekt werden ausschließlich im
angegebenen lokalen Store und unter Bruce-Kontrolle gelesen. Ein kanonisches
`READY`-Manifest sowie Run-, Node-, Attempt-, Index-, Digest- und Größen-
Provenienz müssen zusammenpassen.

Direkte V1.3-Abhängigkeiten und externe Inputs werden gemeinsam in die
unveränderte Worker-Grenze eingerechnet: höchstens 15 Payloadobjekte je Node,
weil der Request einen der 16 Gesamtinputplätze belegt. Ein einzelnes Sidecar-
Binding darf ebenfalls höchstens 15 Objekte enthalten. Bruce prüft die
Objekte vor dem Worker bounded, materialisiert sie in owner-only private
Attempt-Dateien und entfernt bei einem späteren Fehler alle Teildateien.
Der Worker erhält weder Storepfade noch Manifest-IDs oder Sidecarbytes.

Sidecar-Syntax, Dateirechte, Symlinks, unbekannte Nodes, doppelte Objekte und
Arityfehler werden als Vorprüfungsfehler mit leerem stdout und Exit 2
abgewiesen. Ein fehlendes oder nicht bereites Manifest, Objektintegritäts-,
Store-, Größen-, Inputlimit- oder Timeout-Fehler nach Laufbeginn erzeugt einen
`FAILED`-Receipt, blockiert abhängige Nodes und beendet den Lauf mit Exit 1.
Ein explizites `--retry N` bleibt die einzige Retry-Steuerung.

Belege: [V1.4-CLI](../../apps/dag_scheduler_v5_cli.hpp),
[V1.4-Typen](../../include/bruce/artifact/artifact_payload_external_v1.hpp),
[V1.4-Parser](../../src/artifact/artifact_payload_external_v1.cpp),
[V1.4-Workflow](../../tests/integration/test_dag_scheduler_v5_payload.py)
und [V1.4-Spezifikation](../../docs/superpowers/specs/2026-10-15-artifact-payload-v1-external-inputs-design.md).

## DAG-Scheduler V5 Payload Stream V1.5

**IMPLEMENTIERT UND BELEGT:** nur der explizite Aufruf
`bruce dag-scheduler-v5 payload SPEC --output BUNDLE --store STORE --stream-v15`
wählt die private Streamvorbereitung, optional zusammen mit V1.4
`--external-inputs INPUTS`. Für Streamproduktion ist `--stream-v15` payload-only, wertlos und genau
einmal erlaubt. Ohne Option bleibt das Verhalten unverändert. Es gibt keine
Streamfelder in V1.3-/V1.4-Berichten und keinen neuen öffentlichen Reportvertrag.

Die Bruce-eigenen owner-only Dateien liegen pro Attempt/Input getrennt unter
`BUNDLE.run/payload-inputs/<attempt-id>/input-<index>/stream-payload/`.
`header.jsonl` enthält exakt eine kanonische JSON-Zeile mit abschließendem LF:

```json
{"stream_id":"payload","sha256":"<64-hex>","size_bytes":131073,"chunk_bytes":65536,"chunk_count":3}
```

`chunk-00000000000000000000.bin` und folgende 20-stellige, aufsteigende Indizes
enthalten feste 65.536-Byte-Chunks (65,536 bytes), außer dem letzten Teilchunk.
Leere Payloads haben `chunk_count:0` und keine Chunks. `seal.json` enthält exakt
folgendes kanonisches JSON ohne abschließendes LF:

```json
{"sha256":"<64-hex>","size_bytes":131073,"chunk_bytes":65536,"chunk_count":3}
```

Unbekannte/duplizierte/umgeordnete Felder, alternative Zahlendarstellung,
zusätzliche Einträge, falscher Seal, SHA-256-Digest, exakte Größe oder Chunkanzahl
werden abgewiesen. Verzeichnisse sind owner-only `0700`, Dateien `0600`;
`O_NOFOLLOW`, reguläre Dateien, Eigentümer und einzelne Hardlinks werden geprüft.
Die Übernahme nach vollständiger Prüfung und `fsync` ist exklusiv und atomar.
Byte-Limit und monotone End-to-End-Deadline gelten vor Reads, Writes und Prüfung;
ein laufender Kernel-Dateizugriff kann nicht unterbrochen werden.

Aus den authentifizierten Chunks entsteht bounded eine zusätzlich verifizierte
reguläre Kompatibilitätsdatei `../input.bin`. Nur deren relativer ArtifactRef
wird an WorkerRequest gebunden, weil der unveränderte Supervisor reguläre
Dateien verlangt. Das worker-v1 Frame und die bestehenden Adapterlimits bleiben
unverändert; dies ist keine Zero-Copy-Schnittstelle. Vor Workerstart müssen
Digest und exakte Größe dieser Datei erneut stimmen. Fehler einschließlich der
letzten Deadlineprüfung entfernen den gesamten aktuellen Attempt-Inputbereich,
auch frühere Inputs und partielle Dateien, ohne `ATTEMPT_STARTED` oder
Store-Publikation. Erfolgreiche private Dateien bleiben lokale Laufartefakte;
temporäre Quellen und Stagingreste werden entfernt.

Öffentliche Ausgabe und Run-Log bleiben metadata-only ohne Payloadbytes,
Payloadhex, private Pfade, Streamnamen oder Header-/Sealinhalt. Verifikation
authentifiziert den Zustand zum Prüfzeitpunkt, keine Unveränderlichkeit gegenüber
späteren Schreibzugriffen desselben Benutzers. Externe/Netzwerk-Stores, GPU,
freie Plugins, freie Sprache, allgemeines Beweisen und große
Runtime-Orchestrierung bleiben **DOKUMENTIERT / KONZEPT**.
Belege: [Stream-API](../../include/bruce/artifact/artifact_payload_stream_v1.hpp),
[Streamtests](../../tests/unit/artifact_payload_stream_v1_tests.cpp) und
[Payload-Workflow](../../tests/integration/test_dag_scheduler_v5_payload.py).

## Payload Run Inspector V1

**IMPLEMENTIERT UND BELEGT:**
`bruce dag-scheduler-v5 payload-inspect BUNDLE [--stream-v15]` ist read-only.
Es akzeptiert die kanonischen Solo-/Multi-Payload-Reports V1.3 und V1.4;
Writer und Quellreportverträge bleiben unverändert. Execute-Reports sind hier
`PROTOCOL_ERROR`. Die Ausgabe enthält exakt diese Schlüssel in dieser Reihenfolge:

```json
{"contract":"dag-scheduler-v5-payload-inspect-v1","action":"payload-inspect","status":"VALID","run_finished":true,"run_status":"READY","source_contract":"dag-scheduler-v5-payload-v13","run_id":"example","last_seq":7,"node_count":1,"attempt_count":1,"accepted_artifact_count":1,"stream_check":"NOT_REQUESTED","verified_stream_count":0,"verified_stream_bytes":0,"input_completeness":"NOT_PROVABLE","external_provenance":"NOT_CHECKED"}
```

Das Beispiel zeigt die Form, keine zugesicherte Run-ID oder Sequenzzahl.
Eine kompakte JSON-Zeile plus LF auf stdout, leeres stderr: `VALID`/Exit 0
bedeutet lokale Konsistenz auch bei `run_status=FAILED`; `INCOMPLETE`/Exit 1
bedeutet fehlendes `RUN_FINISHED`, `run_finished=false` und
`run_status=INCOMPLETE`. Abgeschnittene JSONL-Daten, Integritätsfehler, Mutation,
unsichere Dateien oder Prüffristablauf liefern Exit 2, leeres stdout und exakt
`{"error":"PROTOCOL_ERROR"}` plus LF auf stderr. Kein Teilbericht.

`--stream-v15` fordert die Prüfung erhaltener Streams ausdrücklich an:
`stream_check=VERIFIED_PRESENT`, sonst `NOT_REQUESTED`. Fehlende/leere ganze
Streambäume können null geprüfte Streams ergeben. `verified_stream_count` und
`verified_stream_bytes` zählen ausschließlich tatsächlich geprüfte lokale
Streams. `input_completeness=NOT_PROVABLE` und `external_provenance=NOT_CHECKED`
sind immer gesetzt. Kein historischer Modus-, Vollständigkeits-, Reihenfolge-
oder externer Herkunftsnachweis; nachträglich konsistente Streams sind dadurch
nicht als nachträglich erkennbar. Die Namen allein beweisen keine Gültigkeit.
Header, Seal, Chunks und reguläre `input.bin` werden bei angeforderter Prüfung
auf kanonische Struktur, Größe und SHA-256 abgeglichen. Legacy-Dateien,
Indexlücken, fremde Attempts und Stagingreste sind Fehler und bleiben bestehen.

Die neue Route schreibt nichts, startet keinen Worker, publiziert nicht,
repariert nicht und benötigt keinen Store. Sie prüft Request-/Output-Digests
sowie Payload-spezifische Hostzustände, einschließlich unaccepted READY nach
Publikationsfehler und reservierter Preflight-ID ohne Workerstart. Öffentlich
werden weder Digests noch Payloadbytes/Hex, private Pfade, Stream-/Manifestdetails
oder OS-Diagnosen ausgegeben. Feste monotone Prüffrist: 30000 ms; JSON-Tiefe 64,
Retry 0..3, Workers 1..4, 15 Inputs je Attempt, höchstens 1048576 Bytes je Input
und höchstens das Run-Inputbudget in Summe. Puffer höchstens 65536 Bytes.
`O_NOFOLLOW`, owner-only 0700/0600 für Streams, Eigentümer-/Einzellinkprüfung und
Mutationsabgleich begrenzen den Leseweg; atime kann sich durch Lesen ändern.

Belege: [Kern](../../src/artifact/payload_run_inspector_v1.cpp),
[Streamprüfung](../../src/artifact/payload_run_stream_inspector_v1.cpp),
[CLI-Verträge](../../tests/unit/dag_scheduler_v5_payload_inspector_cli_tests.cpp),
[reale Workflows](../../tests/integration/test_dag_scheduler_v5_payload_inspector.py).

## DAG-Scheduler V5 Payload Flow V2

**IMPLEMENTIERT UND BELEGT:** Flow V2 persistiert die bereits ausgeführte
lokale V1.3-Kantenübergabe als `BUNDLE.flow-v2.json`. Die beiden Aufrufe sind:

```text
bruce dag-scheduler-v5 payload SPEC --output BUNDLE --store STORE --flow-v2 [--stream-v15]
bruce dag-scheduler-v5 payload-inspect BUNDLE --flow-v2
```

Der Sidecar ist eine kanonische JSON-Zeile mit abschließendem LF und Mode 0600.
Die Schlüsselreihenfolge ist fest:

```text
contract="bruce-payload-flow-v2"
run_id
spec_digest
report_digest
stream_v15
nodes[]: node_id, callback_id, request_digest, dependencies[]
attempts[]: consumer_node_id, consumer_attempt_id, inputs[]
inputs[]: input_index, producer_node_id, producer_attempt_id,
          manifest_id, object_index, sha256, size_bytes
```

`report_digest` bindet die exakten Legacy-BUNDLE-Bytes; `spec_digest` ist der
bereits gelesene Digest der Specbytes. `object_index` ist in V2 immer 0.
`nodes[]` bewahrt Spec- und Dependency-Reihenfolge, `attempts[]` die Reihenfolge
von `ATTEMPT_STARTED`. Jeder gestartete Attempt erhält seine vollständige
Inputbindung; Preflight-IDs ohne Workerstart werden nicht veröffentlicht.
Das Sidecar enthält keine Payloadbytes, Pfade, PIDs, Kommandozeilen, Secrets
oder streaminternen Dateinamen.

Der read-only-Inspector `payload-inspect BUNDLE --flow-v2` prüft Sidecar,
Legacy-Report, Run-Log, Request-/Outputdateien und die vollständige Menge der
privaten Legacy- oder Stream-V1.5-Inputs. Er bestätigt lokale Konsistenz zum
Prüfzeitpunkt; er bestätigt weder externe Storeexistenz noch historische
Authentizität. Die öffentliche Ausgabe enthält exakt:

```json
{"contract":"dag-scheduler-v5-payload-flow-inspect-v2","action":"payload-inspect","status":"VALID","run_status":"READY","run_id":"example","node_count":2,"attempt_count":2,"binding_count":1,"verified_input_bytes":1,"stream_v15":false,"input_completeness":"VERIFIED","provenance_scope":"LOCAL_CONSISTENCY"}
```

`VALID`/Exit 0 gilt auch für einen konsistenten fachlich fehlgeschlagenen Lauf;
`INCOMPLETE`/Exit 1 bedeutet fehlendes `RUN_FINISHED`. Ungültiges JSON,
fehlende oder zusätzliche Inputs, falsche Lineage-/Digest-/Manifestbindung,
unsichere Dateitypen oder beobachtete Mutation ergeben leeres stdout und
`{"error":"PROTOCOL_ERROR"}` plus LF auf stderr, Exit 2. `--external-inputs` wird mit `FLOW_V2_EXTERNAL_INPUTS_UNSUPPORTED`
abgewiesen; es ist mit Flow V2 ein Syntax-/Protokollfehler vor jedem Datei- oder Storezugriff.

Belege: [PayloadFlowV2](../../include/bruce/artifact/payload_flow_v2.hpp),
[Inspector](../../include/bruce/artifact/payload_flow_inspector_v2.hpp),
[Unit-Verträge](../../tests/unit/payload_flow_v2_tests.cpp),
[reale Payload-Workflows](../../tests/integration/test_dag_scheduler_v5_payload.py)
und [Inspector-Workflows](../../tests/integration/test_dag_scheduler_v5_payload_flow_inspector.py).
Der direkte In-Memory-DAG-Scheduler-V2-Payloadfluss bleibt ein separater Vertrag;
Flow V2 führt keine neue Scheduler-V2-API ein. Externe Stores, Netzwerk/GPU,
freie Plugins, freie Sprache, allgemeines Beweisen und große Runtime-
Orchestrierung bleiben **DOKUMENTIERT / KONZEPT**.

## Artifact Store V1: Objekte und Manifest

**IMPLEMENTIERT UND BELEGT:** Artifact Store V1 verwendet ein lokales,
content-addressed Objektformat. Der Digest ist der kleingeschriebene
64-stellige SHA-256-Textwert; die Identität eines Objekts ist das Paar aus
`sha256` und `size_bytes`. Das Objekt liegt unter:

```text
STORE/
├── store.lock                 # owner-only Sperrdatei
├── objects/
│   └── aa/
│       └── <64-hex>.blob      # unveränderliche Payloadbytes
└── manifests/
    └── <manifest-id>.manifest
```

Jedes Objekt ist höchstens **32 MiB = 33.554.432 Bytes** groß. Diese Grenze
gilt für `put_bytes`, `put_file`, vorhandene Objekte und Manifeste. Der Store
prüft Größe und SHA-256 beim Lesen vollständig; die CLI rendert daraus nur
folgendes metadata-only JSON mit finalem LF:

```json
{"contract":"artifact-store-v1","action":"import","status":"IMPORTED","sha256":"<64-hex>","size_bytes":23}
```

`action` ist `import`, `inspect` oder `verify`; `status` ist entsprechend
`IMPORTED`, `INSPECTED` oder `VERIFIED`. `sha256` und `size_bytes` sind die
einzigen Objektwerte im Bericht. Es gibt keine Payload-, Pfad-, Kommandozeilen-
oder Betriebssystemfelder.

### Kontrollierte Manifestauflösung V1.2

**IMPLEMENTIERT UND BELEGT:** Ein bereits publiziertes kanonisches Manifest
kann ausschließlich über diese neue, read-only Route aufgelöst werden:

```text
bruce artifact-store-v1 resolve MANIFEST_ID --store STORE
```

`MANIFEST_ID` ist ein einzelner validierter ASCII-Token und wird nie als
Dateipfad behandelt. `--store` ist die einzige Option. Nach dem bounded Lesen
und der kanonischen Prüfung des `BRUCE_ARTIFACT_MANIFEST_V1` verifiziert die
Route jedes referenzierte Objekt vollständig gegen Digest und `size_bytes`,
bevor sie den Erfolgsbericht schreibt. Die Manifestreihenfolge bleibt erhalten;
ein leeres `origin` wird als `""` bewahrt und ein Manifest ohne Objekte als
`objects:[]` ausgegeben.

Der Erfolgsvertrag ist metadata-only, hat feste Feldreihenfolge und endet mit
LF:

```json
{"contract":"artifact-store-resolve-v1","action":"resolve","status":"RESOLVED","manifest_id":"v5-abc123","run_id":"run-123","node_id":"a","attempt_id":"a-1","manifest_status":"READY","result_code":"OK","objects":[{"sha256":"<64-hex>","size_bytes":12,"origin":"artifact/a"}]}
```

Die äußere Reihenfolge ist exakt `contract`, `action`, `status`,
`manifest_id`, `run_id`, `node_id`, `attempt_id`, `manifest_status`,
`result_code`, `objects`. Pro Objekt folgen `sha256`, `size_bytes`, `origin`;
`manifest_status` stammt aus dem Manifest und `status` ist der feste
Resolve-Ausgang `RESOLVED`. `media_type` gehört nicht zum öffentlichen
Resolve-Ergebnis.

Bei jeder Syntax-, Store-, Manifest-, Berechtigungs-, Größen- oder
Integritätsverletzung wird kein Teilbericht ausgegeben. stdout bleibt leer,
stderr enthält exakt mit LF und Exit 2:

```json
{"contract":"artifact-store-resolve-v1","error":"PROTOCOL_ERROR"}
```

Resolve verändert keine Dateien, materialisiert keine Payload, liest keine
externen Provider und gibt weder Payloadbytes noch private Pfade aus. Die
bestehenden `import`, `inspect` und `verify`-Feldreihenfolgen und Fehlerverträge
bleiben bytekompatibel. Es gibt kein neues Diagnosebudget; die vorhandene
32-MiB-/`max_file_bytes`-Begrenzung gilt auch für die vollständige Prüfung.

Der optionale kanonische Manifestcodec beginnt mit
`BRUCE_ARTIFACT_MANIFEST_V1` und verwendet diese feste Reihenfolge:

```text
BRUCE_ARTIFACT_MANIFEST_V1
run_id=<token>
node_id=<token>
attempt_id=<token>
status=<token>
result_code=<token>
object_count=<0..1024>
object_0_sha256=<64-hex>
object_0_size_bytes=<0..33554432>
object_0_media_type=<token oder ->
object_0_origin=<token oder ->
```

Die vier Objektzeilen wiederholen sich exakt für `object_count`. IDs und
kontrollierte Token sind höchstens 128 Bytes; Digest und Größe binden jedes
Manifestobjekt an den Store. Unbekannte oder doppelte Felder, falsche
Reihenfolge, absolute oder ausbrechende Pfade, nichtkanonische Zahlen,
Payloadbytes und zusätzliche/trailing Bytes werden verworfen. Die Manifest-
Publikation ist exklusiv und idempotent nur für bytegleiche Inhalte.

### CLI-Fehlervertrag und Sicherheitsgrenze

```text
bruce artifact-store-v1 import FILE --store STORE
bruce artifact-store-v1 inspect DIGEST --store STORE
bruce artifact-store-v1 verify DIGEST --store STORE
bruce artifact-store-v1 resolve MANIFEST_ID --store STORE
```

Gültige Aufrufe liefern Exit 0 und den Metadatenbericht. Syntaxfehler,
ungültige Digests, fehlende oder manipulierte Objekte, Überschreitung der
32-MiB-Grenze, Symlinks, Mehrfach-Hardlinks, nichtreguläre Dateien,
unsichere Rechte, Pfad-Escapes und Storefehler liefern leeres stdout, exakt
`{"contract":"artifact-store-v1","error":"PROTOCOL_ERROR"}` auf stderr und
Exit 2. Die interne Fehlerklassifizierung bleibt eine Core-API-Eigenschaft;
die öffentliche CLI gibt keine OS- oder Pfaddetails preis.

Die Implementierung verwendet owner-only Verzeichnisse und Dateien,
descriptor-relative `O_NOFOLLOW`-Öffnungen, Streaming-Kopie und SHA-256,
`fsync` sowie exklusive atomare Veröffentlichung. SHA-256 und Digest-Adressen
sind Integritätsmerkmale, keine Verschlüsselung, Authentifizierung oder
Zugriffskontrolle. Download, GC, Netzwerk, externe Provider, automatische
Execute-Integration, freie Runtime-Funktionen und Remote-Payloadspeicher sind
**DOKUMENTIERT / KONZEPT**. Die explizite V5-Publish-Route bindet nur bereits
validierte lokale Ergebnisse und ist kein allgemeiner Upload- oder Download-
Dienst.

Belege: [Store-Header](../../include/bruce/artifact/artifact_store_v1.hpp),
[Manifest-Header](../../include/bruce/artifact/artifact_manifest_v1.hpp),
[CLI-Header](../../apps/artifact_store_v1_cli.hpp),
[Unit-Tests](../../tests/unit/artifact_store_v1_tests.cpp),
[Resolve-CLI-Tests](../../tests/unit/artifact_store_v1_cli_tests.cpp),
[Resolve-Workflow](../../tests/integration/test_artifact_store_resolve_v1.py) und
[Workflow-Tests](../../tests/integration/test_artifact_store_v1.py).

## Artifact Store V2: Providerdatei und Fetch-Bericht

**IMPLEMENTIERT UND BELEGT:** Die explizite Providerdatei ist kanonisches
UTF-8-JSON, höchstens **32 MiB = 33.554.432 Bytes**, mit exakt diesem Schema
und dieser Feldreihenfolge; nach der schließenden Klammer folgt genau ein LF:

```json
{"contract":"bruce-artifact-store-providers-v2","providers":[{"id":"mirror-a","root":"/absolute/root"}]}
```

Es gibt keine weiteren Whitespaces, Felder oder alternativen Darstellungen.
`providers` ist eine nach eindeutiger `id` streng aufsteigende Liste;
`[]` ist gültig, erlaubt aber keinen Fetch. IDs erfüllen
`[A-Za-z0-9][A-Za-z0-9._-]{0,127}`. Jeder `root` ist ein nichtleerer absoluter
Dateisystempfad ohne NUL, `.`-/`..`-Komponenten, doppelte oder abschließende
Separatoren; `/` allein ist damit ebenfalls ungültig. Nicht-ASCII-Zeichen
stehen direkt als UTF-8 in Strings. Quotes und Backslashes werden mit
Backslash escaped, Kontrollzeichen ausschließlich als kleingeschriebenes
`\u00xx`; alternative Unicode-, Slash- oder Kurz-Escapes werden abgewiesen.
NUL bleibt auch escaped unzulässig. Ein Root muss erst beim Fetch existieren.

Die Datei selbst muss regulär, owner-only, im Besitz der effektiven UID und
genau einmal hardlinked sein. Jede Öffnungskomponente ist symlinkfrei;
Vorfahren dürfen gemeinsam genutzt sein. Beim Fetch gelten die owner-only-
und UID-Prüfungen außerdem für Mirror-Root, `objects/`, Digestpräfix und die
reguläre single-hardlink Objektdatei. V2 liest nur den aus dem erwarteten
Digest abgeleiteten Pfad `objects/aa/<64-hex>.blob`, keine Remote-Manifeste.
Das erwartete Paar aus Digest und exakter Größe ist die Objektidentität;
Mirror-Metadaten sind keine Autorität. Der Payloadgrenzwert ist ebenfalls
32 MiB; leere Objekte sind zulässig.

Die [einzige Fetch-Route](commands.md#artifact-store-v2--kontrollierter-dateisystem-mirror-fetch)
liefert nach vollständiger Prüfung und lokaler V1-Publikation mit Exit 0,
leerem stderr und genau einem LF auf stdout:

```json
{"contract":"artifact-store-fetch-v2","action":"fetch","status":"FETCHED","provider":"mirror-a","sha256":"<64-hex>","size_bytes":123}
```

Feldreihenfolge: `contract`, `action`, `status`, `provider`, `sha256`,
`size_bytes`. Der Digest-Platzhalter steht für 64 lowercase Hexzeichen.
Fehler liefern Exit 2, leeres stdout und exakt diese Zeile mit LF auf stderr:

```json
{"contract":"artifact-store-fetch-v2","error":"PROTOCOL_ERROR"}
```

Beide Berichte sind metadata-only, ohne Payload, Pfade oder OS-Diagnosen.
Es entsteht kein V2-Objektformat und kein Manifest: die materialisierten
Bytes liegen im unveränderten lokalen V1-Layout. V1/V1.2-Ausgaben und
V1.3/V1.4-/Worker-/Bundleformate ändern sich nicht. Nach einem
Post-Rename-Synchronisierungsfehler zählt eine erfolgreiche frische
V1-Verifikation als Fetch-Erfolg; bei zusätzlichem Verifikations-I/O-Fehler
ist `PROTOCOL_ERROR` trotz möglicherweise sichtbarem vollständigem Objekt
möglich. Es gibt keinen Rollback oder zusätzlichen Crash-Dauerhaftigkeits-
vertrag. Weitere [Sicherheitsgrenzen und Nichtziele](commands.md#artifact-store-v2--kontrollierter-dateisystem-mirror-fetch)
gelten auch für dieses Format.

Belege: [Provider-/Fetch-Core](../../src/artifact/artifact_store_v2.cpp),
[CLI-Renderer](../../apps/artifact_store_v2_cli.hpp),
[Core-Verträge](../../tests/unit/artifact_store_v2_tests.cpp) und
[reale Workflows](../../tests/integration/test_artifact_store_v2.py).
Die generierten CLI-Beispiele prüfen V2 nicht.

## Artifact Store V3: HTTPS-Providerdatei und Fetch-Bericht

**IMPLEMENTIERT UND BELEGT:** Die explizit benannte, höchstens
**33.554.432 Bytes (32 MiB)** große Providerdatei verwendet exakt diese
Feldreihenfolge und ein abschließendes LF:

```json
{"contract":"bruce-artifact-store-providers-v3","providers":[{"id":"https-a","origin":"https://artifacts.example.org:443/bruce"}]}
```

Keine weiteren Felder, Whitespaces, JSON-Escapes oder Nicht-ASCII-Zeichen
sind zulässig. `providers` ist nach eindeutiger ID streng aufsteigend
sortiert; `[]` ist gültig, erlaubt aber keinen Fetch. IDs erfüllen
`[A-Za-z0-9][A-Za-z0-9._-]{0,127}`. Die reguläre owner-only Datei gehört der
effektiven UID, hat genau einen Hardlink und keine Symlinks im Öffnungspfad;
gemeinsame Vorfahren sind zulässig. Größe und Descriptor-Metadaten werden
vor und nach dem begrenzten Lesen geprüft. `..` im Dateipfad ist unzulässig.

`origin` ist höchstens 2048 ASCII-Bytes lang und beginnt exakt mit `https://`.
Zulässig sind ein lowercase DNS-Hostname (höchstens 253 Zeichen, Labels
1–63 Zeichen aus `a-z`, `0-9`, `-`, ohne Rand-Bindestrich und abschließenden
Punkt) oder kanonisches dotted-decimal IPv4. Abgekürzte, oktale oder
ganzzahlige IPv4-Schreibweisen und IPv6-Literale sind ausgeschlossen.
Ein optionaler Port ist dezimal 1–65535 ohne führende Nullen; ohne Port gilt
443. Sowohl explizites `:443` als auch der ausgelassene Port sind zulässig.

Ein optionales absolutes Pfadpräfix besteht aus nichtleeren Komponenten mit
nur `[A-Za-z0-9._~-]`; Komponenten `.` und `..`, doppelte und abschließende
Slashes sind unzulässig. `https://host` ist ein leerer Präfix;
`https://host/` ist ungültig. Userinfo, Query, Fragment, Prozentkodierung,
Backslashes und alternative URL-Schreibweisen werden abgewiesen.
Der einzige Request ist `GET <origin>/objects/aa/<64-hex>.blob`, wobei
`aa` die ersten beiden Zeichen des erwarteten Digests sind. Es werden keine
Remote-Manifeste geladen und keine anderen Provider ausprobiert.

Die [feste V3-Route und Transportgrenzen](commands.md#artifact-store-v3--kontrollierter-https-fetch)
binden Digest und exakte Größe einschließlich leerer Objekte bis 32 MiB.
Nur TLS mit Zertifikats-/Hostnamenprüfung und HTTP 200 ist erfolgreich;
Redirects, Wiederholungen, `Content-Encoding` und doppelte `Content-Length`
scheitern. Die Transportfristen sind absolut 5 Sekunden bis einschließlich
DNS/TLS und 30 Sekunden insgesamt; lokale Dateiarbeit ist davon nicht erfasst.
Der überwachte Linux-Hilfsprozess liefert begrenzte private IPC-Daten;
der Elternprozess prüft den vollständigen Transport und die Objektidentität
vor jeder V1-Publikation. Es entsteht kein neues Objekt- oder Manifestformat.

Erfolg: Exit 0, leeres stderr und exakt diese Felder mit LF auf stdout:

```json
{"contract":"artifact-store-fetch-v3","action":"fetch","status":"FETCHED","provider":"https-a","sha256":"<64-hex>","size_bytes":123}
```

Fehler: Exit 2, leeres stdout und exakt diese Zeile mit LF auf stderr:

```json
{"contract":"artifact-store-fetch-v3","error":"PROTOCOL_ERROR"}
```

Die Berichte enthalten keine Payloadbytes, Pfade, Origins, Hostnamen,
TLS-/OS-Diagnosen oder Teilresultate. V1/V1.2/V2-Ausgaben und bestehende
V5-/Worker-/Bundleformate bleiben unverändert. Die V1-Post-Rename-Grenze
gilt wie bei V2: frische vollständige Verifikation kann einen Sync-Fehler
in Erfolg auflösen; ein weiterer I/O-Fehler kann trotz sichtbarem vollständigem
Objekt Fehler liefern. Es gibt keinen Rollback oder zusätzlichen
Crash-Dauerhaftigkeitsvertrag. Die
[Nichtziele und Betriebsgrenzen](commands.md#artifact-store-v3--kontrollierter-https-fetch)
umfassen insbesondere erste DNS-Adresse ohne Fallback, keine Credentials,
Proxys, Retries, Remote-Schreibvorgänge oder Worker-Netzwerkzugriffe.

Belege: [Core](../../src/artifact/artifact_store_v3.cpp),
[CLI-Renderer](../../apps/artifact_store_v3_cli.hpp),
[Core-Verträge](../../tests/unit/artifact_store_v3_tests.cpp) und
[lokale TLS-Workflows](../../tests/integration/test_artifact_store_v3.py).
Die generierten CLI-Beispiele führen V3 nicht aus.

## Artifact Store V4: Mirror-Envelope und Bericht

**IMPLEMENTIERT UND BELEGT:** V4 verwendet unveränderte kanonische V1-Objekt-
und Manifestbytes. Nur der Remote-Manifesttransport erhält ein Envelope:

```text
BRUCE_ARTIFACT_MIRROR_V4
manifest_id=example
manifest_sha256=<SHA-256 der folgenden V1-Manifestbytes>
manifest_size_bytes=<exakte Bytezahl der folgenden V1-Manifestbytes>
BRUCE_ARTIFACT_MANIFEST_V1
<übrige kanonische V1-Manifestzeilen einschließlich finalem LF>
```

Es gibt keine Leerzeile zwischen Bindungszeilen und Manifest. Das Envelope
endet mit den exakten Manifestbytes; zusätzliche Bytes werden abgewiesen.
`manifest_size_bytes` ist kanonisch dezimal ohne Vorzeichen oder führende
Nullen außer `0`. Der Decoder bindet die angeforderte Manifest-ID, SHA-256,
exakte Bytezahl und das kanonische READY-Manifest. SHA-256 schützt die
übertragenen Bytes; es ist keine Signatur oder Autoridentität.

Die feste Remoteroute lautet `/manifests/<MANIFEST_ID>.mirror`, die Objektroute
`/objects/<erste-zwei-hex>/<sha256>.blob`, jeweils hinter dem expliziten
HTTPS-Basispfad. Mirror lädt die unveränderten Objektbytes hoch und das
Envelope zuletzt; Restore lädt zuerst das Envelope. Das gesamte Envelope
passt in **1.048.576 Bytes (1 MiB)**, die Summe der referenzierten Objektgrößen
in **33.554.432 Bytes (32 MiB)**, einschließlich mehrfacher Referenzen.
Es entsteht kein neues lokales V1-Format und kein Workerformat.

Die [beiden festen Befehle und Transportgrenzen](commands.md#artifact-store-v4--hostseitiger-https-mirror-und-restore)
verwenden GET/200 und PUT/200, 201 oder 204. PUT-Antwortkörper sind auf
65.536 Bytes begrenzt. 204 darf ohne Content-Length oder mit genau einer
`Content-Length: 0` enden; Transfer-Encoding, andere deklarierte Länge,
doppelte Content-Length und Content-Encoding werden abgewiesen.

Der Erfolgsbericht besteht in fester Reihenfolge aus `contract`, `action`,
`status`, `manifest_id`, `manifest_sha256`, `manifest_size_bytes`,
`object_count` und `total_object_bytes`, gefolgt von genau einem LF:

```json
{"contract":"artifact-store-mirror-v4","action":"mirror","status":"MIRRORED","manifest_id":"example","manifest_sha256":"<64-hex>","manifest_size_bytes":312,"object_count":1,"total_object_bytes":23}
```

Restore ersetzt nur `action` durch `restore` und `status` durch `RESTORED`.
`manifest_sha256` und `manifest_size_bytes` beschreiben die kanonischen
V1-Manifestbytes ohne Envelope; `object_count` und `total_object_bytes`
zählen Manifestreferenzen. Erfolg liefert stdout/Exit 0 und leeres stderr.
Jeder Fehler liefert leeres stdout, Exit 2 und genau diese stderr-Zeile mit LF:

```json
{"contract":"artifact-store-mirror-v4","error":"PROTOCOL_ERROR"}
```

URL, Pfade, Credentials, Payloadbytes und Diagnosen erscheinen nicht in den
Berichten. Es gibt keine Remotekonfiguration in Records oder Workerframes.
Vorhandene lokale Einträge werden nie ersetzt; vor vollständig geprüftem
Download erfolgt keine lokale Publikation. Lokale I/O-Fehler während der
anschließenden V1-Publikation können bereits verifizierte Einträge
hinterlassen; V4 ergänzt keinen transaktionalen Rollback.

Belege: [Core](../../src/artifact/artifact_store_v4.cpp),
[Renderer](../../apps/artifact_store_v4_cli.hpp),
[Core-Verträge](../../tests/unit/artifact_store_v4_tests.cpp) und
[reale lokale TLS-Workflows](../../tests/integration/test_artifact_store_v4.py).
Die generierten CLI-Beispiele führen V4 nicht aus.

## DAG-Scheduler V4 Run-Bundle

**IMPLEMENTIERT UND BELEGT:** Die drei festen lokalen Routen sind:

```text
bruce dag-scheduler-v4 execute SPEC --output BUNDLE
bruce dag-scheduler-v4 inspect BUNDLE
bruce dag-scheduler-v4 replay BUNDLE
```

`execute` übernimmt ausschließlich die [V3-Spec](#dag-scheduler-v3-spec-datei)
und deren festen Callback-Katalog. Es veröffentlicht atomar und exklusiv;
beide Leser schreiben keine Datei. Das kanonische Bundle ist höchstens
5 MiB (5.242.880 Bytes) groß, einschließlich des folgenden Envelopes:

```text
BRUCE_DAG_SCHEDULER_RUN_V4
<SHA-256 des vollständigen kanonischen Payloads>
<kanonischer längenpräfixierter Payload>
```

Die ersten beiden Zeilen enden exakt mit LF. Der Digest besteht aus 64
kleingeschriebenen Hexziffern. Es gibt kein BOM, CRLF, JSON oder implizites
finales LF nach dem Payload. Strings werden als minimale dezimale Byteanzahl,
`:` und genau diese uninterpretierten Bytes kodiert; sie können NUL und LF
enthalten. Unsigned-Zahlen werden minimal dezimal mit anschließendem LF
kodiert, ohne Pluszeichen, Leerraum oder führende Nullen (außer `0`).

Die feste Feldreihenfolge des Payloads ist:

| Block | Kodierte Daten in Reihenfolge |
| --- | --- |
| Spec | Scheduler-Version, DAG-ID, Startbudget, Receipt-Budget, Knotenanzahl |
| Je Knoten | Knoten-ID, Callback-ID, Abhängigkeitsanzahl und IDs, Requestbytes, Requestdigest |
| Receipts | Receiptanzahl; je Receipt Knoten-ID, Callback-ID, Requestdigest, Inputanzahl und Inputs, Outcome, Output-Präsenz, optionaler Output, Fehlercode |
| Je direktem Input | Producer-ID, Resultcode, Payloadbytes, Payloaddigest |
| Optionaler Output | Outputversion, Resultcode, Payloadbytes, Payloaddigest |
| Trace | Ereignisanzahl; je Ereignis numerischer Eventwert und Knoten-ID |

Outcome und Event werden mit den bestehenden V2-Enumwerten kodiert;
Output-Präsenz ist `0` oder `1`. Die Spec nutzt `dag-scheduler-v2` und
Outputs `dag-scheduler-output-v2`. Knoten, Abhängigkeiten und Receipts sind
nach ID sortiert, direkte Inputs nach Producer-ID; Trace bleibt chronologisch.
Eine einzelne Zeichenkette ist vor Allokation auf 64 Bytes, Request- und
Payloadfelder auf 65.536 Bytes begrenzt. Decoder begrenzt Knoten/Receipts auf
64, direkte Abhängigkeiten/Inputs auf 15 und Trace auf höchstens `2*N+2`.
V2-Struktur-, Digest-, Arity- und Receipt-Budgetprüfung gelten zusätzlich.
Die Dateiobergrenze erweitert diese engeren Limits nicht.

**Vertraulichkeitsgrenze:** Das Bundle enthält Request- und Payload-Rohbytes;
es ist ein lokales vertrauliches Artefakt. stdout enthält nur Metadaten mit
IDs, Größen, Digests, Receipts und Trace, keine Rohbytes oder deren Hex.
SHA-256 ist keine Verschlüsselung oder Authentifizierung. Die ursprüngliche
V3-Textdatei wird nicht gespeichert; ihr Dateidigest wird nicht als V4-
`spec_digest` ausgegeben. Optionales finales LF in der Spec ändert nicht das
Bundle des gleichen geparsten Laufs.

`inspect` prüft Header, Digest, Dekodierung, Kanonizität und Struktur ohne
Callbacks. Es bestätigt die kodierten Receipts und den Trace als strukturell
zulässig, keine erneute Ausführung. `replay` rekonstruiert die Spec, führt
sie mit `literal-v1`, `append-v1`, `join-v1` erneut über V2 aus und verlangt
vollständige Gleichheit von Spec, Receipts und Trace sowie byteidentisches
Packen. Ein neu berechneter Digest für geänderte Receipts/Trace kann die
Inspektion bestehen, während Replay `DagSchedulerV4ReplayMismatch` meldet.
Replay bestätigt lokale Reproduzierbarkeit unter dem festen Vertrag, keinen
mathematischen Beweis, Autor oder Wahrheitsgehalt.

Es gibt keine freien Callbacks, externen Speicher, Netzwerk- oder
Parallelpfade und keine Migration alter V3-Läufe. MCP bietet eine optionale
[read-only Strukturprüfung](#mcp-inspector), ohne erneute Ausführung. Siehe
[Befehle, Fehler und Belege](commands.md#dag-scheduler-v4--kanonische-lokale-bundles-und-replay).

## DAG-Scheduler V3 Spec-Datei

**IMPLEMENTIERT UND BELEGT:** `bruce dag-scheduler-v3 execute SPEC` und
`bruce dag-scheduler-v3 inspect SPEC` verwenden dieselbe `parse_spec()`-Validierung
und lesen
höchstens 65.536 unveränderte Dateibytes über den begrenzten lokalen Leser.
Die Datei ist ASCII ohne NUL-Byte und CR; LF ist der einzige Zeilentrenner,
CRLF ist ungültig. Ein abschließendes LF ist optional und zählt zur Grenze;
weitere abschließende LF/Leerzeilen sind unzulässig. Der SHA-256-
`spec_digest` bindet alle Originalbytes einschließlich des optionalen finalen
LF. Die erste Zeile ist exakt `BRUCE_DAG_SCHEDULER_SPEC_V3`.

Es folgen ein globaler Block und 1..64 Knotenblöcke. Genau eine vollständig
leere Zeile trennt je zwei Blöcke. Führende, doppelte oder abschließende
Blockseparatoren und Leerzeilen innerhalb eines Blocks sind ungültig. Jede
Feldzeile hat die Form `key=value`; Kommentare, Whitespace-Trimmung,
unbekannte/doppelte Schlüssel und implizite Standardwerte sind unzulässig.

Die globalen Felder sind vollständig und genau in dieser Reihenfolge:

```text
dag-id=<V2-ID>
max-started-nodes=<positive Dezimalzahl>
max-total-receipt-bytes=<positive Dezimalzahl>
```

Jeder Knotenblock ist vollständig und genau in dieser Reihenfolge:

```text
node=<V2-ID>
depends=<kommagetrennte V2-IDs oder leer>
callback=<literal-v1|append-v1|join-v1>
request-hex=<kleingeschriebene Hexziffern oder leer>
```

`dag-id`, `node` und Abhängigkeits-IDs haben 1..64 ASCII-Zeichen aus
`A-Z`, `a-z`, `0-9`, `-`, `_`. Node-Blöcke müssen strikt lexikographisch nach
`node` aufsteigen; der Parser sortiert sie nicht nachträglich. Nichtleere
`depends`-Listen sind strikt lexikographisch aufsteigend, ohne Duplikate,
Leerzeichen oder leere Listenelemente. `depends=` ist für Wurzelknoten
verpflichtend leer. Abhängigkeiten müssen existieren, dürfen nicht auf den
eigenen Knoten zeigen und müssen einen azyklischen Graphen bilden.

Budgetwerte enthalten ausschließlich `0-9`, ohne Vorzeichen, Leerraum,
Hexpräfix oder Suffix; positive Werte mit führenden Nullen werden angenommen.
Es gelten die unveränderten V2-Grenzen:

| Feld / Größe | Gültiger Bereich / Bedeutung |
| --- | --- |
| `max-started-nodes` | 1..64 tatsächliche Callback-Starts |
| `max-total-receipt-bytes` | 1..4.194.304 retinierte Output-Payloadbytes einschließlich aller direkten Input-Kopien, keine Requests/Metadaten |
| Knoten | 1..64, höchstens 15 direkte Abhängigkeiten je Knoten |
| `request-hex` | Gerade Anzahl ausschließlich `0-9a-f`, ohne Präfix; leer kodiert leere Bytes, `00` kodiert NUL |
| Request und erfolgreiche Payload | Je höchstens 65.536 dekodierte Bytes; die Dateigrenze bleibt zusätzlich maßgeblich |

Großgeschriebenes, ungerades oder ungültiges Hex wird abgewiesen. Decode und
SHA-256 ergeben den V2-Request samt Digest. Der feste Callback `literal-v1`
hat 0 Abhängigkeiten und liefert den Request (`LITERAL_READY`); `append-v1`
hat genau 1 und hängt den Request an dessen Payload (`APPENDED_READY`);
`join-v1` hat 2..15 und verbindet die direkten Payloads in V2-Producer-ID-
Reihenfolge mit Requestbytes als Separator (`JOINED_READY`). Append/Join
prüfen alle Kopien überlaufsicher vor dem Zusammenfügen; ein Ergebnis über
65.536 Bytes wirft `DagSchedulerV3PayloadLimitExceeded` ohne partiellen Run.

Kanonisches vierknotiges Beispiel, exakt mit einem finalen LF; seine Bytes
entsprechen dem [V3-Prozesstest](../../tests/integration/test_dag_scheduler_v3_cli.py):

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
request-hex=0000

node=d-join
depends=b-transform,c-independent
callback=join-v1
request-hex=73656e74696e656c2d6a6f696e2d7633
```

Bei `execute` erzeugt `c-independent` zwei NUL-Bytes. Die Outputgrößen sind 5, 10, 2 und
28 Bytes; Join erhält nur `b-transform` und `c-independent`. Der JSON-Vertrag
`dag-scheduler-v3-cli-v1` zeigt unter `spec_version` den Wert
`dag-scheduler-spec-v3` und unter `scheduler_version` wahrheitsgemäß
`dag-scheduler-v2`. Er enthält nur Metadaten, Größen, Digests und V2-Receipts/
Trace, keine Request-/Payloadbytes oder deren Hex. Das ist kein Bundle,
keine Persistenz-/Replay-Schnittstelle und keine Vertraulichkeitsgarantie.
Der read-only Inspector führt die oben beschriebenen Callbacks nicht aus
und ruft `execute_dag_scheduler_v2` nicht auf. Sein eigener JSON-Vertrag
`dag-scheduler-v3-inspect-v1` enthält dieselben Spec-Versionen, den Digest,
die DAG-ID und Budgets, dazu `nodes` in kanonischer Spec-Reihenfolge mit nur
`node_id`, `callback_id`, `dependencies`, `request_size` und `request_digest`.
`planned_start_order` ist hier `a-source`, `b-transform`, `c-independent`,
`d-join`: die statische lexikographische Kahn-Reihenfolge bei ausschließlich
READY-Ergebnissen, ohne Budget-Cancellation zu simulieren. Es entstehen keine
Payloads, Receipts oder Laufresultate. Der Bericht enthält weder Requestbytes
noch Payloadbytes, deren Hex oder `request-hex`. Auch leere/NUL-Requests bleiben
auf Größe und Digest beschränkt. Callbackfehler, Payloadgrößen und tatsächliche
Receipt-Budgetnutzung werden nicht vorhergesagt. Format-, Zyklus- und
Größenfehler einschließlich 65.537 Dateibytes liefern keinen Teilbericht.
Siehe [Befehl, Bericht und Fehler](commands.md#dag-scheduler-v3--begrenzter-spec-dateiadapter).
Phase-DAG V2/V3 verwenden andere Marker, Parser und Verträge und bleiben
unverändert.

## Phase-DAG V2 Spec und Bundle

**IMPLEMENTIERT UND BELEGT:** `phase-dag-v2 execute` liest einen Text-Spec bis
65536 Bytes. Die erste Zeile ist exakt `BRUCE_PHASE_DAG_SPEC_V1`. Es folgen
ein globaler Block und 1..16 Knotenblöcke, jeweils durch mindestens eine
vollständig leere Zeile getrennt. LF-Zeilenenden verwenden; CRLF wird nicht
normalisiert. Pro nichtleerer Zeile steht genau `key=value` mit genau einem
`=`. Kommentare, unbekannte oder doppelte Schlüssel und fehlende Pflichtfelder
werden abgelehnt; es gibt weder Leerraumtrimmung noch implizite Standardwerte.
Die Schlüsselreihenfolge innerhalb eines Blocks ist frei. Ein abschließender
Zeilenumbruch und weitere leere Zeilen sind erlaubt.

```text
BRUCE_PHASE_DAG_SPEC_V1
dag-id=demo
max-started-nodes=3
max-receipt-bytes=65536
max-trace-entries=32

node=a
depends=
core=1020a54050607080
active-index=2
context-id=reference.context_1
context-file=contexts/a.bin
technique=masked-match-v1
max-operations=16
max-trace-entries=18

node=b
depends=a
core=1020a54050607080
active-index=2
context-id=reference.context_1
context-file=contexts/a.bin
technique=rotate-match-v1
max-operations=16
max-trace-entries=18

node=gate
depends=a,b
adapter=evidence-gate-v1
```

Jeder globale Schlüssel im Beispiel ist verpflichtend. `dag-id`, `node` und
`context-id` enthalten 1..64 Zeichen aus ASCII-Buchstaben, Ziffern, `_`, `.`,
`-`. Knoten-IDs sind eindeutig. `depends=` ist für Wurzelknoten verpflichtend
leer, sonst eine kommaseparierte, lexikographisch sortierte Liste eindeutiger
bestehender IDs ohne Leerzeichen oder leere Elemente. Selbstreferenzen und
Zyklen sind ungültig; höchstens 15 Abhängigkeiten pro Knoten und insgesamt
64 Kanten sind erlaubt. Die CLI sortiert Knotenblöcke, aber keine
Abhängigkeitslisten.

| Feld | Gültiger Bereich / Bedeutung |
| --- | --- |
| Globales `max-started-nodes` | 0..16; Zahl tatsächlich gestarteter Adapteraufrufe, 0 ist ein gültiger Teilbefund |
| Globales `max-receipt-bytes` | 1..262144 Bytes pro kodiertem READY-Receipt; kein Gesamtdateibudget |
| Globales `max-trace-entries` | 2..512 DAG-Traceeinträge; begrenzt die Aufzeichnung, nicht die Knotenzahl |
| Knoten-`core` | Genau 16 Hexziffern für acht Bytes, ohne `0x`; Groß-/Kleinschreibung erlaubt |
| Knoten-`active-index` | 0..7 |
| Knoten-`technique` | Genau `masked-match-v1`, `rotate-match-v1` oder `context-conflict-v1` |
| Knoten-`max-operations` | 0..16 |
| Knoten-`max-trace-entries` | 2..18 NanoCell-Traceeinträge |

Zahlenfelder sind ausschließlich vorzeichenlose dezimale UInt64-Ziffernfolgen;
führende Nullen werden akzeptiert, Vorzeichen und Leerraum nicht. Für einen
NanoCell-Knoten sind alle neun Knotenschlüssel im Beispiel verpflichtend; die
CLI bindet dafür `nanocell-phase-v1`/`v1` und `nanoneuron-core-v1` fest. Ein
Evidence-Gate-Knoten hat dagegen **genau** die drei Schlüssel `node`, `depends`
und `adapter=evidence-gate-v1`; er besitzt keinen Core, Kontext, Technik oder
lokale Budgets. Weitere Adapterwerte oder zusätzliche Schlüssel werden
abgelehnt.

`evidence-gate-v1` bildet seine kanonische Anfrage ohne freie Parameter. Es
wertet ausschließlich die gespeicherten Status- und Digest-Referenzen seiner
direkten READY-Vorgänger aus, nie deren Rohbytes. Mindestens ein Vorgänger und
nur `COMPLETE`-Status erzeugen `COMPLETE`. Ein `INCOMPLETE`- oder `REJECTED`-
Vorgänger beziehungsweise keine Abhängigkeit erzeugt einen gültigen
`REJECTED`-Output. Ein `FAILED`, `BLOCKED` oder `CANCELLED`-Vorgänger blockiert
den Gate-Knoten bereits im DAG-Kern, bevor der Adapter aufgerufen wird.

`context-file` wird relativ zum **Spec-Verzeichnis** aufgelöst, unabhängig
vom Arbeitsverzeichnis des Prozesses. `../contexts/a.bin` ist ebenfalls ein
relativer Pfad; absolute Pfade bleiben absolut. Die Datei wird als rohe
Bytefolge mit höchstens **4096 Bytes** gelesen, einschließlich NUL oder
abschließendem LF. Ein leerer Kontext ist erlaubt; der Digest wird aus den
tatsächlichen Bytes berechnet. Für einen passenden Referenzkontext schreiben:

```sh
mkdir -p contexts
printf %s nanocell-reference-context-v1 > contexts/a.bin
```

Alle Requests werden vor dem Start validiert. Ein formal gültiger, zur Technik
nicht passender Kontext ist dagegen ein normaler NanoCell-Ablehnungsbefund.
Der Export beginnt mit `BRUCE_PHASE_DAG_RUN_V2`, Payload-SHA-256 und
längenpräfixiertem kanonischem Payload. Er bindet Spec, Requests einschließlich
Kontextbytes, Receipts, Ausgaben, Evidenz und Trace. Vertrauliche Kontexte daher
auch im Bundle vertraulich behandeln. Das Bundle ist kein JSON und kein
`BRUCE_MOD_RUN_V1`-Einzellauf; `bruce replay` und `nanocell replay` sind dafür
nicht die passenden Lesebefehle. `phase-dag-v2 inspect/replay` sind portabel
und brauchen die ursprünglichen Dateien nicht. Der MCP Inspector kann
dieselben gespeicherten V2-Bytes prüfen, hat aber eine engere Uploadgrenze
von 1 MiB als die 5-MiB-Grenze der CLI.

## Phase-DAG V3 Spec und Bundle

**IMPLEMENTIERT UND BELEGT:** die getrennte V3-Text-Spec beginnt exakt mit
`BRUCE_PHASE_DAG_SPEC_V3`; `execute` liest höchstens 65536 Specbytes.
Globale Felder, LF-Blocktrennung, strikte `key=value`-Syntax, Identifikatoren,
Zahlenbereiche, Knoten-/Kantengrenzen und Kontextdateiauflösung folgen den
[oben beschriebenen Regeln](#phase-dag-v2-spec-und-bundle). Unbekannte,
doppelte oder fehlende Schlüssel werden vor Ausführung abgelehnt.
V3 ist ein eigenständiger Vertrag; ein V1-Specmarker wird nicht angenommen.

Die lokale Ablaufsteuerung `PhaseDagControlV3::max_parallel_nodes` mit
Standard 1 und Bereich 1..4 steht nicht im Spec oder Bundle. Nur
`execute SPEC --output BUNDLE [--max-parallel-nodes N]` setzt sie über die CLI;
`inspect` und `replay` akzeptieren keine Optionen. Physische Wellen unabhängiger
Adapter ändern weder die kanonische Receiptordnung noch den bisherigen
seriellen logischen Trace. Für denselben Spec bleiben die Bundlebytes bei
allen zulässigen Werten bytegleich; Replay verwendet weiterhin Standard 1.
Thread-, Laufzeit- und Timingdaten werden nicht gespeichert. Marker, Codec,
Payload- und Receiptgrenzen bleiben unverändert. Keine Performanceverbesserung
wird behauptet.

```text
BRUCE_PHASE_DAG_SPEC_V3
dag-id=demo-v3
max-started-nodes=3
max-receipt-bytes=65536
max-trace-entries=32

node=a
depends=
core=1020a54050607080
active-index=2
context-id=reference.context_1
context-file=contexts/a.bin
technique=masked-match-v1
max-operations=16
max-trace-entries=18

node=gate1
depends=a
adapter=payload-gate-v1

node=gate2
depends=gate1
adapter=payload-gate-v1
```

Der NanoCell-Knoten benötigt genau die neun gezeigten Knotenschlüssel und
bindet fest `nanocell-phase-v3`/`v1`; sein Kontext wird relativ zum Spec gelesen,
maximal 4096 Bytes. Der Payload `nanocell-phase-payload-v1` enthält NanoCell-
Status, Final-State-Präsenz und gegebenenfalls den Final State, keine
Kontextbytes und keinen vollständigen NanoCell-Lauf. Die drei Techniken bleiben
`masked-match-v1`, `rotate-match-v1` und `context-conflict-v1`.

Ein Gate hat genau `node`, `depends`, `adapter=payload-gate-v1` und bindet
`payload-gate-v1`/`v1`; keine weiteren Requestfelder, Budgets oder Versionen.
Es verarbeitet nur direkte READY-Vorgänger in lexikographischer ID-Reihenfolge.
Mindestens ein Input, ausschließlich `COMPLETE`, nichtleere Payloadbytes und
passende Payloaddigests ergeben `COMPLETE`. Keine Abhängigkeit, ein
`INCOMPLETE`-/`REJECTED`-Status oder leere beziehungsweise digestinkonsistente
Payloadbytes ergeben beim Gate `REJECTED`. Strukturell ungültige
Inputreferenzen führen zu einem Adapterfehler.
Der `payload-gate-payload-v1`-Manifestpayload enthält Entscheidung, Inputanzahl
und geordnete Payloaddigests. Ein nachfolgendes Gate kann diesen konsumieren.
Gate-Evidence speichert Inputreferenzen und Digests ohne Payload-Rohbytes.

Jeder Outputpayload hat höchstens **65536 Bytes (64 KiB)**; 65537 Bytes werden
abgelehnt. Output und Evidence sind ebenfalls je auf 65536 Bytes begrenzt.
Payloadbytes und eingebettete Inputpayloads zählen zum kodierten READY-Receipt
mit `max-receipt-bytes` von 1..262144, kein Gesamtdateibudget. Der allgemeine
Payloadgrenzfall wird im [V3-Kernvertrag](../../tests/unit/phase_dag_v3_tests.cpp)
belegt; die festen CLI-Adapter erzeugen kleine strukturierte Payloads.

Das Bundle beginnt mit `BRUCE_PHASE_DAG_RUN_V3`, anschließend SHA-256 des
kanonischen Bundleinhalts und längenpräfixierte Felder. Es bindet Spec,
Requests einschließlich Kontextbytes, Receipts mit Output-/Evidence-/Payload-
Bytes und Digests, Inputreferenzen einschließlich tatsächlicher Payloadbytes
sowie Trace. Der Bundleinhalt enthält also Rohbytes; die CLI-Reports nicht.
`inspect` und `replay` prüfen alle Digests, Kanonizität und erneut ausgeführte,
bytegleiche Serialisierung ohne externe Dateien, bis höchstens 5 MiB.
V2-Marker `BRUCE_PHASE_DAG_SPEC_V1` und `BRUCE_PHASE_DAG_RUN_V2` bleiben
kompatibel, werden von V3 jedoch abgelehnt. Für V3 gelten nur seine eigenen
Lesebefehle und die getrennte MCP-Route `POST /api/v3/inspect`.
Die V2-Route akzeptiert weiterhin ausschließlich V2-Bundles.

## MCP Inspector

**IMPLEMENTIERT UND BELEGT:** the optional static Mod uses
`BRUCE_BUILD_MOD_MCP=ON` and requires `BRUCE_BUILD_MOD_NANOCELL=ON`.
It serves only `127.0.0.1`; the browser sends a complete binary bundle as the
POST body, without a server-side file path or a JSON wrapper.

The read-only Phase-DAG V2 and Phase-DAG V3 inspector uses fixed NanoCell
catalogs: NanoCell/Evidence-Gate for V2 and NanoCell/Payload-Gate for V3.
`POST /api/v1/inspect` retains its 1 MiB (1048576 bytes) limit and response
model; `POST /api/v3/inspect` accepts V3 bundles up to 5 MiB (5242880 bytes).
These two routes replay existing bundles; no new DAG executions, no jobs or
imports. Wrong-version bundles are rejected. The browser requires explicit
selection of Phase-DAG V2, Phase-DAG V3 or DAG-Scheduler V4, with no automatic
detection from file bytes. Its JSON and DOM expose metadata only;
`request_bytes`, `context_bytes`, `output_bytes`, `evidence_bytes` and
`payload_bytes` are never exposed. There are no adapter plugins.
Replay/digest establishes bundle consistency, not author identity or a general
mathematical proof. The 5-MiB limit does not override tighter core/catalog limits.

Stored V2/V3 bundles include raw requests, contexts and evidence; keep the
files private even though the inspection response hides their contents.
The metadata is a replay-verified view, not a sanitized downloadable bundle.
V3 inputs show `producer_payload_digest`; outputs show `payload_digest`.
`payload_size` is the actual byte count of the verified direct payload.
A digest is not encryption or an author signature. No download route is provided.
See the [route and HTTP contract](commands.md#mcp-inspector).

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


## Kandidaten-CSV für Kompatibilität

```csv
id,domain,layer,x,y,value
1,demo,0,2,6,64
2,demo,0,4,3,64
3,demo,0,1,1,-1
```

| Feld | Typ / Bedeutung |
| --- | --- |
| `id` | Eindeutige UInt64-Kandidatenkennung |
| `domain` | Gemeinsame Domain-ID, keine Zahlendomänenauswahl für GMP |
| `layer` | UInt64; alle Kandidaten müssen dieselbe Schicht verwenden |
| `x`, `y` | UInt64-Koordinaten; nicht pauschal auf das lokale 8×8-Grid begrenzt |
| `value` | Int64-Wert für Regelvergleiche |

1..4096 Kandidaten, höchstens 1048576 Bytes. `z` ist keine CSV-Spalte: Es wird durch die globale Paarungsfunktion berechnet und muss UInt64-genau darstellbar sein. Zeilen werden nach ID sortiert; Reihenfolge im Input löst Gleichrangigkeit nicht auf. Alle Kandidaten gehören zur selben Domain und Schicht.

Bezeichner für Domain, Snapshot, Regeln und Evidenz: 1..64 ASCII-Zeichen aus `A-Z a-z 0-9 _ - .`; der einzelne Bindestrich `-` ist reserviert und kein gültiger Bezeichner. CSV-Felder besitzen kein Quoting und kein Whitespace-Trimmen.

## Regeldatei `BRUCE_RULES_V1`

```text
BRUCE_RULES_V1
snapshot demo-v1
rule prefer positive value eq 64 0 10 manual -
rule exclude reject value lt 0 0 0 manual -
rule context-ok context layer eq 0 0 0 manual -
```

Header und Snapshotzeile sind Pflicht, danach 0..128 Regeln. Bis zu 1048576 Bytes. Zeilen werden in Whitespace-getrennte Wörter zerlegt; keine Kommentare und keine Leerzeilen als Regeln. Jede Regel hat genau zehn Felder:

```text
rule ID KIND FIELD OP ARGUMENT REMAINDER WEIGHT manual EVIDENCE
```

| Feld | Zulässige Werte |
| --- | --- |
| ID | Eindeutiger Regelbezeichner |
| KIND | `positive`, `reject`, `negative`, `context` |
| FIELD | `x`, `y`, `z`, `value`, `layer` |
| OP | `eq`, `ne`, `lt`, `le`, `gt`, `ge`, `mod_eq` |
| ARGUMENT | Int64-Vergleichswert; bei `mod_eq` positiver Modul |
| REMAINDER | 0 für normale Vergleiche; bei `mod_eq` 0..ARGUMENT−1 |
| WEIGHT | −1000000..1000000 für `positive`, sonst exakt 0 |
| Quelle | Ausschließlich `manual` |
| EVIDENCE | Bei `negative` gültiger Bezeichner, sonst exakt `-` |

`positive`: Bei Treffer wird das Gewicht auf den Score addiert, negative Gewichte sind möglich. `reject`: Treffer schließt hart aus. `negative`: Treffer schließt mit einer angegebenen negativen Evidence-ID aus. Die ID selbst wird nicht extern nachgeprüft. `context`: Jeder Kontextvergleich muss zutreffen; ein Nichttreffer macht den Kandidaten unzulässig. Alle Regeln werden in Dateireihenfolge geprüft, solange das Budget reicht.

Für `mod_eq` wird der euklidische Rest verwendet, auch bei negativen Int64-Werten. Nur vollständig geprüfte und kontextpassende Kandidaten ohne Ausschluss sind auswählbar. Höchster Score gewinnt; mehrere gleiche Maximalwerte bleiben `AMBIGUOUS`.

Je 64 sortierte Kandidaten bilden eine Maskengruppe. `complete_mask`, `context_role_mask`, `positive_match_mask`, `hard_reject_mask`, `negative_evidence_mask` und `eligible_mask` haben verschiedene Rollen. Letztere entspricht `complete & context & ~reject & ~negative`; unbenutzte Bits bleiben ausgeschlossen.

## Koeffizientenlisten

`compare` erwartet kommaseparierte rationale Koeffizienten ohne Leerzeichen in aufsteigender Potenzreihenfolge. `-1,0,1` steht für x²−1, `-1,1` für x−1, `1` für die Konstante 1. Jede Liste besitzt mindestens einen Wert und höchstens 65 Einträge. Leere Felder und nachgestellte Kommata sind ungültig. Führende Koeffizienten mit Wert 0 können durch Normalisierung verschwinden; die ursprünglichen Definitionsausschlüsse bleiben fachlich relevant.

## Bundles `.bruce` und Snapshots `.experience`

Die Dateiendung dient Menschen; die Decoderauswahl erfolgt durch den Inhalt. Bundles enthalten einen versionierten Textheader, eine SHA-256-Prüfsumme und einen versionsabhängigen Payload mit Aufgabe, logischen Entscheidungen und Ergebnis. Verschachtelte Inhalte werden teilweise durch Byteanzahlen abgegrenzt. Dateien nicht als CSV oder JSON behandeln und keine Zeilenenden umschreiben.

### Grid-Stage-Evidenz

**IMPLEMENTIERT UND BELEGT:** Nur die feste Regel `scaled-exponential-v1`
erlaubt eine explizite Stage 0..57. Ohne `--stage` gelten weiterhin Blatt-`y`
und die bisherigen kanonischen Bytes einschließlich Cache-Schlüssel, JSON
und Trace; historische skalierte Bundles bleiben replaybar.

| Ablauf | Ohne explizite Stage | Mit expliziter Stage |
| --- | --- | --- |
| Einzel-Grid | `BRUCE_GRID_BUNDLE_V1` | `BRUCE_GRID_BUNDLE_V2` |
| unbegrenzter Batch | `BRUCE_GRID_BATCH_BUNDLE_V1` | `BRUCE_GRID_BATCH_BUNDLE_V3` |
| begrenzter Batch, auch bei `COMPLETED` | `BRUCE_GRID_BATCH_BUNDLE_V2` | `BRUCE_GRID_BATCH_BUNDLE_V4` |
| Resume | `BRUCE_GRID_RESUME_BUNDLE_V1` | `BRUCE_GRID_RESUME_BUNDLE_V2` |

Die Batch-Headerwahl bezeichnet die konfigurierte Ausführungsgrenze,
nicht den Ergebnisstatus; V2/V4 gelten auch bei vollständig bearbeiteten
Pfaden mit Status `COMPLETED`.

Die zugehörige kanonische Spezifikation ist `grid-spec-v2`; Batch speichert
sie ausdrücklich im Feld `spec`. Einzel- und Batch-Payloads tragen unmittelbar
nach `zero_zero` die kanonischen Zeilen `stage_source explicit`, dann `stage N`.
`N` ist die dezimale Stufe ohne führende Nullen (außer `0`). Einzel-Grid setzt
danach mit `TRACE`, Batch mit seinen Cachefeldern fort. Die Spezifikation
bindet die Stage an den Cache-Schlüssel; JSON und Trace enthalten dieselbe
Stage und deren explizite Quelle. Implizite Formate behalten `grid-spec-v1`
und erhalten keine zusätzlichen Stage-Felder.

Resume V2 übernimmt Stage und Cache ausschließlich aus dem replayten Elternlauf.
Es trägt die abgeleiteten Zeilen `stage_source explicit` und `stage N`
unmittelbar nach `policy`. Der Replayer verlangt exakt dieselbe optionale Stage
wie im Elternlauf; eine eigene Resume-Stage-Option existiert nicht. Resume V1
verlangt einen Elternlauf ohne explizite Stage.

Die neuen Header verlangen beide Stage-Felder in kanonischer Reihenfolge.
Fehlende, unbekannte, doppelte oder nichtkanonische Felder werden abgewiesen;
eine korrekte äußere Prüfsumme genügt nicht: Replay rekonstruiert die Ausführung
und vergleicht die vollständige Payload. Der zentrale `replay`-Befehl erkennt
alle Header der Tabelle. Lokale Koordinaten bleiben 0..7; Stage ist keine
Koordinatenerweiterung und keine freie oder physikalische Formel.
Belege: [Grid-Verträge](../../tests/unit/grid_tests.cpp),
[Cache-/Resume-Verträge](../../tests/unit/grid_cache_tests.cpp),
[Bedienung](commands.md#grid--einen-pfad-auswerten).

### Grid-Stage-Sweep-V1-Evidenz

**IMPLEMENTIERT UND BELEGT:** `BRUCE_GRID_STAGE_SWEEP_BUNDLE_V1` ist ein
eigener Header für den festen, zusammenhängenden Sweep `scaled-exponential-v1`
innerhalb `0..57`; die Grid-/Batch-/Resume-Header oben bleiben erhalten.
Auf den Header folgen SHA-256 des Payloads und der kanonische Payload:

```text
spec <gemeinsamer kanonischer GridSpec als JSON>
path <kanonischer lokaler Pfad>
from_stage A
to_stage B
mode exact|metric|modular
max_bits N
modulus M|none
cache_bytes N
debug 0|1
RESULT
<vollständiger kanonischer grid-stage-sweep-v1-JSON-Report>
```

Der gemeinsame Spec trägt die feste Regel und `zero_zero=one`, aber keine
einzelne Stage und daher `grid-spec-v1`. Jede ausgeführte Grid-Abfrage erhält
ihre explizite Stage und damit eine eigene `grid-spec-v2`-Cacheidentität.
Der Report bindet Grenzen, angeforderte/verarbeitete Anzahl, aufsteigende
Records mit Index, Stage, Ergebnis, `cache_key_digest`, Cacheereignis und
Status sowie Cachebudget/-statistik und terminalen Status/Grund/Stage.
Nur `debug 1` speichert zusätzlich die vollständige Grid-Trace pro Record.
Ohne Debug sind diese Traces sowohl aus Records als auch JSON/Payload
ausgeschlossen; Werte, Schlüssel, Ereignisse und Cacheabrechnung bleiben gleich.
Ein Exact-`IntegerBitLimit` bewahrt ausschließlich das erfolgreiche Präfix
mit `INCOMPLETE` und erster fehlgeschlagener `stopped_stage`; auch ein leeres
Präfix ist replaybar. Vollständige Läufe haben `stopped_stage: null`.

Der zentrale `replay`-Befehl prüft Digest und Parameter, rekonstruiert den
vollständigen Sweep mit frischem Cache und vergleicht den gesamten
kanonischen Payload, ohne ursprüngliche Eingabedatei. Neu gehashte Änderungen
an Stage, Wert, Cache-Digest/-Ereignis/-Statistik, Trace oder Endstatus werden
dadurch abgewiesen (`ReplayMismatch` bei ansonsten gültigen Parametern).
V1 hat kein Resume, keine Stage-Listen/-Wiederholungen und keine
Cachepersistenz. Belege: [Sweep-Verträge](../../tests/unit/grid_stage_sweep_tests.cpp),
[CLI-Workflows](../../tests/integration/test_cli.py),
[Bedienung](commands.md#grid-stage-sweep--begrenzte-stufenreihe-mit-diagnose).

### Grid-Stage-Verify-V1-Evidenz

**IMPLEMENTIERT UND BELEGT:** Auf `BRUCE_GRID_STAGE_VERIFY_BUNDLE_V1` und
den SHA-256 des gesamten folgenden Payloads folgt exakt:

```text
formula 100*(2^stage-1)
claim_scope finite-uint64-scaled-exponential-stage-domain-0..57
sweep_digest <SHA-256 der vollständigen ursprünglichen Sweep-Bundlebytes>
sweep_bytes N
SOURCE
<exakt N unveränderte Sweep-Bundlebytes>
RESULT
<vollständiger kanonischer grid-stage-verify-v1-JSON-Report>
```

Zwischen den N Quellbytes und `RESULT` steht ein zusätzliches LF; der
Payload endet ebenfalls mit LF. Die Bytezahl, nicht eine Suche nach
Markern innerhalb der Quelle, begrenzt die eingebetteten Bytes. Quelle,
Quellhash, feste Formel, endlicher Scope, Modus, alle 58 Vergleiche und
Endstatus sind dadurch gebunden. `expected`/`observed` sind Dezimalstrings:
Exact-Werte oder Metric-Bitlängen. Bei Erfolg sind `verified_domain` gleich
`"0..57"`, `checked_entries` gleich 58 und `first_mismatch` null.

Replay prüft den äußeren Digest, rekonstruiert die komplette eingebettete
Sweep-V1-Quelle, verlangt den vollständigen Exact-/Metric-Bereich `0..57`,
berechnet die Vergleiche erneut und vergleicht den gesamten kanonischen
Verify-Payload. Es benötigt keine ursprüngliche Quelldatei. Neu gehashte
Änderungen an Quelle, Quellhash, Formel, Scope, Vergleich oder Status werden
abgewiesen; semantische Abweichungen ergeben `ReplayMismatch`.
Ein beschädigter äußerer Digest ergibt `InvalidEvidence`.

`REFUTED` ist defensiv mit `verified_domain: null`, allen Vergleichen und
dem ersten abweichenden Record unter `first_mismatch` darstellbar. Ein
erfolgreicher Refutationslauf samt Replay ist **IMPLEMENTIERT, ABER NICHT
AKTUELL VERIFIZIERT**: Gültige heutige Sweep-V1-Quellen regenerieren bereits
dieselbe feste Regel und können diesen Zweig nicht erreichen. Ein künstlich
veränderter Lauf wird beim kanonischen Replay abgewiesen. Digest und Replay
belegen Konsistenz im endlichen Vertrag, weder Autoridentität noch einen
allgemeinen mathematischen Beweis. Bestehende Grid-/Sweep-/Batch-/Resume-
Formate werden durch diesen eigenen Header nicht ersetzt.
Belege: [Verify-Verträge](../../tests/unit/grid_stage_verify_tests.cpp),
[CLI-Workflows](../../tests/integration/test_cli.py) und
[Bedienung](commands.md#grid-stage-verify--feste-regel-auf-der-endlichen-stage-domäne).

Allgemeine Bundle-/Dateigrenze: 33554432 Bytes (32 MiB). Spezifische Importe können enger sein: CSV/Regeln/Kandidaten/Erfahrung 1 MiB, Grid-Forschungsquelle 2 MiB. Eine gültige große Datei kann deshalb bei einem engeren Import trotzdem abgelehnt werden.

Erfahrungssnapshots enthalten versionierte Ereignisketten, eingebettete Quellnachweise und Korrekturbeziehungen. Mit `experience inspect` prüfen, nicht mit `replay`. Bundles mit `replay` oder `inspect-run` prüfen. Eine passende Prüfsumme allein ersetzt nicht den semantischen Replay.

`--output` schreibt über eine private temporäre Datei im Zielverzeichnis und veröffentlicht ohne Überschreiben eines vorhandenen Zielnamens. Fehler beim Erstellen, Schreiben, Synchronisieren oder Veröffentlichen führen zu Exit 2. Existierende Dateien werden nicht überschrieben. Verzeichnisse werden nicht automatisch erzeugt. Die Implementierung synchronisiert die Datei; daraus folgt keine allgemeine Zusage für sämtliche Dateisystem-/Stromausfallszenarien. Bei `ExportCommitDurabilityUnknown` ist der Zielname bereits verlinkt; sein Inhalt kann sichtbar sein, obwohl die Verzeichnissynchronisation scheiterte. [Details](local-files.md#fehler-und-aufräumen).

## Regelvorschläge und bestätigte Exporte

`BRUCE_RULE_PROPOSAL_V1` enthält zwei kanonische Regelfassungen in einem eigenen gequoteten Textformat mit äußerem Digest. Es ist weder JSON noch ein mathematisches Replaybundle. Der bestätigte Export ist wieder eine gewöhnliche `BRUCE_RULES_V1`-Datei. [Byteaufbau, Grenzen und Digestbindung](rule-review.md#paketformat-und-digestbindung).

## Aktive Regelspeicher

`BRUCE_RULE_STAGE_V1` und `BRUCE_RULE_HISTORY_V1` binden Vorschläge und bestätigte Wechsel an Speicherpfad, Generation und Journalverlauf. Die Historie wird durch `rules-activate` nach erneuter Prüfung unter Schreibsperre atomar ersetzt; sie ist kein exklusiver Bundleexport. [Byteformat, Grenzen und Schreibvertrag](active-rules.md#speicherformat-und-schreibvertrag).

## Erfahrungsübergabepläne

`BRUCE_EXPERIENCE_ADD_TRANSFER_V1` bindet Quelldigest, Basisdigest und zwei Bytegrenzen in einem SHA-256-umschlossenen kanonischen Textformat. Der Plan enthält keine Eingabebundles. [Exaktes Format](experience-transfer.md#planformat).

## Z-Auswahlpläne

`BRUCE_COMPATIBILITY_TRANSFER_V1` bindet Quelldigest, kanonischen Regeldigest, Auswertungsstelle und Budgets in einem SHA-256-umschlossenen Textkörper. Der Plan enthält weder Quelle noch Regeln. [Exaktes Format](compatibility-transfer.md#planformat).
