# Artifact Store V1.2 — kontrollierte Referenzauflösung

**Status:** IMPLEMENTIERT UND BELEGT — der lokale metadata-only Resolve-Vertrag
ist implementiert und die Umsetzung abgeschlossen. Grundlage sind die
[CLI](../../../apps/artifact_store_v1_cli.hpp), die
[Core-API](../../../include/bruce/artifact/artifact_store_v1.hpp), die unten
verlinkten Tests sowie Abschlussbericht (archivaler lokaler, nicht versionierter Pfad:
`runs/bruce-artifact-store-resolve-v1-2026-10-15.md`) und Final-Review (PASS;
archivaler lokaler, nicht versionierter Pfad:
`runs/bruce-artifact-store-resolve-v1/final-review.md`).
Die Abnahmegrenzen stehen im Abschnitt „Belegstatus“; diese Spezifikation
allein ist kein Laufzeitbeleg.

## Ziel

Artifact Store V1.2 ergänzt den bestehenden lokalen Artifact Store um einen
separaten, read-only Auflösungsschritt für ein bereits veröffentlichtes
Manifest. Die Route liest das kanonische Manifest, prüft jedes darin
referenzierte content-addressed Objekt vollständig auf Digest und exakte
Bytegröße und gibt anschließend ausschließlich kontrollierte Metadaten aus.

Der Vertrag macht aus einer opaken Manifest-ID eine überprüfbare, aber
weiterhin payloadfreie Referenz:

```text
Manifest-ID
    |
    v
kanonisches BRUCE_ARTIFACT_MANIFEST_V1
    |
    v
Digest-/Größenbindung je Objekt
    |
    v
vollständige lokale Integritätsprüfung
    |
    v
metadata-only JSON
```

Die Auflösung ist kein Lese- oder Download-Endpunkt. Bruce darf die Bytes
intern zur Integritätsprüfung streamen, aber weder stdout/stderr noch ein
anderer öffentlicher Vertrag darf Payloadbytes oder private Dateipfade
ausgeben.

## Bestehende Belege und Anschlussgrenzen

**IMPLEMENTIERT UND BELEGT:** Artifact Store V1 besitzt einen lokalen,
unveränderlichen content-addressed Objektbestand. Die öffentliche C++-API
bietet `read_manifest`, `inspect` und `verify`; Digest und Größe werden gegen
die gespeicherte reguläre Datei geprüft.

Belege:

- `include/bruce/artifact/artifact_store_v1.hpp` —
  `ArtifactStoreV1::read_manifest`, `inspect` und `verify`;
- `src/artifact/artifact_store_v1.cpp` — descriptor-relative, bounded
  Manifest- und Objektzugriffe mit `O_NOFOLLOW`, Owner-only-Prüfungen und
  SHA-256-Verifikation;
- `include/bruce/artifact/artifact_manifest_v1.hpp` und
  `src/artifact/artifact_manifest_v1.cpp` — kanonisches Manifestformat und
  Token-/Digest-/Größenvalidierung;
- `apps/artifact_store_v1_cli.hpp` — bestehende V1-Routen `import`, `inspect`
  und `verify`;
- `tests/unit/artifact_store_v1_tests.cpp`,
  `tests/unit/artifact_store_v1_cli_tests.cpp` und
  `tests/integration/test_artifact_store_v1.py` — belegte V1-Verträge.

**IMPLEMENTIERT UND BELEGT:** V5 kann über den getrennten Publish-Schritt

```text
bruce dag-scheduler-v5 publish BUNDLE --store STORE
```

für jeden akzeptierten Knoten ein `BRUCE_ARTIFACT_MANIFEST_V1` mit Run-,
Node-, Attempt-, Status-, Result-Code-, Digest-, Größen- und Origin-Metadaten
veröffentlichen. Der Publish-Schritt verändert weder den Execute-Bericht noch
den privaten Run-Baum.

Belege:

- `apps/dag_scheduler_v5_cli.hpp`;
- `docs/superpowers/specs/2026-10-15-worker-artifact-publish-v1-design.md`;
- `docs/cli/commands.md` und `docs/cli/formats.md`;
- `tests/integration/test_dag_scheduler_v5_publish.py`.

**IMPLEMENTIERT UND BELEGT:** V1.2 ergänzt die lokale metadata-only
Manifestauflösung über `ArtifactStoreV1::resolve_manifest()` und die separate
CLI-Route `artifact-store-v1 resolve`. Die CLI öffnet ausschließlich einen
bestehenden Store über `open_existing()`; auch bei fehlendem oder
unvollständigem Store werden keine Dateien oder Verzeichnisse angelegt.

**DOKUMENTIERT / KONZEPT:** Payload-Reader, Materialisierung in neue
DAG-Eingaben, automatische Retention/GC sowie Netzwerk- oder externe Provider
bleiben außerhalb dieses Vertrags.

## Öffentliche CLI-Schnittstelle

Die neue Route lautet exakt:

```text
bruce artifact-store-v1 resolve MANIFEST_ID --store STORE
```

Dabei ist:

- `MANIFEST_ID` eine einzelne, opake Manifest-ID nach der bereits geltenden
  ASCII-Grammatik des Artifact Store;
- `STORE` der lokale Root eines bestehenden Artifact Store V1;
- `--store` die einzige Option dieser Route.

Die Route akzeptiert keine Datei, keinen Digest als Objekt-Substitution,
keinen alternativen Manifestpfad, keine URL, keinen Provider, keinen
Download-Schalter, keine Ausgabeoption, keine Materialisierungsoption und
keine V5- oder Worker-Option. `MANIFEST_ID` wird nicht als Dateipfad
interpretiert; die konkrete Datei wird descriptor-relativ als
`manifests/<MANIFEST_ID>.manifest` geöffnet.

Die bestehende Syntax bleibt unverändert:

```text
bruce artifact-store-v1 import FILE --store STORE
bruce artifact-store-v1 inspect DIGEST --store STORE
bruce artifact-store-v1 verify DIGEST --store STORE
```

V1.2 ergänzt ausschließlich `resolve`; es ändert weder Feldreihenfolge noch
Exit-Verhalten der drei bestehenden Routen.

## Erfolgsvertrag

Bei einem erfolgreichen Resolve schreibt die Route genau ein kanonisches
JSON-Objekt mit abschließendem LF auf stdout, nichts auf stderr und Exit 0.
Der implementierte Vertrag heißt `artifact-store-resolve-v1`. Die feste
Feldreihenfolge ist:

```json
{"contract":"artifact-store-resolve-v1","action":"resolve","status":"RESOLVED","manifest_id":"v5-abc123","run_id":"run-123","node_id":"a","attempt_id":"a-1","manifest_status":"READY","result_code":"OK","objects":[{"sha256":"<64-hex>","size_bytes":12,"origin":"artifact/a"}]}
```

Die Felder bedeuten:

| Feld | Bedeutung |
| --- | --- |
| `contract` | exakt `artifact-store-resolve-v1` |
| `action` | exakt `resolve` |
| `status` | Ergebnis der Route, bei Erfolg exakt `RESOLVED` |
| `manifest_id` | die geprüfte opake Manifest-ID |
| `run_id` | `run_id` aus dem kanonischen Manifest |
| `node_id` | `node_id` aus dem kanonischen Manifest |
| `attempt_id` | `attempt_id` aus dem kanonischen Manifest |
| `manifest_status` | der unveränderte `status`-Wert des Manifests, z. B. `READY` |
| `result_code` | der unveränderte kontrollierte Result-Code des Manifests |
| `objects` | kanonische Liste der geprüften Manifestobjekte |

Jedes Element in `objects` enthält exakt:

```json
{"sha256":"<64-hex>","size_bytes":12,"origin":"artifact/a"}
```

Die Objektliste folgt der Reihenfolge im Manifest. `sha256` und `size_bytes`
werden aus dem Manifest übernommen und danach gegen das tatsächlich
gespeicherte Objekt geprüft. `origin` wird als bereits validiertes kontrolliertes
Token ausgegeben; absolute Pfade, `..`, Symlinkziele und freie Textfelder sind
kein zulässiger Origin-Wert. Ein leerer `origin` bleibt als leerer JSON-String
erhalten. `media_type` ist für V1.2 nicht Teil der öffentlichen Ausgabe, weil
die Route nur die angeforderten Auflösungs- und Integritätsmetadaten
bereitstellt.

Die Auflösung eines gültigen Manifests mit null Objekten ist zulässig. In
diesem Fall lautet `objects` exakt `[]`; es gibt keine implizite Quelle und
keine Materialisierung.

## Auflösungs- und Prüfablauf

Die Implementierung muss die Schritte in dieser Reihenfolge ausführen:

1. CLI-Argumente und `MANIFEST_ID` werden mit der bestehenden ASCII-
   Manifest-ID-Grammatik geprüft. Ungültige oder zusätzliche Argumente werden
   vor jedem Store-Zugriff abgewiesen.
2. Der lokale Store-Root wird über `open_existing()` mit den bestehenden
   Root-, Owner-only-, Verzeichnis-, Symlink- und descriptor-relative Regeln
   ohne Erzeugungsflags geöffnet. Lock-Datei und Store-Unterverzeichnisse
   müssen bereits vorhanden sein.
3. `read_manifest(MANIFEST_ID)` liest die gebundene Manifestdatei bounded,
   prüft Vertrag, feste Feldreihenfolge, abschließendes LF, Token, Objektanzahl,
   Digest, Größe, Origin und kanonische Re-Serialisierung.
4. Für jedes Manifestobjekt wird eine `ArtifactIdV1` aus exakt dessen
   `sha256` und `size_bytes` gebildet.
5. Jedes gebundene Objekt wird über die bestehende Digest-/Größenprüfung
   vollständig verifiziert. Dabei müssen regulärer Dateityp, Owner-only-
   Rechte, Single-Hardlink, maximale Objektgröße und SHA-256 stimmen.
6. Erst nach erfolgreicher Prüfung aller Objekte wird der metadata-only
   Erfolgsbericht in der Manifestreihenfolge gerendert.

Die Route muss bei einem Fehler in Schritt 3, 4 oder 5 den gesamten Bericht
verwerfen. Sie darf keinen Teilbericht und keine als erfolgreich markierte
Referenz ausgeben. Die Prüfung ist read-only: Es werden keine Objekte,
Manifeste oder temporären Dateien erzeugt, geändert, verschoben oder gelöscht.

## Integritätssemantik

V1.2 betrachtet ein Manifest nur dann als aufgelöst, wenn alle darin
referenzierten Objekte aktuell vorhanden und bytegenau verifiziert sind.

| Zustand | Ergebnis |
| --- | --- |
| Manifest fehlt | `PROTOCOL_ERROR` / Exit 2 |
| Manifest-ID syntaktisch ungültig | `PROTOCOL_ERROR` / Exit 2 |
| Manifest nicht kanonisch, abgeschnitten oder zu groß | `PROTOCOL_ERROR` / Exit 2 |
| Objekt fehlt | `PROTOCOL_ERROR` / Exit 2 |
| Digest oder Größe weichen ab | `PROTOCOL_ERROR` / Exit 2 |
| Objekt ist Symlink, Mehrfach-Hardlink, nichtregulär oder unsicher berechtigt | `PROTOCOL_ERROR` / Exit 2 |
| irgendein Objekt ist korrupt | `PROTOCOL_ERROR` / Exit 2 |
| alle Objekte und Manifestfelder sind gültig | metadata-only Bericht / Exit 0 |

Es gibt in V1.2 keinen partiellen `RESOLVED`-Status, keinen Warnstatus und
keine erfolgreiche Antwort mit `objects`-Einträgen, die nicht vollständig
verifiziert wurden. Die Integritätsprüfung bestätigt lokale Byteidentität
gegen den Digest; sie authentifiziert weder Herkunft, Benutzer, Signatur noch
Berechtigung.

## Fehlervertrag und Geheimnisgrenze

Jede Syntax-, Store-, Manifest-, Rechte-, Pfad-, Symlink-, Hardlink-,
Größen-, Dekodierungs- oder Integritätsverletzung schreibt kein stdout und
genau diesen Fehler auf stderr, gefolgt von LF:

```json
{"contract":"artifact-store-resolve-v1","error":"PROTOCOL_ERROR"}
```

Der Exit-Code ist 2. Die öffentliche Antwort darf keine Store- oder
Manifestpfade, Betriebssystemtexte, `errno`-Werte, Stackdaten, Payloadbytes,
Kommandozeilen, PIDs, Zeitstempel, Benutzer- oder Geheimnisdaten enthalten.
Interne Diagnosen dürfen für lokale Entwicklerprotokolle klassifiziert werden,
gehören aber nicht in diesen CLI-Vertrag.

## Sicherheits- und Ressourcenmodell

Die Route übernimmt die bestehenden V1-Grenzen:

- Manifest- und Objektgrößen bleiben bounded und überschreiten nicht die
  vorhandene `max_file_bytes`-/32-MiB-Grenze;
- Manifest-ID, Digest, Origin und alle übrigen Tokens werden mit expliziter
  ASCII-Grammatik geprüft;
- Manifest- und Objektpfade werden aus validierten Einzelkomponenten
  descriptor-relativ aufgebaut;
- `O_NOFOLLOW`, reguläre Datei, Owner-only-Rechte und genau ein Hardlink
  bleiben zwingende Prüfungen;
- keine externe Datei wird geöffnet, nur weil sie im Manifest als freie
  Zeichenkette erscheint;
- die Route besitzt keinen Netzwerkzugriff und startet keinen Prozess;
- die Route hält keine Payload im öffentlichen Ergebnis und schreibt keinen
  Cache oder Index.

Die vollständige SHA-256-Prüfung verursacht lokale Lese-I/O proportional zur
Objektgröße. V1.2 definiert dafür keinen neuen Zeit-, CPU- oder Speicher-
Budgetparameter; die bestehende bounded Store-Prüfung ist die maßgebliche
Grenze. Ob später ein separates Diagnosebudget erforderlich ist, bleibt eine
offene Entscheidung.

## Verhältnis zu V5 und späterem DAG-Payloadfluss

V1.2 liest nur bereits veröffentlichte Store-Manifeste. Es:

- verändert `dag-scheduler-v5 execute`, `inspect` oder `publish` nicht;
- schreibt nicht in `BUNDLE`, `BUNDLE.run/` oder `control/run-log.jsonl`;
- startet keinen Worker, Adapter, Replay- oder Recovery-Pfad;
- erzeugt kein neues V5-Receipt und ändert keine V5-Ausgabe;
- wandelt keine Artifact-Referenz in eine neue DAG-Eingabedatei um;
- gibt keinen Objektpfad und keine Payloadbytes an einen Aufrufer weiter.

Ein späterer kontrollierter Materialisierungs- oder Payloadfluss müsste einen
eigenen versionierten Vertrag mit expliziten Zielpfad-, Größen-, Rechte-,
Provenienz- und Überschreibungsregeln erhalten. V1.2 ist dafür nur eine
Integritäts- und Metadatenbasis. :codex-annotation{index="1"}

## Tests und Abnahme

Die folgenden Abnahmeanforderungen beschreiben den Resolve-Vertrag. Die
implementierten Core-, CLI- und Integrationstests sowie der tatsächlich
aufgezeichnete Prüfumfang sind unten als Belege verlinkt:

### CLI-Erfolg

- gültiges V1-Manifest mit einem Objekt liefert exakt den kanonischen
  `artifact-store-resolve-v1`-Bericht;
- mehrere Objekte bleiben in Manifestreihenfolge;
- null Objekte liefert `objects:[]`;
- Run-, Node-, Attempt-, Manifest-Status-, Result-Code-, Digest-, Größen- und
  Origin-Felder werden korrekt und ohne private Pfade ausgegeben;
- wiederholtes Resolve ist byteidentisch und verändert den Store nicht.

### Integritätsfehler

- fehlende Manifestdatei;
- ungültige, zusätzliche oder als Pfad interpretierte Manifest-ID;
- abgeschnittenes, nicht kanonisches oder übergroßes Manifest;
- fehlendes Objekt;
- manipulierte Objektbytes;
- Digest-/Größenabweichung;
- Symlink, zusätzlicher Hardlink, nichtreguläre Datei oder unsichere Rechte;
- beschädigte Store-Struktur;
- Fehler in einem späteren Objekt verwirft den gesamten stdout-Bericht.

### Geheimnis- und Kompatibilitätsprüfungen

- stdout und stderr enthalten keine Payloadbytes und keine absoluten Pfade;
- Fehler sind exakt `PROTOCOL_ERROR` mit Exit 2;
- bestehende `import`, `inspect` und `verify`-Ausgaben bleiben bytegleich;
- V5 Execute-, Inspect- und Publish-Tests bleiben unverändert erfolgreich;
- ein gespeichertes Manifest und seine Objekte werden durch Resolve nur gelesen;
- keine Datei außerhalb des bestehenden Store-Roots wird geöffnet.

## Nicht Bestandteil von V1.2

- Payloadausgabe, `cat`, Download oder Streaming an stdout;
- Kopieren, Materialisieren oder Exportieren eines Objekts in eine Datei;
- Verwendung eines aufgelösten Objekts als V5- oder DAG-Eingabe;
- automatische Veröffentlichung von V5-Läufen;
- Retention, Referenzzählung, Indexierung oder Garbage-Collection;
- Löschen, Reparieren, Überschreiben oder Neuveröffentlichen;
- Netzwerk, S3, Redis, Datenbank oder externer Provider;
- Authentifizierung, Autorisierung, Verschlüsselung oder Signaturen;
- freie Code-Plugins, GPU-/Netzwerkausführung, freie Sprache, allgemeines
  Beweisen oder übergreifende Runtime-Orchestrierung.

## Festgelegte Implementierungsentscheidungen

Die fünf Planungsdetails sind im implementierten Vertrag festgelegt:

1. **Manifeststatus:** `manifest_status` bleibt vom äußeren Erfolgsstatus
   `RESOLVED` getrennt.
2. **Leerer `origin`:** Ausgabe als leerer String; `media_type` wird nicht
   ausgegeben.
3. **Fehlerhafter Store-Root:** fehlender, unsicherer oder beschädigter Root
   liefert dieselbe kompakte `artifact-store-resolve-v1`-Fehlerhülle. Resolve
   erzeugt weder Root noch Lock-Datei oder Unterverzeichnisse.
4. **Objektreihenfolge:** unveränderte Reihenfolge des kanonischen Manifests,
   ohne zusätzliche Sortierung.
5. **Diagnosebudget:** keine neue CLI-Option in V1.2; die bestehenden bounded
   Store-Grenzen bleiben maßgeblich. Ein späteres Zeit- oder I/O-Budget bleibt
   eine **OFFENE ENTSCHEIDUNG** für einen eigenen Vertrag.

## Belegstatus

- **IMPLEMENTIERT UND BELEGT:** die Manifest-, Objekt- und
  Integritätsprimitive des Artifact Store V1 sowie `resolve_manifest()`,
  `open_existing()`, die Route `artifact-store-v1 resolve`, ihr
  Erfolgs-/Fehlerbericht und die vollständige read-only Auflösungskette.
- **IMPLEMENTIERT UND BELEGT:** der Final-Review (archivaler lokaler, nicht
  versionierter Pfad: `runs/bruce-artifact-store-resolve-v1/final-review.md`)
  meldet PASS nach dem Read-only-Fix und einem frischen Build mit **11/11**
  erfolgreichen fokussierten Tests für Resolve, bestehende Store-Verträge,
  Worker-Log/Inspector und V5-Kompatibilität.
- Der Abschlussbericht (archivaler lokaler, nicht versionierter Pfad:
  `runs/bruce-artifact-store-resolve-v1-2026-10-15.md`)
  dokumentiert außerdem **20** erfolgreiche CLI-Dokumentationstests und die
  Prüfung der generierten CLI-Beispiele ausschließlich in einer temporären
  Repositorykopie. Die vollständige CTest-Abnahme **vor** dem Read-only-Fix
  ergab **183/184**; `cockpit_inspector_workflows` scheiterte an der bekannten
  Loopback-Socket-Sandboxgrenze. Ein vollständiger erfolgreicher Lauf nach
  dem Fix ist **nicht belegt**.
- Nutzbare Git-Metadaten fehlen in dieser Arbeitskopie. Ein sauberer Git-Diff,
  ein sauberer Git-Status oder ein Commit werden nicht behauptet.
- **DOKUMENTIERT / KONZEPT:** Payload-Reader, Materialisierung, automatischer
  DAG-Payloadfluss, Retention/GC und Netzwerk-/externe Provider.

## Implementierungs- und Testbelege

- [Artifact Store API: Resulttypen, ID-Prüfung, open_existing und resolve_manifest](../../../include/bruce/artifact/artifact_store_v1.hpp)
- [Resolve-Core und read-only Store-Zugriff](../../../src/artifact/artifact_store_v1.cpp)
- [CLI-Parser, Renderer und Fehlervertrag](../../../apps/artifact_store_v1_cli.hpp)
- [Core-Verträge](../../../tests/unit/artifact_store_v1_tests.cpp)
- [CLI-Verträge](../../../tests/unit/artifact_store_v1_cli_tests.cpp)
- [Real-Binary-Resolve-Workflow](../../../tests/integration/test_artifact_store_resolve_v1.py)
- [CTest-Registrierung](../../../CMakeLists.txt)
- [Dokumentationsverträge](../../../tests/integration/test_cli_docs.py)
- Task-1-Bericht einschließlich Read-only-Fix (archivaler lokaler, nicht
  versionierter Pfad: `runs/bruce-artifact-store-resolve-v1/task-1-report.md`)
- Task-2-Bericht (archivaler lokaler, nicht versionierter Pfad:
  `runs/bruce-artifact-store-resolve-v1/task-2-report.md`)
- Task-2-Review (archivaler lokaler, nicht versionierter Pfad:
  `runs/bruce-artifact-store-resolve-v1/task-2-review.md`)
- Abschlussbericht (archivaler lokaler, nicht versionierter Pfad:
  `runs/bruce-artifact-store-resolve-v1-2026-10-15.md`)
- Final-Review (archivaler lokaler, nicht versionierter Pfad:
  `runs/bruce-artifact-store-resolve-v1/final-review.md`)

## Referenzen

- [Artifact Store V1 Design](2026-10-15-artifact-store-v1-design.md)
- [V5 Artifact-Store-Importbrücke V1.1](2026-10-15-worker-artifact-publish-v1-design.md)
- [Artifact Store API](../../../include/bruce/artifact/artifact_store_v1.hpp)
- [Manifest API](../../../include/bruce/artifact/artifact_manifest_v1.hpp)
- [Artifact Store CLI](../../../apps/artifact_store_v1_cli.hpp)
- [CLI-Befehle](../../cli/commands.md)
- [CLI-Formate](../../cli/formats.md)
