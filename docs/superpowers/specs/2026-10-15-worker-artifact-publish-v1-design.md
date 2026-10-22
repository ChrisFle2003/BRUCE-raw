# V5 Artifact-Store-Importbrücke V1.1 — Architektur-Spezifikation

**Status:** DOKUMENTIERT / KONZEPT — Architekturentwurf vor der
Implementierungsplanung. Es wurde in diesem Schritt kein Quellcode geändert.

## Ziel

Die V5-Artifact-Store-Importbrücke ergänzt den bestehenden lokalen
`dag-scheduler-v5`-Lauf um einen ausdrücklich getrennten, read-only geprüften
Publikationsschritt. Ein bereits vollständig beendeter und vom Inspector als
gültig bestätigter Lauf darf seine akzeptierten Worker-Ergebnisse in den
lokalen Artifact Store V1 übernehmen und dazu kanonische Metadaten-Manifeste
veröffentlichen.

Die Brücke verbindet damit drei bereits vorhandene Grenzen:

```text
BUNDLE + BUNDLE.run/
        |
        v
V5-Run-Inspector (read-only, gebundener Seitensatz)
        |
        v
nur bestätigte ARTIFACT_ACCEPTED-Ergebnisse
        |
        v
ArtifactStoreV1::put_file (Digest-/Größenbindung)
        |
        v
BRUCE_ARTIFACT_MANIFEST_V1 (atomar, exklusiv, idempotent)
```

Die Brücke führt keinen Worker erneut aus, ruft keinen Adapter auf, setzt
keinen unvollständigen Lauf fort und ersetzt keine V5-Ausgabe. Sie ist ein
separater Importbefehl mit eigener Versionierung.

## Bestehende Belege und Anschlussgrenzen

**IMPLEMENTIERT UND BELEGT:** V5 legt den öffentlichen Execute-Bericht und den
privaten Seitensatz `BUNDLE.run/` an. Im Seitensatz befinden sich unter anderem
`inputs/`, `attempts/<attempt-id>/output/result.bin` und
`control/run-log.jsonl`. Akzeptierte Ergebnisse werden im Run-Log als
`ARTIFACT_ACCEPTED` mit kontrollierter relativer Referenz, SHA-256-Digest und
Bytegröße gebunden.

Belege:

- `apps/dag_scheduler_v5_cli.hpp` — Run-Erzeugung, private Attempt-Dateien,
  `ARTIFACT_ACCEPTED` und die unveränderte Execute-Ausgabe;
- `include/bruce/worker/worker_run_log_v1.hpp` und
  `src/worker/worker_run_log_v1.cpp` — append-only JSONL-Vertrag,
  Referenz-/Digest-/Größenprüfung und kontrollierte Ereignisse;
- `include/bruce/worker/worker_run_inspector_v1.hpp` und
  `src/worker/worker_run_inspector_v1.cpp` — read-only Laufprüfung;
- `tests/integration/test_dag_scheduler_v5_inspector.py` — vollständige,
  unvollständige und manipulierte Laufzustände;
- `docs/cli/commands.md` und `docs/cli/formats.md` — öffentliche V5- und
  Inspector-Verträge.

**IMPLEMENTIERT UND BELEGT:** Artifact Store V1 publiziert lokale immutable,
content-addressed Objekte über SHA-256 und exakte Größe. `put_file` prüft die
Quelle bounded und streamt Digest/Kopie; `publish_manifest` schreibt den
kanonischen `BRUCE_ARTIFACT_MANIFEST_V1`-Datensatz exklusiv und idempotent.

Belege:

- `include/bruce/artifact/artifact_store_v1.hpp`;
- `include/bruce/artifact/artifact_manifest_v1.hpp`;
- `src/artifact/artifact_store_v1.cpp` und
  `src/artifact/artifact_manifest_v1.cpp`;
- `apps/artifact_store_v1_cli.hpp`;
- `tests/unit/artifact_store_v1_tests.cpp`,
  `tests/unit/artifact_store_v1_cli_tests.cpp` und
  `tests/integration/test_artifact_store_v1.py`.

**DOKUMENTIERT / KONZEPT:** V5 publiziert aktuell nicht automatisch in den
Artifact Store. Der aktuelle Store hat keinen Download-Endpunkt, keine
Netzwerkprovider und keine GC-/Retention-Funktion. Diese Spezifikation führt
keine dieser Funktionen ein.

## Öffentliche CLI-Schnittstelle

Die neue Route lautet exakt:

```text
bruce dag-scheduler-v5 publish BUNDLE --store STORE
```

Dabei ist:

- `BUNDLE` die von `dag-scheduler-v5 execute ... --output BUNDLE`
  veröffentlichte Execute-Datei;
- `BUNDLE.run/` der ausschließlich dazugehörige private Seitensatz;
- `STORE` der lokale Root eines Artifact Store V1.

Die Route akzeptiert keine weiteren Optionen, keinen alternativen Run-Root,
keine Spec-Datei, keinen Workerpfad, keine Adapterauswahl, keine URL und
keinen Provider. `BUNDLE.run` darf nicht als BUNDLE-Argument angegeben werden.
Lexikalisch äquivalente relative Schreibweisen werden nach den bestehenden
V5-Regeln normalisiert; Root-Escape, Symlink- und Mehrfach-Hardlinkpfade
werden abgewiesen.

### Erfolgsbericht

Bei Erfolg schreibt die Route genau einen metadata-only JSON-Bericht mit
abschließendem LF auf stdout, leeres stderr und Exit 0. Der neue Vertrag wird
`dag-scheduler-v5-publish-v1` genannt. Der Bericht muss kanonisch und
stabil bleiben; ein möglicher V1.1-Aufbau ist:

```json
{"contract":"dag-scheduler-v5-publish-v1","action":"publish",
 "status":"PUBLISHED","run_id":"run-123","manifests":[
 {"node_id":"a","attempt_id":"a-1","reference":"artifact/a",
  "manifest_id":"v5-<digest>","sha256":"<64-hex>","size_bytes":12}
 ]}
```

Die endgültige Feldreihenfolge und die Frage, ob `dag_id` zusätzlich in den
Bericht aufgenommen wird, sind in der Implementierungsplanung als
Kompatibilitätsentscheidung festzuschreiben. Der Bericht darf keine
Payloadbytes, absolute Pfade, private `attempts/...`-Referenzen,
Kommandozeilen, PIDs, Stderr oder Betriebssystemfehler enthalten.

Ein Lauf mit mehreren akzeptierten Knoten liefert einen Eintrag je
akzeptiertem Knoten in der kanonischen V5-Spec-Reihenfolge. Die Manifest-IDs
sind deterministisch aus einer kanonischen Bindung von Run-, Knoten- und
Attempt-Identität abzuleiten, beispielsweise als `v5-` plus SHA-256 dieser
Bindung. Die konkrete ID-Kanonisierung muss vor Implementierungsbeginn
festgeschrieben werden; sie darf keine Pfade oder frei übernommene Texte
enthalten.

### Fehlerbericht

Syntax-, Bindungs-, Integritäts-, Store-, Rechte-, Symlink-, Hardlink-,
Größen- und Manifestkonflikte liefern kein stdout, einen stabilen kompakten
Fehler auf stderr und Exit 2. Für V5-Kompatibilität ist der öffentliche
Fehlerkörper:

```json
{"error":"PROTOCOL_ERROR"}
```

Interne Fehlerursachen dürfen für lokale Diagnosen klassifiziert werden,
dürfen aber nicht als Pfad, OS-Text, Stackinhalt oder Payloadinformation in
der öffentlichen Antwort erscheinen. Ein als `INCOMPLETE`, `FAILED`,
`BLOCKED` oder `CANCELLED` beendeter Lauf ist für diesen Publikationsvertrag
kein erfolgreicher Import und wird kontrolliert abgewiesen.

## Inspector-first-Vertrag

Die Publikationsroute muss die Prüfung in dieser Reihenfolge durchführen:

1. CLI-Argumente, BUNDLE-Bindung und STORE-Root werden kanonisch validiert.
2. Der bestehende V5-Inspector wird auf dem unveränderten BUNDLE und dem
   gebundenen `BUNDLE.run/` ausgeführt bzw. über eine gemeinsame interne
   Prüfkomponente aufgerufen. Die öffentliche `inspect`-JSON-Ausgabe ist dafür
   nicht als unvalidierte Datenquelle zu behandeln.
3. Der Inspector muss einen vollständigen Lauf mit `RUN_FINISHED` und
   `status=VALID` feststellen. Zusätzlich muss der beobachtete Run-Status
   `READY` sein.
4. Für jeden zu importierenden Knoten müssen Run-Log und Execute-Bericht ein
   konsistentes `ARTIFACT_ACCEPTED`-Ereignis binden. Das Ergebnis muss
   `READY`/`accepted=true` sein, `artifact_verified=true` besitzen und eine
   identische relative Referenz, SHA-256-Prüfsumme und Bytegröße in allen
   geprüften Quellen aufweisen.
5. Der Inspector liefert intern eine bereits validierte, node- und
   attemptgebundene Importliste. Die Brücke darf private Pfade nicht aus dem
   öffentlichen Bericht rekonstruieren oder ungeprüft aus CLI-Text
   zusammensetzen.
6. Erst nach vollständiger Laufprüfung wird jeder Quellpfad descriptor-relativ
   innerhalb des privaten Run-Roots mit den bestehenden `O_NOFOLLOW`-,
   regulären-Datei-, Owner-only- und Single-Hardlink-Regeln geöffnet.
7. `ArtifactStoreV1::put_file` importiert die Datei mit dem erwarteten Digest
   und der erwarteten Größe. Ein abweichender Digest, eine Größenänderung,
   Truncation, ein Symlink, ein Hardlink oder ein Root-Escape bricht die
   Publikation ab.
8. Erst wenn alle erforderlichen Objekte erfolgreich veröffentlicht oder
   bereits identisch vorhanden sind, werden die kanonischen Manifeste
   exklusiv veröffentlicht.

Die Route ruft nach dem Inspector-Schritt keinen Worker, keinen Adapter und
keinen Replay-Pfad auf. Sie schreibt nicht in BUNDLE oder BUNDLE.run.

## Auswahl der importierbaren Ergebnisse

V1.1 importiert nur die Ergebnisse, die der V5-Scheduler selbst als akzeptiert
markiert hat. Ein bloß vorhandener Digest, ein `RESULT_RECEIVED` ohne
`ARTIFACT_ACCEPTED`, ein `READY`-Status ohne verifizierte Datei oder eine
manuell in den Execute-Bericht eingetragene Referenz reichen nicht aus.

Für die vorgeschlagene V1.1-Policy gilt:

- alle Knoten des Laufs müssen im finalen V5-Zustand akzeptiert und `READY`
  sein;
- `INCOMPLETE`, `FAILED`, `BLOCKED`, `CANCELLED`, fehlende Knoten und
  widersprüchliche Receipt-/Run-Log-Zuordnungen werden abgewiesen;
- Retries sind nur als bereits geprüfte Attempt-Historie relevant; importiert
  wird ausschließlich der final akzeptierte Attempt je Knoten;
- abgelehnte oder fehlgeschlagene frühere Attempts werden nicht in den Store
  kopiert und erhalten kein Manifest;
- ein leerer, aber gültiger Output darf importiert werden, sofern der bestehende
  V5- und Store-Vertrag ihn mit Digest und Größe bestätigt.

Damit bleibt ein erfolgreicher Publish-Lauf eine vollständige, über den
Inspector belegte V5-Ausführung und keine nachträgliche Teilfreigabe eines
fehlerhaften DAGs.

## Manifestmodell

Der bestehende `ArtifactManifestV1`-Codec enthält bereits:

- `run_id`;
- `node_id`;
- `attempt_id`;
- `status`;
- `result_code`;
- eine geordnete Liste aus `sha256`, `size_bytes`, `media_type` und `origin`.

Die vorgeschlagene Brücke erzeugt **ein Manifest je akzeptiertem V5-Knoten**.
Damit bleibt das vorhandene Manifestmodell unverändert und kann die
Run-/Knoten-/Attempt-Bindung eines einzelnen Ergebnisses vollständig abbilden.
Das Manifest enthält keine Payloadbytes, keine absoluten Pfade und keine
privaten Attempt-Referenzen.

Für jedes Manifest werden mindestens diese Werte aus dem validierten Run
übernommen:

| Manifestfeld | Herkunft und Bedeutung |
|---|---|
| `run_id` | kanonische V5-Run-ID aus Run-Log/Inspector |
| `node_id` | V5-Knoten-ID aus gebundenem Execute-Report und Run-Log |
| `attempt_id` | final akzeptierte Attempt-ID |
| `status` | `READY` aus dem akzeptierten Ergebnis |
| `result_code` | geprüfter V5-Resultcode |
| `object_0_sha256` | Digest des importierten `result.bin` |
| `object_0_size_bytes` | exakte geprüfte Dateigröße |
| `object_0_origin` | kontrolliertes, nicht privates Herkunftstoken, vorzugsweise die öffentliche Referenz wie `artifact/a` |

`media_type` bleibt leer oder erhält nur einen explizit kontrollierten,
vertraglich festgelegten Token; aus Dateinamen oder Inhalt wird kein freier
Typ abgeleitet. Die bestehende V1-Manifestgrammatik und das Objektlimit von
32 MiB bleiben gültig.

Die öffentliche V5-Referenz (`artifact/a`) wird nicht als Dateipfad benutzt.
Sie dient nur als kontrolliertes Metadatenfeld. Falls die Implementierung eine
separate `public_reference`-Eigenschaft benötigt, ist das eine versionierte
Manifeständerung und darf nicht still in V1 eingeführt werden. Für V1.1 ist
die Wiederverwendung des vorhandenen `origin`-Feldes der bevorzugte Weg.

### Manifest-ID und Wiederholung

Für jedes Knotenmanifest wird eine deterministische ID aus einer kanonischen
Tuple-Bindung gebildet, zum Beispiel:

```text
BRUCE_DAG_SCHEDULER_V5_PUBLISH_V1\n
run_id=<run_id>\n
node_id=<node_id>\nattempt_id=<attempt_id>\n
```

Die ID wird vor dem Store-Aufruf validiert und als sicherer
`<manifest-id>.manifest`-Name verwendet. Die endgültige Bytecodierung und
Trennzeichen müssen Bestandteil der Implementierungsplanung werden.

Die Veröffentlichung ist idempotent:

- Ein identisches Objekt mit gleichem Digest und gleicher Größe wird vom Store
  erfolgreich wiederverwendet.
- Ein identisches Manifest unter derselben ID wird erfolgreich wiederverwendet.
- Ein vorhandenes Objekt oder Manifest mit abweichendem Inhalt ist ein
  Konflikt; es wird nie überschrieben.
- Eine erneute Publish-Anfrage darf keine neue Attempt-ID und keine neue
  Version desselben Manifests erzeugen.

## Fehler- und Teilpublikationssemantik

Der Store V1 besitzt keine Transaktion über mehrere Objekte und Manifeste.
Deshalb gilt folgende klare V1.1-Grenze:

1. Die Brücke validiert den gesamten Run und die gesamte Importliste vor dem
   ersten Store-Schreibvorgang.
2. Objektpublikationen sind immutable und idempotent. Bei einem Fehler werden
   vorhandene Objekte nicht gelöscht und nicht überschrieben.
3. Manifeste werden erst nach erfolgreichem Objektimport geschrieben.
4. Jedes einzelne Manifest wird atomar und exklusiv veröffentlicht.
5. Ein Fehler beim späteren Objekt- oder Manifest-Schritt kann bereits
   veröffentlichte, noch nicht referenzierte immutable Objekte oder bereits
   veröffentlichte Knotenmanifeste zurücklassen. Die Route darf dies nicht als
   vollständigen Erfolg melden.
6. Es gibt in V1.1 keinen Rollback durch Löschen und keine automatische GC.
   Ein späterer Lauf kann identische Objekte/Manifeste idempotent übernehmen;
   ein Operator kann die nicht referenzierten Daten erst in einem zukünftigen
   Retention-/GC-Vertrag behandeln.

Die Implementierung soll vor dem Schreiben alle vorhandenen Zielmanifeste
prüfen, soweit dies ohne zusätzliche Store-API möglich ist, damit ein
vorhersehbarer Konflikt möglichst vor Teilpublikation erkannt wird. Eine
vollständige Mehrfachmanifest-Transaktion ist ausdrücklich kein V1.1-Ziel.

## Kompatibilitätsgrenzen

Die Brücke darf nicht verändern:

- `bruce dag-scheduler-v5 execute`-Syntax, Defaults, Budgets, Retry- und
  Workersemantik;
- den Vertrag `dag-scheduler-v5-cli-v1` und dessen Byteausgabe;
- `bruce dag-scheduler-v5 inspect BUNDLE`-Syntax und
  `dag-scheduler-v5-inspect-v1`-Bericht;
- `worker-run-log-v1`, seine Ereignisse, Sequenzen oder privaten Pfade;
- Artifact Store V1-Objektidentität, Objektlimit, Sicherheitsgrenzen,
  Manifestgrammatik und CLI-Routen;
- V2-, V3- und V4-Routen.

Erlaubt ist eine gemeinsame interne Validierungskomponente, wenn sie die
bestehenden öffentlichen Berichte bytegleich lässt und weiterhin strikt
read-only arbeitet.

## Sicherheits- und Datenflussgrenzen

- Das BUNDLE wird vor dem Seitensatz geprüft und an genau `BUNDLE.run/`
  gebunden.
- Kein Pfad aus einer öffentlichen JSON-Referenz wird direkt geöffnet.
- Der Import folgt keiner Symlink-Kette und verlässt den Run-Root nicht.
- Quellen müssen regulär, owner-only und einfach verlinkt sein; Digest und
  Größe werden beim Import erneut gegen den validierten Receipt geprüft.
- Artifact Store und Manifestdateien behalten ihre owner-only- und
  descriptor-relativen Regeln.
- Payloadbytes erscheinen weder im Publish-Bericht noch in Logs.
- `sha256` bindet lokale Bytes, ist aber keine Verschlüsselung,
  Authentifizierung oder Benutzerfreigabe.
- V1.1 führt keinen Download- oder Readback-Endpunkt ein.

## Tests und Abnahmebelege für die spätere Implementierung

Die Implementierungsplanung muss mindestens folgende Fälle abdecken:

1. Ein einzelner vollständig gültiger `READY`-Run importiert sein Ergebnis,
   veröffentlicht Objekt und Manifest und liefert den kanonischen Publish-
   Bericht.
2. Ein gültiger Mehrknoten-Run veröffentlicht je akzeptiertem Knoten genau
   ein deterministisches Manifest in Spec-Reihenfolge.
3. Eine identische Wiederholung ist bytegleich und idempotent.
4. Ein bestehendes Manifest oder Objekt mit gleichem Inhalt wird wiederverwendet;
   ein Konflikt führt zu Exit 2 ohne Überschreiben.
5. `INCOMPLETE`, `FAILED`, `BLOCKED` und `CANCELLED` werden vor jedem Store-
   Schreibvorgang abgewiesen.
6. Manipulierter Execute-Bericht, abgeschnittenes Run-Log, falsche Sequenz,
   falscher Digest, falsche Größe, fehlendes Artefakt, Symlink, Hardlink,
   Root-Escape und unsichere Rechte werden durch Inspector/Store abgewiesen.
7. Ein Fehler beim zweiten Objekt oder Manifest erzeugt keinen falschen
   Erfolgsbericht und löscht keine bereits immutable publizierten Daten.
8. Publish schreibt weder BUNDLE noch BUNDLE.run und startet keinen Child-
   Prozess; bestehende Execute-/Inspect-Tests bleiben unverändert gültig.
9. Der Bericht enthält keine Payloadbytes, privaten Pfade, OS-Fehlertexte oder
   Geheimnisse.
10. Die bestehenden Artifact-Store-, Worker-, V5- und Dokumentationstests
    bleiben kompatibel.

## Nicht Bestandteil von V1.1

- automatische Ausführung oder Wiederholung eines Workers;
- Veränderung oder Reparatur eines V5-Laufs;
- Payload-Download, Payload-Readback oder öffentliche Byteausgabe;
- Garbage Collection, Retention, Löschen oder Rollback;
- Netzwerk, Remote Store, S3, Datenbank oder externe Provider;
- dynamische Code-Plugins, GPU-/Netzwerkausführung, freie Sprache,
  allgemeines Beweisen oder übergreifende Runtime-Orchestrierung;
- Authentifizierung, Autorisierung, Verschlüsselung oder Multi-Host-Replikation;
- automatische Integration in `execute` über eine neue Option wie
  `--artifact-store`;
- Änderung der bestehenden V2-, V3-, V4- oder V5-Verträge.

## Offene Entscheidungen vor der Implementierungsplanung

1. **Mehrknoten-Manifest:** Der Entwurf schlägt ein Manifest je akzeptiertem
   Knoten vor, weil `ArtifactManifestV1` genau eine `node_id`/`attempt_id`
   Bindung vorsieht. Zu bestätigen ist, dass kein neues run-weites
   Aggregatmanifest für V1.1 benötigt wird.
2. **Öffentlicher Publish-Bericht:** Die genaue kanonische Feldreihenfolge,
   insbesondere die zusätzliche Ausgabe von `dag_id`, `manifest_id` und
   `reference`, muss vor der CLI-Implementierung festgelegt werden.
3. **Manifest-ID-Kanonisierung:** Die exakte Tuple-Codierung und der daraus
   berechnete Digest-basierte ID-Name müssen festgeschrieben werden.
4. **`origin` als Referenzfeld:** Der bevorzugte Entwurf legt die bestehende
   öffentliche V5-Referenz in `ArtifactManifestObjectV1::origin` ab. Eine
   separate `public_reference`-Spalte wäre eine neue Manifestversion und
   braucht eine eigene Freigabe.
5. **Teilpublikation bei Mehrfachmanifeste:** V1.1 hat keine Store-Transaktion
   über mehrere Manifeste. Zu bestätigen ist, dass immutable, nicht
   referenzierte Objekte/Manifeste bei einem späteren Fehler liegenbleiben
   dürfen und erst ein zukünftiger GC-Vertrag sie behandelt.
6. **Interne Inspector-Schnittstelle:** Zu entscheiden ist, ob die Brücke eine
   gemeinsame interne `ValidatedV5Run`-Struktur erhält oder den bestehenden
   Inspector um einen rein internen Importbericht erweitert. Die öffentliche
   Inspect-Ausgabe darf in beiden Fällen unverändert bleiben.
7. **Fehlercode:** Der Entwurf verwendet für alle öffentlichen
   Publikationsfehler das bestehende kompakte `PROTOCOL_ERROR`-Envelope. Ein
   differenzierter, aber weiterhin metadatenfreier Fehlervertrag wäre eine
   separate CLI-Versionierung.

## Zusammenfassung der Beleglage

**IMPLEMENTIERT UND BELEGT:** V5 erzeugt prüfbare, private
`ARTIFACT_ACCEPTED`-Artefakte; der Inspector validiert sie read-only; der
Artifact Store kann immutable Objekte und kanonische Manifeste sicher und
idempotent veröffentlichen.

**DOKUMENTIERT / KONZEPT:** Die hier beschriebene Verbindung über
`bruce dag-scheduler-v5 publish BUNDLE --store STORE` existiert zum Zeitpunkt
dieses Entwurfs noch nicht.

**VORSCHLAG:** Die konkrete Publish-Route, der per-Knoten-Manifestzuschnitt,
die deterministische ID-Bildung und die oben beschriebene Fehler-/Teil-
publikationssemantik werden nach Bestätigung dieses Entwurfs in einen
Implementierungsplan überführt.
