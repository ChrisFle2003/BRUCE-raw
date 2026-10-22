# BRUCE Artifact Payload Flow V1.3 — Phase-0-Design

> **Status:** ENTSCHEIDUNG FÜR V1.3 — IMPLEMENTIERUNGSBEREIT. Dieses Dokument
> beschreibt einen freigegebenen V1.3-Vertrag; Produktionscode existiert noch
> nicht.

**Datum:** 2026-10-15\
**Geltungsbereich:** Kontrollierter Payloadfluss zwischen direkten DAG-
Vorgängern und Nachfolgern über den lokalen Artifact Store und die bestehende
Bruce-Worker-Grenze.

## Beleglage und Ausgangspunkt

Die Phase-0-Entscheidungsgrundlage sind die beiden lesenden Analysen:

- Contract Inventory (archival local path, not versioned:
  `runs/bruce-artifact-payload-v1/contract-inventory.md`)
- Design Review (archival local path, not versioned:
  `runs/bruce-artifact-payload-v1/design-review.md`)

Die Analyse belegt den aktuellen Stand, nicht die nachfolgende V1.3-Idee.
Bestehende Belege sind insbesondere:

- **IMPLEMENTIERT UND BELEGT:** `ArtifactStoreV1::resolve_manifest()` prüft
  kanonische Manifeste und alle referenzierten Objekte, liefert aber nur
  Metadaten.
- **IMPLEMENTIERT UND BELEGT:** Phase-DAG V2/V3 und DAG-Scheduler V2/V3/V4
  transportieren direkte Payloadbytes mit Digest-/Größenbindungen.
- **IMPLEMENTIERT UND BELEGT:** V5-Worker erhalten Bruce-freigegebene,
  relative private Dateireferenzen und melden Ergebnisdigest, Größe und Usage.
- **IMPLEMENTIERT UND BELEGT:** V5 Publish kann geprüfte Ergebnisse in
  Artifact-Store-Objekte und Manifeste überführen.
- **DOKUMENTIERT / KONZEPT:** Die Kante von einer geprüften Store-Referenz zu
  den Eingabebytes eines nachfolgenden Knotens existiert noch nicht.

Die folgenden Regeln sind **ENTSCHEIDUNG FÜR V1.3**. Sie sind noch nicht
implementiert und dürfen deshalb nicht als **IMPLEMENTIERT UND BELEGT** gelesen
werden. Nur die ausdrücklich als spätere Erweiterung markierten Punkte bleiben
offen.

## Ziel

Ein direkter DAG-Nachfolger soll den geprüften Output eines erfolgreichen
Vorgängers konsumieren können. Bruce bleibt dabei der einzige Host, der den
Artifact Store öffnet, die Referenz prüft und Bytes bounded in einen privaten
Worker-Eingabebereich überführt. Der Worker erhält weiterhin nur eine
kontrollierte private Eingabe und veröffentlicht weder Storepfade noch
Rohpayload über seinen Frame.

Der vorgeschlagene Datenfluss lautet:

```text
Node A READY
  -> Ergebnis prüfen und als Artifact-Objekt/Manifest veröffentlichen
  -> ArtifactInputRefV13 für die direkte Kante bilden
  -> Bruce löst Manifest und Objekt read-only auf
  -> Bruce kopiert bounded in den privaten Attempt-Inputbereich
  -> Worker B liest nur die private freigegebene Eingabe
  -> Bruce prüft B, veröffentlicht B und entsperrt dessen Nachfolger
```

## Harte Nicht-Ziele

V1.3 darf nicht:

- Phase-DAG V2/V3, DAG-Scheduler V2/V3/V4, deren Bundles oder bestehenden
  Reports umdeuten oder bytekompatible Felder ändern;
- die bestehende V5-Route `execute` still mit Storezugriff koppeln;
- Worker den Store-Root, Manifestpfade, Netzwerkzugriff oder eigene
  Prozessstarts geben;
- `origin` als Pfad, URL oder Provider konfigurieren;
- indirekte Vorgänger oder einen vollständigen DAG-Kontext automatisch
  übertragen;
- Payloadbytes in öffentlichen Berichten, CLI-Ausgaben oder Workerframes
  ausgeben;
- Retention, Garbage Collection, Löschung, Repair, Cache oder Remote-Sync
  einführen;
- Netzwerk, S3, Redis, Datenbank, externe Provider, GPU-Adapter, freie
  Programme, freie Code-Plugins, freie Sprache, allgemeines Beweisen oder
  eine übergreifende Runtime-Orchestrierung implementieren.

Diese größeren Funktionen bleiben **DOKUMENTIERT / KONZEPT**.

## Referenzmodell

### Vorgeschlagener logischer Typ

Der V1.3-Payloadfluss verwendet eine eigene, versionierte Referenz. Sie ist
kein Dateipfad und kein Ersatz für den bestehenden `ArtifactRef`-Workervertrag:

```text
ArtifactInputRefV13
├── manifest_id       opake kanonische Manifest-ID
├── object_index      nullbasierter Index im Manifest
├── sha256            erwarteter Objekt-Digest
└── size_bytes        erwartete exakte Bytegröße
```

Die Referenz darf nur aus einer akzeptierten direkten DAG-Kante entstehen.
`origin` bleibt Provenienzmetadatum des Manifests; es ist kein Leseweg und
gehört nicht in `ArtifactInputRefV13`. Für die aktuelle V5-Publishform mit
genau einem Objekt ist `object_index=0`.

Bruce akzeptiert die Referenz nur, wenn alle folgenden Bedingungen erfüllt
sind:

1. `manifest_id` entspricht der bestehenden opaken ASCII-Manifestgrammatik.
2. Das Manifest ist kanonisch vorhanden und hat zwingend den freigegebenen
   Status `status=READY`.
3. `object_index` liegt innerhalb der Manifestobjektliste.
4. Digest und Größe der Referenz stimmen exakt mit dem Manifestobjekt überein.
5. Das content-addressed Objekt besteht die vollständige bestehende
   Storeprüfung: reguläre Datei, `O_NOFOLLOW`, Owner-only-Rechte,
   Single-Hardlink, Größenlimit und SHA-256.
6. Die Referenz ist eine direkte Dependency des zu startenden Knotens und
   nicht nur ein frei vom Nutzer eingetragener Storeverweis.

Punkte 1 bis 6 sind verbindliche V1.3-Vertragsregeln. Eine Referenz mit
anderem Manifeststatus oder ohne direkte DAG-Bindung wird abgewiesen.

### Provenienz

Die Provenienzprüfung erfolgt über die direkte DAG-Kante und das kanonische
Manifest (`run_id`, `node_id`, `attempt_id`, Status und Result-Code). Die
vier Felder des `ArtifactInputRefV13` bleiben klein und opak; private Attempt-
Pfade werden nicht in die Referenz übernommen. `run_id`, erwartete
Vorgänger-ID und `attempt_id` werden nicht als zusätzliche Referenzfelder
persistiert. Bruce prüft diese Provenienz über die direkte DAG-Kante und die
entsprechenden Manifestfelder.

## Host-/Worker-/Store-Grenzen

| Verantwortlicher | Darf | Muss | Darf nicht |
|---|---|---|---|
| Bruce Host/Orchestrator | Spec prüfen, direkte Dependencies bestimmen, Manifeste auflösen, Objekte bounded lesen, private Inputs erzeugen, Worker starten/abbrechen, Output prüfen und veröffentlichen | Authority über DAG, Store, Budgets, Retry, Status und Veröffentlichung behalten | unvalidierte Pfade weiterreichen, Teilpayload als gültig akzeptieren, interne OS-Fehler öffentlich ausgeben |
| Worker/Child | freigegebene private Inputs lesen, festen Adapter ausführen, eigenes Ergebnis schreiben, Digest/Größe/Usage melden | nur vom Host gebundene Inputs und Limits verwenden | Store öffnen, Netzwerk nutzen, Prozesse starten, Plugins laden, DAG-Entscheidungen treffen, Manifeste veröffentlichen |
| Artifact Store | unveränderliche Objekte/Manifeste halten und bounded verifizieren | Digest-, Größen- und kanonische Manifestbindung liefern | GC, Retention, Authentifizierung oder Remote-Synchronisierung implizit ausführen |

### Empfohlene Host-mediated Materialisierung

Nur Bruce öffnet den Store. Ein zukünftiger Reader soll descriptor-relativ,
bounded und digestprüfend arbeiten. Die Bytes werden in eine neu angelegte,
Bruce-owned private Attempt-Eingabedatei gestreamt. Der Worker erhält danach
eine bestehende relative `ArtifactRef` unter seinem privaten Run-Root.

```text
ArtifactInputRefV13
  -> Manifest-/Status-/Kantenprüfung
  -> Objektprüfung und bounded Stream
  -> private input-Datei, erneut Digest/Größe prüfen
  -> bestehende WorkerRequest-Grenze
```

Ein ungebundenes `std::string`-Lesen, `std::filesystem::copy_file` mit
unkontrollierter Zielauflösung und eine öffentliche Materialisierungsroute
sind nicht zulässig. Ein späterer Byte-Frame wäre ein eigener versionierter
Vertrag, nicht eine stille Erweiterung von `worker-v1`.

## DAG-Semantik

1. Nur direkte Kanten transportieren Payload. Indirekte Vorgänger werden
   nicht automatisch weitergereicht.
2. Ein Node startet erst, wenn alle direkten Vorgänger `READY`, vollständig
   geprüft und erfolgreich veröffentlicht sind.
3. Inputrefs werden in der bestehenden deterministischen Spec-/Node-Reihenfolge
   gebildet. Die Reihenfolge darf nicht von `origin` oder Dateinamen abhängen.
4. Eine fehlende, nichtkanonische, beschädigte oder nicht freigegebene
   Referenz lässt den betroffenen Node kontrolliert fehlschlagen; abhängige
   Nodes werden `BLOCKED`.
5. Ein Retry erhält eine neue Attempt-ID. Nur der tatsächlich akzeptierte
   Attempt veröffentlicht und entsperrt Nachfolger.
6. `--workers N` darf unabhängige READY-Kandidaten parallel ausführen. Die
   Store-Publikation und Zustandsadoption bleiben durch Bruce determiniert.
7. Zyklen und unbekannte Vorgänger werden vor dem ersten Store-Zugriff als
   Spec-/Protocol-Fehler abgelehnt.

Ein vorgeschlagener Lauf:

```text
Spec lesen und DAG validieren
↓
direkt bereite Nodes bestimmen
↓
ArtifactInputRefs auflösen und alle Objekte prüfen
↓
Inputs bounded in private Attempt-Verzeichnisse materialisieren
↓
Worker ausführen, überwachen und bei Bedarf kontrolliert abbrechen
↓
Workerresultat prüfen und Output publizieren
↓
direkte Nachfolger freischalten
↓
JSONL-Log, Receipt und metadata-only Bericht abschließen
```

## Ressourcenbudgets

Die bestehende V5-Budgetfamilie bleibt unverändert und wird für V1.3 wie folgt
angewandt:

| Budget | V1.3-Arbeitssemantik | Bestehender Default / Maximalwert |
|---|---|---:|
| `max_input_bytes` | Summe der logisch einmal gezählten direkten Vorgängerobjekte plus Requestbytes; keine doppelte öffentliche Zählung der Hostkopie | `65.536 / 1.048.576` |
| `max_output_bytes` | Workeroutput vor Store-Publikation | `65.536 / 5.242.880` |
| `max_wall_ms` | Node-Versuch einschließlich Referenzauflösung, Materialisierung, Worker, Prüfung und Publikation | `30.000 / 600.000` |
| `max_operations` | fachliche Adapteroperationen; Store-Digest-Reads zählen nicht als Adapteroperation | `1.000.000 / 10.000.000` |
| `max_memory_bytes` | harte Child-Grenze via bestehendes `RLIMIT_AS`; Host hält keine vollständige Payload im Speicher | `268.435.456 / 2.147.483.648` |
| Objektgröße | Minimum aus Store-Maximum und verbleibendem Inputbudget | bestehendes Artifact-Store-Limit |
| direkte Inputobjekte | höchstens 15 pro Node | feste V1.3-Grenze; der unveränderte Worker-Vertrag reserviert einen der 16 Gesamtplätze für den Request |

Die gemeinsame `max_wall_ms`-Deadline beginnt vor dem ersten Store-Lesezugriff
und ist eine End-to-End-Hard-Deadline für Referenzauflösung, Materialisierung,
Worker, Ergebnisprüfung und Publikation. Ein separates `max_resolve_ms`, ein
globales Store-Read-Budget und ein separates Host-I/O-Limit gehören nicht zum
V1.3-Vertrag. `resolve_ms` darf ausschließlich metadata-only diagnostisch
erfasst werden.

## Fehler- und Geheimnisvertrag

Die interne Klassifikation soll mindestens folgende Fälle unterscheiden:

| Klasse | Bedeutung | Wirkung |
|---|---|---|
| `ARTIFACT_REFERENCE_INVALID` | syntaktisch oder semantisch ungültige Kante | Node `FAILED`, keine Materialisierung |
| `ARTIFACT_MANIFEST_INVALID` | fehlend, nichtkanonisch oder nicht freigegeben | Node `FAILED`, abhängige Nodes `BLOCKED` |
| `ARTIFACT_DIGEST_MISMATCH` | fehlendes, falsches oder beschädigtes Objekt | Node `FAILED`, kein Nachfolgerstart |
| `ARTIFACT_STORE_VIOLATION` | Symlink, Hardlink, Berechtigung, Nichtregulärdatei oder Rootverletzung | Node `FAILED`, keine öffentliche Pfadinformation |
| `INPUT_LIMIT_EXCEEDED` | Inputobjekte oder Gesamtgröße überschreiten Budget | kontrolliertes Fehlschlagen, kein automatischer Retry |
| `TIMEOUT` / `PROCESS_CRASHED` | bestehende Workerfehler | nur mit expliziter Retry-Option wiederholbar |
| `OUTPUT_LIMIT_EXCEEDED` / `DIGEST_MISMATCH` | Workeroutput nicht akzeptiert | kein Manifest, kein Nachfolgerstart |

Die Fehlerklassen und der neue CLI-Subcommand sind **ENTSCHEIDUNG FÜR V1.3**.
Der neue öffentliche Einstieg lautet:

```text
bruce dag-scheduler-v5 payload SPEC --output BUNDLE --store STORE
```

Die Fehlerklassen werden stabil metadata-only ausgegeben. Fest steht: kein
partieller `READY`-Bericht, kein automatischer Retry von Integritäts- oder
Berechtigungsfehlern, und Retry ist ausschließlich über die explizite
CLI-Option zulässig.

Eine öffentliche V1.3-Route soll metadata-only JSON mit Exitcode 2 verwenden.
Sie darf keine Storepfade, privaten Attempt-Referenzen, PIDs, `errno`-Texte,
Stackdaten, Payloadbytes, Secrets oder Kommandozeilen ausgeben. Die genaue
Feldreihenfolge wird in der Implementierungsplanung festgeschrieben.

## Kompatibilitätsvertrag

Folgende Verträge bleiben unverändert:

- Phase-DAG V2/V3 und NanoCell-Adapter samt direkten Payloadfeldern und
  Bundles;
- DAG-Scheduler V2/V3/V4 samt Spec-, Replay- und Reportformaten;
- Artifact Store V1 `import`, `inspect`, `verify`;
- Artifact Store V1.2 `resolve` als read-only metadata-only Route;
- V5 `execute`, `inspect` und `publish`, solange kein expliziter V1.3-
  Payloadmodus gewählt wird;
- der metadata-only `worker-v1`-Frame.

Der Payloadfluss erhält den eigenen expliziten V1.3-Subcommand:

```text
bruce dag-scheduler-v5 payload SPEC --output BUNDLE --store STORE
```

Der Name und die Route sind **ENTSCHEIDUNG FÜR V1.3**. Ein bestehendes
`execute --store` wird nicht verwendet. Alte V5-Bundles und Run-Logs werden
nicht automatisch migriert.

## Abnahmekriterien für die spätere Implementierung

Die V1.3-Implementierung darf erst als belegt gelten, wenn reproduzierbare
Tests mindestens zeigen:

- ein erfolgreicher Ein-Knoten-Lauf mit kontrolliertem Input;
- mehrere direkte Vorgänger in deterministischer Reihenfolge;
- Null-Input- und Mehrfach-Input-Fälle gemäß dem festgelegten Vertrag;
- fehlendes Manifest, nicht freigegebenes Manifest, fehlendes Objekt,
  Digest-/Größenkonflikt, unsichere Storedatei und Inputbudgetüberschreitung;
- keine Payload-/Pfad-/OS-Fehlerleaks in öffentlichen Berichten;
- transitive `BLOCKED`-Semantik nach einem Inputfehler;
- Retry nur mit expliziter CLI-Option und neuer Attempt-ID;
- unabhängige Nodes mit `--workers N`, ohne nondeterministische Store-Adoption;
- unveränderte V2/V3/V4/V5- und Artifact-Store-V1/V1.2-Regressionen;
- read-only Resolve bleibt read-only, während V1.3-Materialisierung nur im
  privaten Run-Bereich schreibt.

## Entscheidungen für V1.3

Für die Implementierung sind folgende Punkte verbindlich festgelegt:

1. Die öffentliche Route lautet `bruce dag-scheduler-v5 payload SPEC
   --output BUNDLE --store STORE`.
2. Nur kanonische Manifeste mit `status=READY` dürfen als Input dienen.
3. `ArtifactInputRefV13` enthält genau `manifest_id`, `object_index`,
   `sha256` und `size_bytes`.
4. Provenienz wird über die direkte DAG-Kante und die Manifestfelder geprüft;
   zusätzliche Provenienzfelder werden nicht in die Referenz aufgenommen.
5. Bruce materialisiert Payloads host-mediated in private bounded Dateien.
6. Die bestehende `max_wall_ms`-Deadline gilt end-to-end; ein separates
   Resolve-Budget wird in V1.3 nicht eingeführt.
7. Alle bestehenden Worker-Budgets, Defaults und Maximalwerte bleiben gültig.
8. Ein Node darf höchstens 15 direkte Payloadobjekte referenzieren; der
   unveränderte Worker-Vertrag reserviert einen der 16 Gesamtplätze für den
   Request.
9. Fehler werden über stabile metadata-only Fehlerklassen ausgegeben.
10. Ein Retry ist nur über die explizite CLI-Option zulässig.
11. Ein akzeptierter READY-Node wird sofort veröffentlicht; erst danach
    werden direkte Nachfolger freigeschaltet.
12. Bruce adoptiert Store- und DAG-Zustände deterministisch.

## Spätere Erweiterungen außerhalb von V1.3

Die folgenden Punkte sind echte spätere Erweiterungen und blockieren die
V1.3-Implementierung nicht:

- ein eigener versionierter Byte- oder Streaming-Frame statt Datei-
  Materialisierung;
- mehrere Outputobjekte, falls ein späterer Vertrag sie benötigt;
- getrennte Host-/Resolve-Budgets nach Messdaten;
- Retention, Leases, Garbage Collection oder automatische Store-Reparatur;
- Remote-/Netzwerk-Provider, externe Payloadspeicher und Authentifizierung;
- GPU-Adapter, freie Plugins und andere Ausführungsumgebungen.

`origin` bleibt in V1.3 ausschließlich Metadatum. Eine spätere semantische
Provenienzprüfung von `origin` wäre ein neuer Vertrag.

## Phase-0-Einstufung

**ENTSCHEIDUNG FÜR V1.3 — implementierungsbereit, aber noch nicht
implementiert.**

Die bestehende Architektur liefert belastbare Sicherheitsbausteine für eine
lokale host-kontrollierte Umsetzung. Die Implementierungsplanung darf jetzt
den Reader, den V1.3-Inputtyp und den DAG-Ausführungspfad umsetzen. Bis die
geplanten Tests und Reviews bestehen, bleibt die Funktion
**DOKUMENTIERT / KONZEPT**.
