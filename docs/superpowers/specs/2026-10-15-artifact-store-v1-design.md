# Artifact Store V1 — Design

**Status:** DOKUMENTIERT / KONZEPT — vom Benutzer für die Planungsphase
freigegeben; noch nicht implementiert.

## Ziel

Artifact Store V1 stellt einen lokalen, synchronen und unveränderlichen
Content-Addressed Store für bereits validierte Dateien bereit. Ein Objekt wird
durch `SHA-256` und seine exakte Bytezahl identifiziert. Der Store darf keine
Payloaddaten in Logs oder öffentlichen Metadatenberichten ausgeben und darf die
bestehenden V2-, V3-, V4- und V5-Verträge nicht verändern.

V1 ist ein lokaler Baustein für spätere Provenienz- und V5-Importpfade. Es ist
kein Netzwerkdienst, kein allgemeiner Payload-Downloader und kein
Recovery-Executor.

## Bestehende Belege und Anschlussgrenzen

**IMPLEMENTIERT UND BELEGT:** Der V5-Run-Baum speichert temporäre Dateien unter
`BUNDLE.run/inputs`, `BUNDLE.run/attempts` und `BUNDLE.run/control`. Der aktuelle
Inspector prüft diese Dateien rungebunden und read-only.

Belege:

- `apps/dag_scheduler_v5_cli.hpp`
- `include/bruce/worker/worker_boundary_v1.hpp`
- `include/bruce/worker/worker_run_log_v1.hpp`
- `src/worker/worker_run_inspector_v1.cpp`

**DOKUMENTIERT / KONZEPT:** Ein dauerhafter Store, externe Payloadreferenzen,
Payload-Download, GC und V5-Migration existieren vor V1 nicht.

## Architektur

```text
Quelle (Datei oder Bytes)
        |
        v
bounded read + SHA-256 + Größenprüfung
        |
        v
private temp object
        |
        v
fsync + atomic RENAME_NOREPLACE
        |
        v
objects/<digest-prefix>/<digest>.blob
```

Der Store besitzt keine veränderliche Objektversion. Wiederholtes Publizieren
identischer Bytes ist idempotent. Ein Ziel mit identischem Digest, aber anderer
Größe oder abweichenden Bytes ist ein Protokollfehler.

## Dateistruktur

```text
STORE/
├── objects/
│   └── <erste-zwei-hex>/<64-hex>.blob
├── manifests/
│   └── <opaque-manifest-id>.manifest
└── store.lock
```

V1 verwendet keinen Pflichtindex und keine automatische Bereinigung. Root,
Unterverzeichnisse und Objekte werden owner-only angelegt. Objektpfade werden
nicht aus frei übernommenen Pfaden zusammengesetzt; der Digest wird nach
strikter Grammatik validiert und selbst als Pfadkomponente erzeugt.

## C++-Schnittstelle

Die öffentliche V1-Schnittstelle liegt unter
`include/bruce/artifact/artifact_store_v1.hpp`.

```cpp
struct ArtifactIdV1 {
    std::string sha256;
    std::uint64_t size_bytes{};
};

struct ArtifactMetadataV1 {
    ArtifactIdV1 id;
    std::string media_type;
    std::string origin;
};

class ArtifactStoreV1 {
public:
    static ArtifactStoreV1 open(const std::filesystem::path& root);

    ArtifactIdV1 put_bytes(std::string_view bytes);
    ArtifactIdV1 put_file(const std::filesystem::path& source,
                          std::string_view expected_digest = {},
                          std::optional<std::uint64_t> expected_size = {});
    bool contains(const ArtifactIdV1& id) const;
    ArtifactMetadataV1 inspect(const ArtifactIdV1& id) const;
    void verify(const ArtifactIdV1& id) const;
};
```

`put_file` liest bounded und streamt Digest sowie Kopie. Ein erwarteter Digest
oder eine erwartete Größe müssen mit den gelesenen Bytes übereinstimmen. Die
öffentliche Schnittstelle gibt keine privaten Dateipfade zurück. Ein späterer
Reader erhält einen eigenen Vertrag und ist nicht Teil von V1.

## Store-Vertrag

1. Digest ist exakt 64 Kleinbuchstaben im Hexalphabet.
2. Größe ist nichtnegativ und liegt unter dem festgelegten V1-Objektlimit.
3. Quellen müssen reguläre Dateien ohne Symlink, mit owner-only-Rechten und
   genau einem Hardlink sein; Bytes-Eingaben werden vor der Veröffentlichung
   bounded verarbeitet.
4. Der Store folgt keinen Symlinks und akzeptiert keine Root-Escape-Pfade.
5. Temporäre Dateien liegen im Store-Zielbereich, sind owner-only und werden
   vor Veröffentlichung synchronisiert.
6. Veröffentlichung erfolgt exklusiv; vorhandene identische Objekte sind
   erfolgreich, andere Inhalte werden abgewiesen.
7. `verify` öffnet descriptor-relativ, prüft Typ, Hardlinkzahl, Größe und
   SHA-256 und veröffentlicht keine Bytes.
8. Fehler werden auf stabile Store-Fehlercodes abgebildet; OS-Pfade und
   interne Diagnosen erscheinen nicht in öffentlichen JSON-Antworten.

Das konkrete Objektlimit wird aus den bestehenden lokalen Datei-/Payload-
Grenzen abgeleitet und im Implementierungsplan als feste V1-Konstante
festgeschrieben. Es darf nicht aus einer unvalidierten Eingabe stammen.

## Manifest

V1 verwendet ein separates, kanonisches Manifest und ändert
`worker-run-log-v1` nicht. Das Manifest enthält ausschließlich Metadaten:

- Manifestvertrag und Version;
- opake `run_id` und `node_id` beziehungsweise Provenienz-ID;
- Attempt-ID, Status und Ergebniscode;
- `sha256` und `size_bytes` jedes Objekts;
- optionale, kontrollierte `media_type`- und `origin`-Tokens.

Es enthält weder Payloadbytes noch absolute Pfade, Kommandozeilen, PIDs,
Stderr, Geheimnisse oder frei übernommene JSONL-Rohzeilen. Veröffentlichung
erfolgt atomar und exklusiv. Das Manifest bindet die logische Referenz an
Digest und Größe, ohne den temporären V5-Pfad zur dauerhaften Identität zu
machen.

## CLI

V1 ergänzt ausschließlich eine neue, eigenständige Route:

```text
bruce artifact-store-v1 import FILE --store STORE
bruce artifact-store-v1 inspect DIGEST --store STORE
bruce artifact-store-v1 verify DIGEST --store STORE
```

`import` veröffentlicht ein Objekt und gibt nur den stabilen Digest-/Größen-
Metadatensatz aus. `inspect` gibt Metadaten ohne Payloadbytes oder private
Pfade aus. `verify` prüft Integrität und gibt bei Erfolg einen stabilen
Metadatensatz aus. Optionen, Pfade, Digests und Größen werden vor Zugriffen
kanonisch validiert. V1 akzeptiert keine URL, keinen Netzwerk-Provider und
keine freien Provideroptionen.

Die bestehende CLI-Ausgabe von V2, V3, V4 und V5 bleibt bytegleich. V5 erhält
keinen impliziten Importpfad; eine spätere Übernahme nach `ARTIFACT_ACCEPTED`
benötigt einen eigenen versionierten Vertrag.

## Fehler- und Sicherheitsmodell

- fehlende Store-Struktur, falsche Rechte, Symlink, Hardlink oder nichtreguläre
  Datei: kontrollierter Store-Fehler;
- Digest-/Größenfehler: Objekt wird nicht veröffentlicht;
- beschädigtes vorhandenes Objekt: `verify` schlägt fehl, kein Überschreiben;
- Pfad-/Argumentfehler: Usage-/Protocol-Fehler ohne OS-Text;
- Abbruch vor Rename: temporäre Datei bleibt privat oder wird kontrolliert
  entfernt; kein teilweise sichtbares Objekt;
- parallele Publikationen desselben Digest bleiben durch No-Replace und
  idempotente Nachprüfung deterministisch.

V1 bietet Integrität und lokale Besitz-/Pfadgrenzen, aber keine Verschlüsselung,
Authentifizierung, Autorisierung oder Vertraulichkeit.

## Tests und Abnahme

Der Vertrag wird durch Unit- und Integrationsprüfungen belegt:

- put/verify/contains für leere, kleine und maximale Dateien;
- idempotentes doppeltes Publizieren;
- falscher Digest, falsche Größe und Digest-Kollision;
- Truncation und beschädigte Objekte;
- Symlinks, Hardlinks, Root-Escape und nichtreguläre Quellen;
- atomare Veröffentlichung bei bestehendem Ziel;
- private Rechte und keine Payload-/Pfad-Leaks in CLI-Ausgaben;
- Manifest-Kanonizität und exklusive Veröffentlichung;
- bestehende V2/V3/V4/V5-Tests unverändert grün.

## Nicht Bestandteil von V1

- Payload-Download oder öffentliche Byteausgabe;
- automatische GC, Retention oder Löschung;
- Netzwerk, S3, Redis, Datenbank oder Remote-Store;
- Verschlüsselung, Benutzerautorisierung oder Multi-Host-Replikation;
- automatische V5-Run-Migration;
- Recovery, Replay, Worker-Steuerung oder Adapterausführung;
- freie Code-Plugins, GPU-/Netzwerkausführung, freie Sprache, allgemeines
  Beweisen oder übergreifende Runtime-Orchestrierung.
