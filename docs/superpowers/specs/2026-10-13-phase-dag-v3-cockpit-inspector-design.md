# Phase-DAG V3 Cockpit Inspector — Design

## Status und Ziel

**VORSCHLAG, zur Implementierung freigegeben:** Der bestehende lokale, read-only Cockpit-Inspector erhält eine additive Phase-DAG-V3-Ansicht.

Sie prüft gespeicherte V3-Bundles durch deterministisches Replay und zeigt nur Metadaten. V2 bleibt über seine bestehende Route und sein bestehendes Antwortformat vollständig kompatibel.

Das Ziel ist eine lokale Diagnoseansicht für einen konkreten V3-Lauf: Graphstruktur, Adapteridentitäten, direkte Knotenbeziehungen, Outcomes, Fehlercodes, Digests und Payloadgrößen sollen sichtbar sein. Payload-, Request- und Evidencebytes dürfen weder im JSON noch im Browser-DOM erscheinen.

## Bestehende Grundlage

**IMPLEMENTIERT UND BELEGT:** Der Cockpit bindet ausschließlich an 127.0.0.1 und exponiert GET /, GET /api/v1/health und POST /api/v1/inspect. Die V2-Inspection ruft Replay mit dem festen NanoCell-V2-Katalog auf, hält Uploadbytes nur im Request-Speicher und begrenzt V2 auf 1 MiB.

**IMPLEMENTIERT UND BELEGT:** Phase-DAG V3 hat eigene Bundlemarker, Typen, Replay und den festen NanoCell-V3-/Payload-Gate-V3-Katalog. V3-Bundles sind durch den Kern auf 5 MiB begrenzt; einzelne direkte Payloads auf 65536 Bytes.

## Öffentliche Schnittstellen

### Bestehende V2-Schnittstelle

- POST /api/v1/inspect bleibt unverändert.
- Sie akzeptiert höchstens 1 MiB.
- Ein V3-Bundle an dieser Route bleibt eine kontrollierte REJECTED-Antwort.
- Das V2-JSON und die V2-UI-Auswahl ändern sich fachlich nicht.

### Neue V3-Schnittstelle

- POST /api/v3/inspect akzeptiert ausschließlich ein vollständiges BRUCE_PHASE_DAG_RUN_V3-Bundle.
- Sie akzeptiert höchstens 5 MiB; größere Anfragen ergeben HTTP 413 mit dem bestehenden sicheren REJECTED-JSON.
- Ungültige, V2- oder manipulierte Bundles ergeben HTTP 422 mit status REJECTED und code REJECTED.
- Ein erfolgreiches Replay ergibt HTTP 200 mit status VALID und code VALID.
- Die Health-Schnittstelle bleibt unverändert; es gibt keinen V3-Health-Sonderpfad.

Der Server darf sein Transportlimit auf 5 MiB erhöhen, weil er beide Routen bedient. Die V2-Inspectorfunktion behält dennoch die eigene 1-MiB-Prüfung, damit die V2-Fachgrenze erhalten bleibt.

## V3-Antwortmodell

Die erfolgreiche Antwort enthält ausschließlich:

- status, code, dag_version und dag_id;
- je Knoten: node_id, adapter_id, adapter_version und dependencies;
- je Input: Producer-Knoten und -Adapter, Producer-Status, Output-, Evidence- und Payload-Digest sowie payload_size;
- je Output: output_version, Status, Output-, Evidence- und Payload-Digest, payload_size und claim_scope;
- je Receipt: node/adaptor identity, request_digest, input_refs, outcome, output oder null und failure_code;
- je Traceeintrag: event und node_id.

Nicht enthalten sind output_bytes, evidence_bytes, payload_bytes, request_bytes, die Inhalte von Kontextdateien und interne Exceptions.

Payloadgrößen sind die Byteanzahl der vollständig replay-verifizierten Payloadbytes. Sie sind eine Diagnosemetrik und keine Inhaltsdarstellung.

## UI und Ablauf

Die vorhandene lokale HTML-Seite enthält eine explizite Auswahl:

1. Phase-DAG V2;
2. Phase-DAG V3.

Die Auswahl bestimmt Uploadgröße, Zielroute und sichtbaren Titel. Es gibt keine automatische Erkennung anhand eines Dateikopfs. Nach einem erfolgreichen Upload zeigt die UI Übersicht, Graph, Receipts, Trace und Validierung. Bei V3 enthalten die Receipt-Ansichten Payloadgrößen und Digests, aber keine Nutzdaten. Reset entfernt Browserauswahl und alle DOM-Ansichten. Der Browser hält Bundlebytes nur bis zum Abschluss der jeweiligen Anfrage.

## Komponenten und Verantwortung

| Komponente | Verantwortung |
| --- | --- |
| cockpit_inspector.hpp/cpp | Eigene V3-Bundleprüfung und sichere V3-JSON-Projektion; V2-Vertrag erhalten |
| cockpit_inspector_server.cpp | Strikte Versionrouten, Statuscodes, Transportlimits und Loopback-only-Server |
| cockpit_inspector_assets.cpp | Explizite Versionwahl und text-sichere Metadatenanzeige |
| Cockpit-Tests | V2-Kompatibilität, V3-Replay, Grenzen, Kreuzablehnung und Datenschutz |
| Dokumentation | Bedienung, Grenzen und die explizite V2/V3-Route erklären |

## Fehlerbehandlung und Grenzen

- Jede Replay-, Decoder-, Katalog- oder Größenverletzung wird als REJECTED normalisiert. Rohfehlertexte, Bundleteile und Pfade werden nicht veröffentlicht.
- Ein serverinterner Ausnahmefehler bleibt HTTP 500 mit dem bestehenden generischen ERROR-JSON.
- Der Cockpit führt keine neue Phase-DAG aus, publiziert keine Bundles, startet keine Worker und speichert keine Uploads.
- Replay belegt kanonische Bundlekonsistenz, nicht Autorenidentität, Berechtigung oder allgemeine mathematische Korrektheit.
- Ein 5-MiB-V3-Upload ist nur im Request-Speicher zulässig. Es gibt keinen externen Payloadspeicher, keine Adaptererkennung und keine frei wählbaren Adapter.

## Testbare Abnahme

1. Ein erzeugtes NanoCell-V3-zu-Payload-Gate-V3-Bundle wird über POST /api/v3/inspect mit VALID und Metadaten ohne Rohbytes angezeigt.
2. Ein V2-Bundle wird über /api/v3/inspect kontrolliert abgelehnt; ein V3-Bundle über /api/v1/inspect ebenso.
3. Manipulierte V3-Payloadbytes oder -digests werden trotz möglicher Neuberechnung äußerer Integritätswerte beim Replay abgelehnt.
4. V3 akzeptiert 5 MiB nur innerhalb der Kern-/Bundlegrenzen und weist Übergröße als 413 zurück. V2 weist zwischen 1 und 5 MiB weiterhin als REJECTED zurück.
5. JSON- und Browserantworten enthalten keine Rohpayload-, Request- oder Evidencebytes und keinen Text aus solchen Feldern.
6. Bestehende V2-Cockpit-, V2-CLI-, V3-Kern- und V3-CLI-Verträge bleiben grün.
7. Der Server bleibt auf 127.0.0.1 beschränkt und registriert weiterhin nur die dokumentierten GET/POST-Routen.

## Außerhalb des Umfangs

Nicht Bestandteil sind parallele DAG-Ausführung, Cockpit-V3-Execution, Payload-Download, Suche, Speicher, externe Bindungen, Authentifizierung, Adapterplugins, GPU-Verarbeitung oder freie Spracheingabe.
