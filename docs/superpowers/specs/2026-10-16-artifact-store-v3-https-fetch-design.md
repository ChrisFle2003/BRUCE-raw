# Artifact Store V3 — kontrollierter HTTPS-Fetch

**Status: IMPLEMENTIERT UND BELEGT** durch
[Core-Verträge](../../../tests/unit/artifact_store_v3_tests.cpp),
[CLI-Verträge](../../../tests/unit/artifact_store_v3_cli_tests.cpp) und
[lokale TLS-/Kompatibilitätsworkflows](../../../tests/integration/test_artifact_store_v3.py).
Diese Spezifikation beschreibt die implementierte Grenze; sie allein ist
kein Laufzeitbeleg. Der CLI-Beispielgenerator führt V3 nicht aus.

## Ziel

Artifact Store V3 materialisiert genau ein vorher über Digest und Größe
identifiziertes Objekt aus einem allowlisted HTTPS-Provider in den bestehenden
lokalen Artifact Store V1. V3 übernimmt den V2-Integritäts- und
Materialisierungsvertrag, ersetzt jedoch den Dateisystem-Mirror durch einen
engen read-only HTTPS-Transport. Nach erfolgreicher Materialisierung verwenden
alle bestehenden Resolve-, Payload- und Workerpfade ausschließlich die lokale
V1-Kopie.

## Bestehende Grenzen

**IMPLEMENTIERT UND BELEGT:** V1 stellt lokale, atomare content-addressed
Publikation bereit. V2 liest streng konfigurierte Dateisystem-Mirrors und
bindet Digest/Größe, ohne bestehende V1/V1.2-Verträge zu ändern.

**IMPLEMENTIERT UND BELEGT:** V3 ergänzt den engen HTTPS-Fetch unten.
Weitere Netzwerkprovider, Zugangsdaten und direkte Worker-Netzwerkzugriffe
bleiben **DOKUMENTIERT / KONZEPT**.

## Öffentliche Schnittstelle

```text
bruce artifact-store-v3 fetch SHA256 SIZE --store LOCAL_STORE \
  --provider PROVIDER_ID --providers PROVIDERS.json
```

Die Argumentreihenfolge ist fest. `SHA256` ist genau 64 lowercase Hexzeichen;
`SIZE` ist eine kanonische nichtnegative Dezimalzahl bis einschließlich
`bruce::max_file_bytes` (32 MiB). Alle Argumente sind genau einmal Pflicht.
Zusätzliche, fehlende, doppelte oder umgeordnete Argumente werden vor jedem
Datei- oder Netzwerkzugriff abgewiesen.

V3 verändert weder V1/V1.2/V2-Ausgaben noch V5-, Bundle- oder Worker-Verträge.

## Provider-Konfiguration

`PROVIDERS.json` ist eine kanonische, owner-only, reguläre Datei mit einem
Hardlink und ohne Symlink. Ihr Vertrag ist exakt
`bruce-artifact-store-providers-v3`:

```json
{"contract":"bruce-artifact-store-providers-v3","providers":[{"id":"https-a","origin":"https://artifacts.example.org:443/bruce"}]}
```

Darauf folgt genau ein LF; die Feldreihenfolge ist fest, Provider-IDs sind
streng aufsteigend und eindeutig. Jede ID folgt der V2-Identifiergrammatik.
`origin` ist höchstens 2048 ASCII-Bytes lang: Schema exakt `https`, ein
lowercase DNS-Hostname oder kanonisches dotted-decimal IPv4, optional ein
dezimaler Port 1–65535 ohne führende Nullen und ein optionaler absoluter
Pfadpräfix. DNS-Namen haben höchstens 253 Zeichen, Labels höchstens 63,
ohne Rand-Bindestrich oder abschließenden Punkt. Pfadkomponenten enthalten
nur `[A-Za-z0-9._~-]`; `//`, Komponenten `.`/`..` und abschließender Slash
sind ungültig. Ohne Port gilt 443, explizites `:443` ist ebenfalls gültig.
Ohne Präfix ist `https://host` gültig, `https://host/` nicht. IPv6-Literale,
IPv4-Aliasse, Query, Fragment, Benutzerinformation, Backslashes,
JSON-Escapes, Prozentkodierung und alternative URL-Schreibweisen sind ungültig.

Der einzige Request lautet:

```text
GET <origin>/objects/<erste-zwei-hex>/<sha256>.blob
```

Der Aufrufer kann keine freie URL, Header, Cookie, Proxy, Credential,
Alternative Providerroute oder HTTP-Methode übergeben.

## Transport und Budgets

V3 verwendet den vorhandenen `third_party/cpp-httplib`-Client mit
OpenSSL-TLS und zwingender Zertifikats- sowie Hostnamenprüfung. Es gelten
OpenSSL-Standardvertrauenspfade einschließlich ihrer Prozessumgebung;
keine V3-Option erlaubt einen CA-Pfad oder Prüfungs-Bypass. V3 folgt keinen
Redirects; nur HTTP 200 ist erfolgreich. Ein Request, keine Wiederholung
und höchstens 32 MiB Antwortdaten sind zulässig. Ein Gate vor einer zweiten
Socket-Verbindung verhindert auch den Versuch weiterer aufgelöster
DNS-Adressen; ist die erste Adresse unerreichbar, scheitert der Fetch.
Das ist kein DNS-Pinning. `Content-Encoding` und doppelte `Content-Length`
werden abgewiesen. Die vendorten Grenzen sind 8 KiB je akzeptierter
Headerzeile, 100 Headerfelder und eine 32-KiB-Zeilenlesegrenze.

Die im Ausführungsledger beschlossene Produktionsgrenze ist ein überwachter
Linux-Hilfsprozess für DNS/TLS/HTTP mit privatem begrenztem IPC. Er schließt
geerbte Deskriptoren außer IPC mittels `close_range`; scheitert dies, bricht
er ab. Der Elternprozess hält allein die Store-Zugriffe und die absoluten
monotonen Fristen: 5 Sekunden ab Transportbeginn einschließlich DNS und
TLS-Authentisierung, 30 Sekunden insgesamt einschließlich Antwortframing,
TLS-Abbau und sauberem Kindprozessende. Die zweite Frist beginnt nicht nach
der Verbindung erneut. Fehler beenden den Kindprozess und warten mittels
`waitpid` auf sein Ende. Konfigurationslesen und lokale Publikation sind
keine Bestandteile dieser Transportbudgets. Der Prozess ist kein Sandbox-
oder Sicherheitsversprechen gegen einen bösartigen Prozess derselben UID.

Bytes werden in höchstens 64-KiB-IPC-Blöcken übertragen, im Elternprozess
bis höchstens zur erwarteten Größe gepuffert, nach vollständigem Transport
gegen erwartete Größe und SHA-256 geprüft und
erst anschließend über die bestehende V1-Publikation lokal materialisiert.
V3 übernimmt die V2-Regel für einen Post-Rename-`fsync`-Fehler: Ein frisch
vollständig verifiziertes lokales Objekt ist Erfolg; bei einer weiteren
Verifikationsstörung bleibt das Ergebnis Fehler. V3 behauptet keine
Crash-Dauerhaftigkeit außerhalb dieser bestehenden V1-Grenze.

## Öffentliche Ergebnisse

Erfolg schreibt exakt:

```json
{"contract":"artifact-store-fetch-v3","action":"fetch","status":"FETCHED","provider":"https-a","sha256":"<64-hex>","size_bytes":123}
```

auf stdout mit LF und Exit 0. Jeder Fehler schreibt ausschließlich
`{"contract":"artifact-store-fetch-v3","error":"PROTOCOL_ERROR"}` auf
stderr, leeres stdout und Exit 2. Öffentliche Ausgaben enthalten keine
Payloadbytes, lokalen Pfade, Origins, Hostnamen, TLS-/OS-Diagnosen oder
Teilresultate.

## Tests und Nichtziele

Ein interner Transport-Port ermöglicht Unit-Tests ohne Außenverbindung;
einzelne Core-Fristtests benötigen lokale Loopback-Sockets.
Integrationstests verwenden einen lokalen TLS-Testserver mit testweiser
Vertrauenswurzel und decken Erfolg, abgelehnte Zertifikate/Hostnamen, HTTP
Status, Redirect, Header/Stream/Timeout, Digest-/Größenfehler und keine
Teilpublikation ab. Sie prüfen außerdem V1/V1.2/V2-Bytekompatibilität und
V1.4-Nutzung nach erfolgreichem Fetch.

Die TLS-Zertifikate werden nur temporär für lokale Tests erzeugt; die Tests
verwenden `SSL_CERT_FILE`/`SSL_CERT_DIR` ohne Systemvertrauensänderung.
Ein vertrauenswürdiges Zertifikat mit falschem Hostnamen wird separat
abgewiesen. Öffentliche Internetprovider und DNS-Fault-Injection sind nicht
verifiziert; die dokumentierte DNS-Grenze ist zusätzlich aus dem Code geprüft.

Nicht Teil von V3 sind Authentisierung, Tokens, mTLS, Proxy, Redirects,
DNS-Pinning, Cache, Sync, Remote-Publish, direkte Worker-Netzwerkzugriffe,
GPU, Plugins und automatische DAG-Nachladung.
