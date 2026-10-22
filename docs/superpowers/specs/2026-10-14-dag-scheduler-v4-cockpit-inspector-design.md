# DAG-Scheduler V4 Cockpit Inspector — Design

**Status: VORSCHLAG — bestätigter Entwurf, vor Implementierungsplanung zu prüfen.**

## Ziel und Nutzerabsicht

Der bestehende lokale Cockpit-Inspector soll zusätzlich kanonische
DAG-Scheduler-V4-Laufbundles lesend erklären. Nutzerinnen und Nutzer sollen
ein mit `bruce dag-scheduler-v4 execute` erzeugtes Bundle hochladen und dessen
Graph, Receipts, Metadaten und logischen Trace verstehen können.

Die Erweiterung ist absichtlich keine Runtime-Steuerung: Sie führt keine
Callbacks aus, startet keinen Replay, schreibt keine Datei und speichert keinen
Upload. Sie erweitert weder den Callback-Katalog noch den Scheduler.

## Bestehende Grundlage

**IMPLEMENTIERT UND BELEGT:** Das Cockpit bindet ausschließlich an
`127.0.0.1`. Es bietet eine explizite Auswahl für Phase-DAG V2 und V3, je eine
versionsgebundene POST-Inspect-Route, sicheres textbasiertes DOM-Rendering und
eine generische Fehlerantwort. V2 bleibt auf 1 MiB begrenzt; V3 und der
Servertransport auf 5 MiB.

**IMPLEMENTIERT UND BELEGT:** DAG-Scheduler V4 kodiert eine V2-Spec inklusive
Requestbytes sowie Receipts inklusive direkter Input- und Output-Payloadbytes
in ein kanonisches, höchstens 5 MiB großes Bundle. Seine
`inspect_dag_scheduler_run_v4`-Schnittstelle prüft Header, Digest, Dekodierung,
Kanonizität und strukturelle Verträge ohne Callback-Ausführung. Der getrennte
V4-Replay führt dagegen den festen V3-Katalog aus und vergleicht das Ergebnis.

## Öffentliche Schnittstelle

### Neue Route

`POST /api/dag-scheduler-v4/inspect` akzeptiert ausschließlich ein komplettes
`BRUCE_DAG_SCHEDULER_RUN_V4`-Bundle mit höchstens 5 MiB.

- Ein gültiges Bundle ergibt HTTP 200 mit `status: VALID` und `code: VALID`.
- Ein ungültiges, manipuliertes, zu einer anderen Version gehörendes oder
  strukturell nicht kanonisches Bundle ergibt HTTP 422 mit der bestehenden
  generischen `REJECTED`-Antwort.
- Eine Anfrage oberhalb von 5 MiB ergibt HTTP 413 mit der bestehenden sicheren
  `REJECTED`-Antwort.
- Die Route ruft ausschließlich die V4-Inspect-Funktion auf; sie verwendet
  keinen Callback-Katalog und darf weder `replay_dag_scheduler_run_v4` noch
  `execute_dag_scheduler_v2` aufrufen.

`GET /`, `GET /api/v1/health`, `POST /api/v1/inspect` und
`POST /api/v3/inspect` bleiben in Verhalten und Format erhalten. Es gibt keine
automatische Formaterkennung und keinen V4-spezifischen Health-Endpunkt.

## Antwortmodell und Geheimnisgrenze

Eine erfolgreiche V4-Antwort enthält nur diese Metadaten:

- `status`, `code`, Scheduler-Version, DAG-ID, Startbudget und
  Receipt-Bytebudget;
- je Knoten: ID, Callback-ID und direkte Abhängigkeiten;
- je Receipt: Knoten-ID, Callback-ID, Request-Größe und -Digest, direkte
  Eingangsreferenzen mit Producer-ID, Result-Code, Payload-Größe und
  Payload-Digest, Outcome, Fehlercode sowie optionale Output-Metadaten;
- je Output: Version, Result-Code, Payload-Größe und Payload-Digest;
- je Traceeintrag: Ereignis und Knoten-ID.

Nicht enthalten sind Requestbytes, Input- oder Output-Payloadbytes,
Bundle-Rohbytes, Dateipfade, Decoderfehler, Exceptions oder Stack-Information.
`bundle_digest` ist nicht Teil der V4-Run-Struktur und wird nicht erfunden.

Eine erfolgreiche Inspect-Antwort bedeutet, dass das Bundle strukturell,
integriert und kanonisch gültig ist. Sie bedeutet nicht, dass die eingebetteten
Payloads aktuell erneut berechnet wurden, dass ihre fachliche Aussage wahr ist
oder dass eine Person autorisiert ist.

## Oberfläche und Ablauf

Die bestehende Auswahl erhält als dritte explizite Option „DAG-Scheduler V4“.
Sie setzt Titel, Zielroute und lokale Uploadgrenze auf den V4-Vertrag. Die
Auswahl entscheidet den Decoder; der Server rät nicht anhand eines Dateikopfs.

Nach erfolgreichem Upload zeigt die vorhandene UI:

1. **Übersicht:** Scheduler-Version, DAG-ID, Budgets und bestätigte
   Inspect-Grenze;
2. **Graph:** Knoten, Callback-ID und gerichtete Abhängigkeiten;
3. **Receipts:** Resultate, Fehler, Größen und Digests ohne Nutzdaten;
4. **Trace:** kanonische logische Ereignisfolge;
5. **Validation:** „strukturell und kanonisch geprüft; nicht erneut
   ausgeführt“.

Die bestehende Scheduler-Erklärung bleibt bei V4 ausgeblendet. V4 behauptet
weder parallele Wellen noch Laufzeiten. Reset entfernt ausgewählte Datei und
alle gerenderten Ansichten. Bundlebytes verbleiben nur bis zum Ende des
jeweiligen Requests im Browser-/Request-Speicher. DOM-Inhalte werden weiter
nur über `textContent` gesetzt.

## Komponenten und Verantwortlichkeiten

| Komponente | Verantwortung |
| --- | --- |
| `cockpit_inspector.hpp/.cpp` | V4-Inspect-Adapter und Metadatenprojektion ohne Rohbytes oder Reexecution |
| `cockpit_inspector_server.cpp` | feste V4-Route, HTTP-Status und Loopback-Serververtrag |
| `cockpit_inspector_assets.cpp` | explizite V4-Auswahl und sichere Darstellung in bestehenden Ansichten |
| Cockpit-Tests | V4-Gültigkeit, Ablehnung, Datenschutz, UI-Vertrag und V2/V3-Kompatibilität |
| Dokumentation | lokale, nur lesende V4-Inspect-Grenze und Bedienung |

## Fehlerbehandlung und Grenzen

V4-Decoderfehler, Digestfehler, nicht kanonische Daten, Größenüberschreitungen
und Strukturfehler werden ohne Details zu `REJECTED` normalisiert. Unerwartete
Serverausnahmen bleiben generisches HTTP 500 `ERROR`. Uploads werden weder
persistiert noch geloggt.

Das Cockpit bleibt lokal und bindet nur an `127.0.0.1`. Es gibt keine
Authentifizierung, Autorisierung, externe Payloadspeicher, Netzwerkworker,
Plugin-Ausführung, GPU-Ausführung, freie Sprache oder Runtime-Orchestrierung.
Diese Grenzen sind ausdrücklich keine Implementierungsbehauptung für spätere
Ausbaustufen.

## Testbare Abnahme

1. Ein real über `dag-scheduler-v4 execute` erzeugtes Bundle liefert über die
   neue Route HTTP 200 und die vereinbarten Metadaten.
2. Ein gültiges V4-Bundle wird ohne Callback- oder Replay-Ausführung inspiziert;
   ein strukturell gültiges, mit neuem Außen-Digest versehenes aber inhaltlich
   verändertes Bundle darf deshalb die Inspect-Grenze passieren, soweit der
   V4-Kerndecoder es akzeptiert.
3. Ein manipuliertes Header-, Digest-, Längen- oder nicht kanonisches Bundle
   wird als `REJECTED` zurückgewiesen.
4. V2- und V3-Bundles werden von der V4-Route kontrolliert abgelehnt;
   V4-Bundles bleiben an V2- und V3-Routen abgelehnt.
5. Übergröße ergibt HTTP 413. Der V2-eigene 1-MiB-Fachgrenzwert bleibt trotz
   des gemeinsamen 5-MiB-Servertransports wirksam.
6. JSON, HTML und Browser-DOM enthalten keine eingebetteten Request-, Input-
   oder Output-Payloadbytes, Dateipfade oder Ausnahmeinformationen.
7. V2- und V3-Cockpit-Routen, bestehende CLI-Verträge und V4-CLI-Ausgaben
   bleiben kompatibel.

## Außerhalb des Umfangs

Nicht Bestandteil sind eine Browser- oder Server-Replay-Aktion, Bundle-Export,
Dateipersistenz, Download von Payloads, automatische Formatwahl, Ausführung
freier Plugins/Code, externe Speicher, Nutzerverwaltung, Netzwerkzugriff,
GPU-Nutzung, Parallelplanung oder eine allgemeine Runtime-Orchestrierung.
