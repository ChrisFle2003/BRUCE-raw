# Artifact Store V2 — kontrollierter Mirror-Fetch

**Status:** **IMPLEMENTIERT UND BELEGT** — freigegebener V2-Vertrag mit
Implementierungsbelegen aus Core-, CLI- und realen Kompatibilitätstests.
Die unabhängige Gesamtabnahme bleibt ein eigener abschließender Prüfschritt;
die Spezifikation selbst ist kein Laufzeitbeleg.

## Ziel

Artifact Store V2 ergänzt den bestehenden lokalen, content-addressed Artifact
Store V1 durch einen kontrollierten read-only Fetch aus ausdrücklich
konfigurierten Dateisystem-Mirrors. BRUCE materialisiert nur ein vorher
identifiziertes Objekt in den lokalen V1-Store. Danach bleiben bestehende
Resolve-, Payload- und Workerpfade unverändert und arbeiten ausschließlich
mit der lokalen, vollständig verifizierten Kopie.

V2 ist ein begrenzter Vorläufer möglicher späterer Remote-Provider. Es ist
weder ein frei konfigurierbarer Downloader noch ein Netzwerkdienst.

## Bestehende Grenzen

**IMPLEMENTIERT UND BELEGT:** Artifact Store V1 speichert lokale Objekte
content-addressed und prüft Digest, Größe, Berechtigungen, Dateityp und
Linkanzahl. V1.2 löst lokale Manifeste metadata-only auf. V1.3/V1.4 nutzen
verifizierte lokale Store-Objekte als begrenzte Payload-Quellen.

Belege:

- `include/bruce/artifact/artifact_store_v1.hpp`;
- `src/artifact/artifact_store_v1.cpp`;
- `docs/architecture/03-blueprint-consolidation.md`;
- `tests/unit/artifact_store_v1_tests.cpp`;
- `tests/integration/test_artifact_store_v1.py`.

**DOKUMENTIERT / KONZEPT:** Netzwerk-Provider, externe Zugangsdaten,
automatische Synchronisierung und direkte Worker-Netzwerkzugriffe.

## Öffentliche Schnittstelle

V2 ergänzt ausschließlich diese Route:

```text
bruce artifact-store-v2 fetch SHA256 SIZE --store LOCAL_STORE \
  --provider PROVIDER_ID --providers PROVIDERS.json
```

`SHA256` ist genau ein lowercase SHA-256-Digest, `SIZE` eine kanonische,
nichtnegative Dezimalzahl bis einschließlich `bruce::max_file_bytes`.
Die Obergrenze ist **32 MiB = 33.554.432 Bytes**. `LOCAL_STORE`, `PROVIDER_ID`
und `PROVIDERS.json` sind Pflichtwerte und genau einmal in der gezeigten
Optionsreihenfolge zulässig. Pfadwerte dürfen weder leer sein noch mit `-`
beginnen. Zusätzliche, umgeordnete, doppelte oder fehlende Argumente werden
vor jeglichem Dateizugriff mit `PROTOCOL_ERROR` abgewiesen.

V2 ändert keine Ausgabe, Syntax oder Semantik von `artifact-store-v1`,
`dag-scheduler-v5` oder vorhandenen Bundleformaten.

## Provider-Konfiguration

`PROVIDERS.json` ist eine Bruce-eigene, reguläre owner-only Datei mit genau
einem Hardlink, ohne Symlink und innerhalb der bestehenden Dateigrenze. Sie
ist kanonisches JSON mit dem festen Vertrag
`bruce-artifact-store-providers-v2` und einer deterministisch geordneten
Liste aus Provider-ID und absolutem Mirror-Root. Das durch die freigegebene
Implementierungsentscheidung präzisierte Schema ist exakt:

```json
{"contract":"bruce-artifact-store-providers-v2","providers":[{"id":"mirror-a","root":"/absolute/root"}]}
```

Darauf folgt genau ein LF, ohne sonstige Whitespaces. Die Felder stehen in
der gezeigten Reihenfolge; Provider sind nach eindeutiger ID streng
aufsteigend sortiert. Eine leere Liste ist gültig, erlaubt aber keinen Fetch.
IDs erfüllen `[A-Za-z0-9][A-Za-z0-9._-]{0,127}`. Roots enthalten weder NUL
noch `.`-/`..`-Komponenten, doppelte oder abschließende Separatoren; `/`
allein ist ungültig. Strings verwenden direktes UTF-8, escaped Quotes und
Backslashes sowie ausschließlich kleingeschriebenes `\u00xx` für
Kontrollzeichen; alternative Escape-Darstellungen sind ungültig.
Doppelte IDs, unbekannte Felder, alternative Feldreihenfolge oder
nichtkanonische JSON-Darstellungen sind ungültig. Das Lesen der Datei
öffnet noch keinen Mirror und keinen lokalen Store.

Ein Mirror-Root muss bei jedem Fetch existieren, owner-only sein und darf
keinen Symlink enthalten. Root, `objects/` und Digestpräfix sowie die reguläre
single-hardlink Objektdatei gehören der effektiven UID und sind owner-only.
Die Konfigurationsdatei gehört ebenfalls der effektiven UID; Symlinks in
ihrem Öffnungspfad sind unzulässig. Gemeinsame Vorfahren wie `/tmp` dürfen
geteilt sein, aber keine Symlinks enthalten. Sein Pfad wird ausschließlich aus der bereits
geprüften Konfiguration übernommen. Eine CLI kann daher weder einen freien
Root noch URL, Host, Port, Header, Credential oder alternativen Providerpfad
übergeben.

## Datenfluss

```text
Digest + Größe + Provider-ID
        ↓
kanonische Provider-Konfiguration und Mirror-Root prüfen
        ↓
objects/<erste-zwei-hex>/<sha256>.blob descriptor-relativ ableiten
        ↓
regular, owner-only, single-hardlink und Größenlimit prüfen
        ↓
bytes begrenzt in einen privaten lokalen temporären Bereich streamen
        ↓
SHA-256 und Größe vollständig vergleichen
        ↓
über ArtifactStoreV1 atomar und idempotent publizieren
        ↓
metadata-only Fetch-Bericht
```

Der Remote-Mirror liefert weder Manifeste noch Metadaten als Autorität. Der
vom Aufrufer erwartete Digest und die erwartete Größe sind die einzige
Objektidentität. Payloadbytes, private temporäre Pfade und Mirror-Pfade werden
niemals öffentlich ausgegeben.

## Erfolgsvertrag

Ein erfolgreicher Fetch schreibt genau ein kanonisches JSON-Objekt mit
abschließendem LF auf stdout, nichts auf stderr und Exit 0:

```json
{"contract":"artifact-store-fetch-v2","action":"fetch","status":"FETCHED","provider":"mirror-a","sha256":"<64-hex>","size_bytes":123}
```

Der Bericht wird erst nach der vollständigen Integritätsprüfung und lokalen
Publikation erzeugt. Wiederholter Fetch gleicher korrekter Bytes ist
idempotent und ergibt denselben Bericht.

## Fehler- und Sicherheitsmodell

Alle Fehler liefern ausschließlich
`{"contract":"artifact-store-fetch-v2","error":"PROTOCOL_ERROR"}` auf
stderr, leeres stdout und Exit 2. Dazu gehören ungültige Argumente,
Konfiguration, Provider, Berechtigungen, Pfade, Links, fehlende oder
nichtreguläre Objekte, Größenüberschreitungen, Digest-/Größenabweichungen und
Konflikte im lokalen Store.

Digest-/Größenfehler vor Publikation erzeugen kein neues sichtbares lokales
Objekt. Bestehende Objekte werden nicht überschrieben; ein korruptes oder
anderweitig konfliktbehaftetes Ziel bleibt erhalten und führt zum Fehler.

Die freigegebene Ledger-Entscheidung präzisiert die V1-Publikationsgrenze:
Nach einem `ArtifactPublishSyncFailed` kann V1 bereits atomar umbenannt haben.
V2 verifiziert dann das lokale Objekt frisch vollständig. Eine erfolgreiche
Prüfung bedeutet idempotenten Fetch-Erfolg; es erfolgt kein Rollback, der
ein fremdes oder zuvor vorhandenes Objekt löschen könnte. Scheitert auch
die unabhängige Prüfung durch einen weiteren I/O-Fehler, bleibt
`PROTOCOL_ERROR`, obwohl ein vollständiges Objekt bereits sichtbar sein
kann. Ein kategorisches Unsichtbarkeitsversprechen bei kombinierten
I/O-Fehlern oder eine zusätzliche Crash-Dauerhaftigkeit wird nicht gegeben.
Staging wird bei normaler Rückkehr und Exceptions bereinigt, nicht garantiert
bei abruptem Prozessende. V2 bietet Integrität und lokale Besitzgrenzen,
aber weder Verschlüsselung, Authentisierung noch Autorisierung gegenüber
einem externen Dienst oder Schutz vor einem bösartigen Prozess derselben UID.

## Tests und Abnahme

Die implementierten Core- und CLI-Verträge sowie realen Workflows umfassen:

- gültige leere, kleine und maximale Objekte;
- idempotente Wiederholung;
- unbekannten Provider sowie manipulierte oder nichtkanonische Konfiguration;
- Root-Escape, Symlink, Hardlink, unsichere Rechte und fehlendes Objekt;
- falschen Digest oder Größe ohne Teilpublikation;
- keine Pfad- oder Payload-Leaks in Erfolg und Fehler;
- unveränderte Bytes bestehender Artifact-Store-V1/V1.2-Routen;
- anschließendes V1.2-Resolve und V1.4-Inputmaterialisierung aus dem lokalen
  Store.

V2 erstellt keine Manifeste; für Resolve und V1.4 wird ein bereits lokal
vorhandenes Manifest verwendet. Der Kompatibilitätsworkflow entfernt nach
Fetch die Mirror-Kopie und prüft die weitere lokale Verarbeitung.
Belege: [Core](../../../src/artifact/artifact_store_v2.cpp),
[CLI](../../../apps/artifact_store_v2_cli.hpp),
[Core-Verträge](../../../tests/unit/artifact_store_v2_tests.cpp),
[CLI-Verträge](../../../tests/unit/artifact_store_v2_cli_tests.cpp) und
[reale Workflows](../../../tests/integration/test_artifact_store_v2.py).
Der unveränderte CLI-Beispielgenerator führt V2 nicht aus. Ausführungsbelege
und verbleibende Sandbox-/Git-Grenzen stehen im lokalen Abschlussbericht
`runs/artifact-store-v2-remote-fetch/reports/final-review.md`. Vorhandene
Cockpit-Sockettests sind in der eingeschränkten Sandbox nicht vollständig
ausführbar; ein solcher Gesamtlauf ist nicht als vollständig grün zu bewerten.

## Nichtziele

- HTTPS, S3, Redis, Datenbank- oder sonstige Netzwerk-Provider;
- URLs, Hosts, Ports, Header oder Credentials in CLI oder Konfiguration;
- Remote-Publish, -Delete, -GC, -Retention, Synchronisierung oder Caching;
- direkte Worker-Netzwerkzugriffe und automatische DAG-Nachladung;
- Änderungen an bestehenden Worker-Frames, V1/V1.2, V1.3/V1.4, Bundles oder
  CLI-Ausgaben;
- freie Code-Plugins, GPU-Ausführung, freie Sprache, allgemeines Beweisen
  oder übergreifende Runtime-Orchestrierung.
