# Artifact Store V4 – Hostseitiger Remote-Mirror

**Status:** IMPLEMENTIERT UND BELEGT — Host-Core, geschlossene CLI und lokale
HTTPS-Testworkflows sind vorhanden. Der [CLI-Vertrag](../../cli/commands.md#artifact-store-v4--hostseitiger-https-mirror-und-restore)
und das [Envelopeformat](../../cli/formats.md#artifact-store-v4-mirror-envelope-und-bericht)
beschreiben die belegten Grenzen.

V4 bietet hostseitiges Spiegeln und Wiederherstellen vollständiger lokaler
READY-Artefakte über HTTPS. Öffentliche Befehle sind
`artifact-store-v4 mirror MANIFEST_ID --store LOCAL_STORE --remote HTTPS_BASE`
und `artifact-store-v4 restore MANIFEST_ID --store LOCAL_STORE --remote HTTPS_BASE`.

Worker bleiben netzwerkfrei: Sie erhalten weiterhin nur bereits lokal
verifizierte, versiegelte FD-6-Snapshots. `mirror` lädt nur vollständige lokale
READY-Manifeste samt Objekten; Uploadfehler erhalten lokale Daten und lokale
DAG-Erfolge. `restore` handelt nur bei fehlendem lokalen Manifest, lädt exakt
gebundene Bytes, prüft Manifest-ID, SHA-256 und Größe und publiziert danach
lokal unveränderlich. Vorhandene lokale Daten werden nie ersetzt.

V4 verwendet HTTPS mit festen Zeit-/Bytegrenzen. URLs oder Zugangsdaten
erscheinen nicht in Workerframes, Records oder öffentlichem JSON. Nicht Teil
sind automatische DAG-Synchronisation, Credential-Speicherung, Remote-Löschung,
Multi-Writer-Konfliktauflösung, Peer-to-Peer oder Remote-Replay.

## Implementierte Grenzen und Belege

Mirror prüft sämtliche lokalen Bytes vor dem ersten PUT; Objekte werden vor
dem gebundenen Manifest-Envelope hochgeladen. Restore prüft den vollständigen
Download vor der ersten lokalen Publikation und publiziert das Manifest
zuletzt. Das Envelope ist auf 1 MiB, die Summe der referenzierten Objektbytes
auf 32 MiB und ein PUT-Antwortkörper auf 64 KiB begrenzt. Alle Requests teilen
eine absolute 30-Sekunden-Frist ab Operationsbeginn; pro Verbindung bleiben
höchstens 5 Sekunden für DNS, Connect und TLS. Lokale Dateisystemaufrufe sind
nicht präemptiv unterbrechbar.

GET verlangt 200, PUT erlaubt 200/201/204. 204 funktioniert ohne
Content-Length sowie mit genau `Content-Length: 0`; andere deklarierte
Längen, Transfer-Encoding, doppelte Content-Length und Content-Encoding
scheitern. Lokale Daten werden nie ersetzt. Ein Uploadfehler kann entfernte
Teilergebnisse hinterlassen; ein lokaler Publikationsfehler kann bereits
verifizierte Einträge hinterlassen. V4 ergänzt keinen transaktionalen
Rollback oder stärkeren Crash-Dauerhaftigkeitsvertrag.

Belege: [Core](../../../src/artifact/artifact_store_v4.cpp),
[CLI](../../../apps/artifact_store_v4_cli.hpp),
[Unit-Verträge](../../../tests/unit/artifact_store_v4_tests.cpp) und
[reale lokale HTTPS-Workflows](../../../tests/integration/test_artifact_store_v4.py)
einschließlich 204 ohne Content-Length. Die Tests verwenden temporäre
Loopback-TLS-Peers; öffentlicher Providerbetrieb ist damit nicht belegt.
Die generierten CLI-Beispiele führen V4 nicht aus.
