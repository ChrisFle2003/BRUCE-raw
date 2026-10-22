# Logic-Mod Result Artifact V1

**Status: IMPLEMENTIERT UND BELEGT** für den dokumentierten Linux-x86_64-Pfad.
Publisher, Runner, CLI, Record und Replay sind durch Quellen, ausgeführte
Vertragstests und unabhängige Reviews belegt. Der P1-FIFO-Befund aus dem
Gesamtreview ist behoben und unabhängig nachgeprüft. Die Gesamtsuite vor der
Reparatur bestand 234/237 Tests; drei Socket-Sandboxgrenzen bleiben gesondert
ausgewiesen. Nach der Reparatur bestanden die lokale Auswahl 17/17 und die
unabhängige Nachprüfung 10/10; die zwei zusätzlich ausgewählten Store-V3-Tests
scheiterten weiterhin an der Socket-Sandbox. Belege und genaue Prüfgrenzen
liegen in den archivalen lokalen, nicht versionierten Pfaden
`runs/logic-mod-result-artifact-v1/reports/final-review.md`,
`runs/logic-mod-result-artifact-v1/reports/whole-change-review.md` und
`runs/logic-mod-result-artifact-v1/reports/final-fix-rereview.md`.
Diese Spezifikation erweitert ausschließlich den Logic-Mod-Plugin-V1- und
Payload-Capability-V1-Pfad; eine vollständig grüne Gesamtsuite oder andere
Plattformen werden damit nicht behauptet.

## Ziel und Erfolgskriterium

Ein isolierter, akzeptierter Logic-Mod-Lauf darf sein bereits vom Host
validiertes Ergebnis genau einmal als lokales Artifact-Store-Objekt
veröffentlichen. Der Worker bleibt vollständig ohne Store-, Pfad- und
Schreibzugriff. Erfolg bedeutet: Ein anschließender lokaler Replay prüft die
identische READY-Manifestreferenz und die kanonischen Ergebnisbytes ohne einen
zweiten Workerstart.

## Bestehende Grundlage

Logic-Mod Plugin V1 startet einen signierten, statischen Linux-Worker nur nach
frischer Registry-Bindung. Payload Capability V1 gibt ihm optional ein einziges
verifiziertes Objekt bis 1 MiB als versiegeltes read-only FD 6. Der Worker
liefert ein privates `response_frame`; der Host validiert dieses gegen den
festen Template-Vertrag und erzeugt erst dann `LogicModRunV1::result`.

Result Artifact V1 veröffentlicht nur dieses akzeptierte `result`. Es
veröffentlicht nie das Worker-Protokoll `response_frame`.

## Öffentliche Schnittstelle

Die bestehende Sitzung erhält zwei zusätzliche optionale Werte:

```text
logic-mod-plugin-v1 session PACKAGE --trust-store TRUST \
  --actions install,activate,run,inspect,replay \
  --output-manifest MANIFEST_ID --store LOCAL_STORE
```

`--output-manifest` und `--store` bilden ein Paar: wenn Ausgabe angefordert
wird, kommen beide genau einmal vor. `--store` bleibt mit einer möglichen
`--input-artifact`-Referenz derselbe lokale Store und kommt auch bei einer
kombinierten Ein-/Ausgabe nur einmal vor. Der vollständige Parservertrag ist:
`--store` kommt genau einmal vor, wenn mindestens eine der Optionen
`--input-artifact` oder `--output-manifest` vorkommt; anderenfalls fehlt es.
Fehlt `--input-artifact`, ist dies ein reiner Ergebnis-Store; fehlt die
Ausgabeoption, sind alle bestehenden inputfreien und Input-Payload-Läufe
bytekompatibel.

Der Aufruf akzeptiert keine zweite Ausgabe, keinen Dateipfad als Ergebnisziel,
keine URL, keine freie Codec-Wahl und keinen Remote-Store.

## Berechtigung und Paketvertrag

Die Capability `artifact.output.v1` autorisiert ausschließlich die
Host-Veröffentlichung eines akzeptierten Ergebnisses. Der Worker erhält daraus
keine weiteren Deskriptoren, Syscalls oder Rechte.

Zulässige, streng sortierte Capability-Mengen eines Logic-Mod-Pakets sind:

```text
{worker.execute}
{artifact.input.v1, worker.execute}
{artifact.output.v1, worker.execute}
{artifact.input.v1, artifact.output.v1, worker.execute}
```

Jede andere Menge wird bei Paketprüfung abgelehnt. Die Ausgabeoption verlangt
`artifact.output.v1`; die Eingabeoption verlangt weiterhin
`artifact.input.v1`. Vor jedem Spawn bindet BRUCE alle benötigten Rechte frisch
an denselben aktiven Paket-, Registry- und Workerzustand.

## Ergebnisdaten und atomare Veröffentlichung

Nur wenn der Worker normal endet und der Host `accepted=true` gesetzt hat,
kodiert der Host `LogicModRunV1::result` unverändert als UTF-8-Bytefolge mit
dem festen Codec `logic-mod-result-text-v1`. Die maximale Ergebnisgröße beträgt
16.384 Bytes; eine Überschreitung kann nicht veröffentlicht werden. Das
gemeinsame stdout/stderr-Budget begrenzt zusätzlich den vollständigen
Worker-Protokollrahmen und ist keine Zusage von 16.384 nutzbaren Ergebnisbytes
im CLI-Standardbudget.

Der Host importiert die Bytes über den bestehenden lokalen Artifact Store und
veröffentlicht ein neues, kanonisches READY-Manifest mit genau diesem Objekt.
`MANIFEST_ID` ist eine bestehende, gültige Artifact-Manifest-ID. Das vorhandene
Store-Verhalten bleibt maßgeblich: ein bereits existierendes, bytegleiches
Manifest ist idempotent; eine abweichende Belegung führt zu
`ArtifactManifestConflict`. Ein Result Artifact V1 ergänzt oder überschreibt
niemals ein vorhandenes Manifest.

Erst nach erfolgreichem Objektimport und Manifest-Publish gilt der Lauf als
mit Ergebnisartefakt veröffentlicht. Bei Validierungs-, Worker-, Limit-,
Objektimport-, Manifest- oder Syncfehlern wird kein neues READY-Manifest als
erfolgreiches Ergebnis ausgegeben. Teilobjekte folgen ausschließlich dem
bereits bestehenden atomaren/idempotenten Store-Verhalten.

## Isolation

Der Worker erhält unverändert nur die festgelegten Standardkanäle, das
ausführbare Memfd, den Exec-Gate-Kanal und optional FD 6 für einen
Eingabepayload. Er erhält niemals Storepfad, Store- oder Manifest-FD,
Ausgabe-Manifest-ID, Schreib-FD oder Netzwerkrecht. Der bestehende Seccomp-
und Einmal-`execveat`-Schutz bleibt für alle vier Capability-Mengen gleich.

## Record, CLI-Antwort und Replay

Wenn Ausgabe angefordert und erfolgreich veröffentlicht wurde, ergänzt der
Run-Record in einer eindeutig optionalen, kanonischen Erweiterung:

```text
output.v1
1
output_manifest_id
output_object_index
output_sha256
output_size_bytes
output_codec = logic-mod-result-text-v1
```

Jeder Wert ist einzeln kanonisch längenpräfixiert. `output.v1` ist der feste
Discriminator und `1` die feste Präsenz. Die Erweiterung folgt auf den alten
11-Feld-Präfix beziehungsweise auf dessen vollständigen optionalen
6-Feld-Inputanhang. Ohne Ausgabe fehlen alle sieben Erweiterungsfelder;
es wird kein Null-Präsenzfeld geschrieben. Der Ergebnisindex ist immer `0`.

Diese Felder enthalten weder Pfad noch Store-Identität. Ohne Ausgabe müssen
die bisherigen Run-Record-Bytes exakt unverändert bleiben. Die JSON-
Sitzungsantwort zeigt bei Erfolg ausschließlich Manifest-ID, Objektindex,
SHA-256, Größe und Codec; niemals rohe Artifact-Bytes oder lokale Pfade.

Replay startet keinen Worker. Neben der bestehenden erneuten Hostvalidierung
des gespeicherten `response_frame` löst der Host das gespeicherte
`output_manifest_id:output_object_index` frisch aus dem angegebenen lokalen
Store auf. Es muss READY sein und Digest, Größe, Codec und Bytefolge müssen
exakt zum kanonisch validierten `result` passen. Fehlendes, verändertes,
nicht-READY oder anders kodiertes Objekt endet mit kontrolliertem
Replay-Mismatch.

## Fehlervertrag

Ungültige/ungepaarte/doppelte Optionen, fehlende Capability, ungültige
Manifest-ID, Storefehler und Fehler bei der Ergebniskodierung werden vor
Workerstart abgewiesen, wenn sie vor dessen Ausführung feststellbar sind.
Eine nicht akzeptierte Antwort, ein Timeout, ein Sandbox-Abbruch oder ein
Output-Limit darf keine Ergebnisveröffentlichung auslösen. Fehlerantworten
enthalten keine Payload-, Pfad-, Store- oder Worker-Diagnosebytes.

## Abnahme

Die Umsetzung benötigt mindestens folgende unabhängige Tests:

1. Capability-Matrix für alte, reine Input-, reine Output- und kombinierte
   Pakete sowie frische gleichartige Bindungen vor Spawn.
2. Exakte CLI-Paarung, Duplikate, ungültige IDs und fehlende Berechtigung.
3. Erfolgreicher Import, kanonisches Einobjekt-Manifest, Idempotenz und
   abweichender Manifestkonflikt.
4. Keine Veröffentlichung bei abgelehnter Antwort, Timeout, Sandbox-/Worker-
   Fehler, Output-Limit oder Storefehler.
5. Replay für gültiges, fehlendes, verändertes und nicht-READY Ergebnisobjekt.
6. Bytekompatibilität aller bestehenden inputfreien und reinen
   Input-Payload-Records.
7. Worker-Probe, dass kein Storepfad, Manifestinhalt oder Schreib-FD sichtbar
   ist.

## Nichtziele

V1 führt keine Mehrfachausgaben, Ergebnisstreams, Überschreiben,
Garbage-Collection, Remote-Publish, externe Stores, Wiederholungen,
Plugin-zu-Plugin-Übergabe, parallele Plugin-Ausführung, freie Codecs oder
allgemeine Runtime-Orchestrierung ein.
