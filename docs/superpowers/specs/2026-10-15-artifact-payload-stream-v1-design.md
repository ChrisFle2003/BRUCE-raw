# BRUCE Artifact Payload Stream V1.5 — Host-kontrollierter Chunk-Datenfluss

**Status:** ENTWURF ZUR PRÜFUNG — noch nicht implementiert

**Datum:** 2026-10-15\
**Geltungsbereich:** kontrollierter lokaler Payload-Datenfluss zwischen Bruce,
privaten Worker-Eingaben und direkten DAG-Kanten.

## Ziel

V1.5 standardisiert den internen Datenfluss großer oder zusammengesetzter
Payloads als bounded Chunk-Strom. Bruce bleibt die einzige Instanz, die
Artifact-Store-Objekte, Manifeste, Budgets und Worker-Lebenszyklen kontrolliert.
Der Stream ist kein öffentlicher Download-Endpunkt und kein neuer freier
Worker-Kanal.

Der Vertrag soll drei bisher getrennte Schritte explizit verbinden:

```text
geprüfte ArtifactInputRef
        |
        v
Bruce liest bounded in feste Chunks
        |
        v
private, versiegelte Stream-Ablage
        |
        v
Worker erhält nur kontrollierte Stream-Referenz und Metadaten
```

V1.3 und V1.4 bleiben unverändert. Ohne ausdrückliche V1.5-Auswahl gilt der
bisherige Datei-Inputpfad byte- und verhaltenskompatibel.

## Beleglage und Ausgangspunkt

**IMPLEMENTIERT UND BELEGT:** V1.3 materialisiert geprüfte direkte Store-
Objekte in private Attempt-Dateien; V1.4 ergänzt kontrollierte externe lokale
Sidecar-Referenzen. Der bestehende Worker-V1-Vertrag transportiert relative
`ArtifactRef`-Referenzen mit Digest und Größe.

Belege:

- `apps/dag_scheduler_v5_cli.hpp` — bounded Inputprüfung und Worker-Bindung;
- `include/bruce/worker/worker_boundary_v1.hpp` — unveränderter V1-Frame;
- `include/bruce/artifact/artifact_payload_v1.hpp` — V1.3-Referenzen und
  Materialisierungsgrenzen;
- `include/bruce/artifact/artifact_payload_external_v1.hpp` — V1.4-Sidecar;
- `docs/superpowers/specs/2026-10-15-artifact-payload-flow-v1-design.md` —
  host-mediated Payloadfluss.

**DOKUMENTIERT / KONZEPT:** Ein versionierter, chunkbasierter Datenfluss mit
expliziter Versiegelung, Chunkzählung und Resume-/Abbruchsemantik existiert
noch nicht.

## Harte Nichtziele

V1.5 implementiert nicht:

- Änderungen an `worker-v1`-Framekodierung oder bestehenden V1.3/V1.4-Reports;
- Netzwerk-, URL-, S3-, Redis- oder Provider-Zugriff;
- freie Programme, dynamische Plugins oder vom Worker gestartete Prozesse;
- öffentliche Payloadausgabe, Download- oder Materialisierungs-CLI;
- Resume über Prozess- oder Laufgrenzen hinweg;
- automatische Retention, Garbage Collection, Reparatur oder Cache;
- GPU-Ausführung, freie Sprache, allgemeines Beweisen oder übergreifende
  Runtime-Orchestrierung.

Diese Punkte bleiben **DOKUMENTIERT / KONZEPT**.

## Öffentliche Auswahl und Kompatibilität

V1.5 wird ausschließlich über eine neue, explizite interne Ausführungsoption
oder eine später benannte versionierte Route aktiviert. Die bestehende Syntax

```text
bruce dag-scheduler-v5 payload SPEC --output BUNDLE --store STORE
```

bleibt V1.3/V1.4. Eine automatische Aktivierung anhand der Objektgröße oder
des Dateinamens ist verboten.

Für bestehende Aufrufer gelten diese Regeln:

1. Der Worker-V1-Frame bleibt strukturell unverändert.
2. V1.3- und V1.4-Berichte erhalten keine Streamfelder.
3. Die bisherige `ArtifactRef` bleibt eine gültige private Eingabereferenz.
4. Ein V1.5-Stream darf erst als `READY` an einen Worker gebunden werden,
   wenn alle Chunks geprüft und die Endbindung versiegelt ist.
5. V1.5-Fehler werden nur im ausdrücklich ausgewählten V1.5-Vertrag sichtbar.

## Logischer Vertrag

Der host-interne Wert `ArtifactPayloadStreamV15` besteht aus:

```text
stream_id       opaker Bruce-interner Bezeichner
reference       relative private Referenz unter dem Attempt-Root
sha256          SHA-256 der vollständigen Payload
size_bytes      exakte Gesamtgröße
chunk_bytes     feste Chunkgröße des Laufs
chunk_count     exakte Anzahl versiegelter Chunks
```

`stream_id` und `reference` sind keine Storepfade, URLs oder vom Benutzer
gewählte Ziele. Sie werden ausschließlich von Bruce erzeugt. Der öffentliche
Report darf höchstens Digest, Größe und Chunkanzahl ausgeben; `stream_id` und
private Referenzen bleiben intern.

### Chunkformat

Die private Ablage verwendet eine feste, kanonische Reihenfolge:

```text
stream/
  header.jsonl
  chunks/00000000.part
  chunks/00000001.part
  ...
  seal.json
```

Die genaue Dateibenennung ist Bestandteil der Implementierungsplanung. Jede
Chunkdatei ist owner-only, regulär, nicht verlinkt und bounded. `seal.json`
enthält nur die bereits bekannten Metadaten `sha256`, `size_bytes`,
`chunk_bytes` und `chunk_count`; freie Pfade, URLs und OS-Texte sind ungültig.

Vor dem Versiegeln darf kein Worker die Ablage lesen. Nach erfolgreicher
Versiegelung wird die Ablage atomar in den privaten Attempt-Bereich übernommen.
Eine unvollständige Ablage ist niemals eine gültige Eingabe.

## Datenfluss

```text
1. Spec und direkte DAG-Kante validieren
2. ArtifactInputRef oder V1.4-Sidecarbindung prüfen
3. Streamgrenzen aus dem bestehenden Inputbudget ableiten
4. private Streamablage owner-only anlegen
5. Quelle descriptor-relativ und bounded in feste Chunks lesen
6. pro Chunk Größe und laufenden Digest aktualisieren
7. Quelle gegen erwarteten Digest und exakte Größe prüfen
8. seal.json schreiben und per fsync versiegeln
9. Stream atomar als private Worker-Eingabe adoptieren
10. bestehenden Worker-V1-Aufruf ausführen
11. bei Erfolg Output wie bisher prüfen und publizieren
12. bei Fehler Stream und nicht angenommene Teildateien entfernen
```

Die Quelle wird nie über `origin` oder einen freien Pfad ausgewählt. Bruce
öffnet ausschließlich bereits validierte Store-Objekte oder bereits
Bruce-eigene private Eingaben.

## Budget- und Zustandssemantik

V1.5 verwendet zunächst die bestehenden Budgets:

- `max_input_bytes` begrenzt die Summe aller gelesenen Payloadbytes;
- `max_wall_ms` begrenzt den gesamten Host- und Workerpfad;
- bestehende Objekt-, Output-, Operations- und Memory-Grenzen bleiben gültig;
- `chunk_bytes` ist ein fester Implementierungswert und keine freie
  Benutzeroption in der ersten Version.

Der Stream durchläuft exakt diese Zustände:

```text
CREATED -> WRITING -> SEALED -> ADOPTED -> CONSUMED
                \-> FAILED
```

Nur `SEALED` darf `ADOPTED` werden. `FAILED` darf nicht wiederverwendet oder
als Input veröffentlicht werden. Ein Timeout, eine Digestabweichung, ein
Budgetfehler oder ein Host-Abbruch führt zu `FAILED`; automatische Retries
bleiben der vorhandenen expliziten Retry-Option untergeordnet.

## Fehlervertrag

Interne Fehlerklassen werden mindestens unterschieden als:

- `STREAM_REFERENCE_INVALID`
- `STREAM_SOURCE_INVALID`
- `STREAM_LIMIT_EXCEEDED`
- `STREAM_DIGEST_MISMATCH`
- `STREAM_SEAL_FAILED`
- `STREAM_ADOPTION_FAILED`
- `STREAM_CANCELLED`

Öffentliche Fehlerberichte enthalten keine Pfade, Payloadbytes, `errno`-
Texte, PIDs, Secrets oder Kommandozeilen. Ein nicht ausgewählter V1.5-Pfad
darf keinen dieser Fehlercodes erzeugen.

## Sicherheitsgrenze

Bruce prüft vor jedem Lesen:

- reguläre Datei und `O_NOFOLLOW`;
- owner-only-Rechte und genau einen Hardlink;
- descriptor-relative Pfadauflösung;
- bounded Größe und monotone Deadline;
- laufenden Digest sowie abschließende Größenbindung.

Der Worker erhält weder Store-Root noch Manifestpfade, Netzwerkrechte,
Prozessstartrechte oder Pluginrechte. Ein Stream ist nur eine Bruce-eigene
private Eingabeform.

## Test- und Abnahmekriterien

Vor einer Implementierung müssen Tests für folgende Fälle geplant werden:

1. leerer, einteiliger und mehrteiliger Stream;
2. exakte Chunkreihenfolge und bytegleicher Enddigest;
3. Quelle fehlt, ist verändert, zu groß, Symlink, Mehrfach-Hardlink oder
   unsicher berechtigt;
4. später Chunkfehler ohne öffentliches Teilresultat;
5. Timeout, Abbruch und Budgetüberschreitung mit Cleanup;
6. atomare Adoption und keine sichtbare Teilablage;
7. Wiederholung desselben Laufs mit identischem metadata-only Ergebnis;
8. unveränderte V1.3-, V1.4-, Worker-, Publish- und Replay-Verträge.

Die Spezifikation ist erst erfüllt, wenn ein fokussierter CTest-Vertrag, ein
Real-Binary-Workflow und die bestehenden Kompatibilitätsprüfungen grün sind.

## Offene Entscheidungen für die Implementierungsplanung

- konkrete feste `chunk_bytes`-Größe;
- ob `header.jsonl` und `seal.json` als Dateien oder ein einzelner
  descriptor-relativer Container umgesetzt werden;
- ob ein Stream nur innerhalb eines Attempt-Verzeichnisses oder auch zwischen
  direkten Nachfolgerknoten adoptiert werden darf;
- welche minimale V1.5-CLI-Auswahl den neuen Pfad explizit aktiviert.

Diese Entscheidungen gehören in den Implementierungsplan und dürfen nicht
implizit durch Änderungen am bestehenden Worker-Frame getroffen werden.
