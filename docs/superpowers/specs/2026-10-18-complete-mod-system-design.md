# Vollstaendiges lokales Mod-System

**Status:** VORSCHLAG — fachlich im Dialog freigegeben, schriftliche Pruefung vor der Umsetzung ausstehend
**Datum:** 2026-10-18

## Ziel

BRUCE erhaelt ein zusammenhaengendes lokales Mod-System fuer drei heute
getrennte Bereiche:

1. Die fest einkompilierten Mods Language, NanoCell und MCP werden in Hilfe,
   Buildvarianten, Katalog, Tests und Dokumentation konsistent abgeschlossen.
2. Signierte Data- und Logic-Mods erhalten einen dauerhaften lokalen
   Lebenszyklus zum Bauen, Pruefen, Installieren, Aktivieren, Aktualisieren,
   Deaktivieren und Zurueckrollen.
3. Eine gemeinsame, rein lesende Katalogsicht zeigt Built-ins und installierte
   Versionen, ohne ihre unterschiedlichen Vertrauens- und Lebenszyklusgrenzen
   zu verwischen.

BRUCE bleibt ein normales lokales CLI-Programm. Es laeuft mit den Rechten des
aufrufenden Betriebssystembenutzers. Es gibt keinen eigenen BRUCE-Benutzer,
keinen Dienst, keinen Daemon und keine Mehrbenutzerverwaltung.

## Bestehender belegter Ausgangspunkt

Die Umsetzung baut auf vorhandenem Code und aktuellen Tests auf:

- `builtin_mod_catalog_v1()` katalogisiert die tatsaechlich einkompilierten
  Built-ins.
- `RuntimeModRegistryV1` persistiert Data-Mod-Versionen in einem privaten,
  journalgefuehrten lokalen Mod-Root.
- `LogicModRegistryV1` prueft Logic-Pakete, Golden Cases, Aktivierungszustaende
  und frische Workerbindungen, ist derzeit aber absichtlich nur In-Memory.
- `bruce-mod-builder` baut signierte Data-Mod-Pakete aus kanonischen lokalen
  Quelldateien und einer owner-only `.env`.
- `logic-mod-plugin-v1 session` belegt die isolierte Workergrenze, ist jedoch
  kein dauerhafter Installationsweg.
- Der Built-in `mcp` ist ein lokaler read-only HTTP-Inspector. Der getrennte
  Python-Adapter `tools/bruce_mcp.py` ist ein Stdio-JSON-RPC/MCP-Server und
  kein Built-in-Mod.

Diese Punkte sind **IMPLEMENTIERT UND BELEGT**. Dauerhafte Logic-Installation,
ein gemeinsamer Produktkatalog, atomare Update-Komfortoperationen und ein
produktiver Logic-Paketbauer sind derzeit **DOKUMENTIERT / KONZEPT**.

## Produktgrenze

### Im Umfang

- lokale, explizit angegebene Paketdateien;
- fest einkompilierte Built-ins;
- lokale owner-only Mod-Roots und Truststores;
- mehrere unveraenderliche Versionen derselben installierten Mod-ID;
- hoechstens eine aktive installierte Version pro Mod-ID;
- getrennte Installation und Aktivierung;
- atomare Update-Komfortoperationen;
- Rollback durch Aktivierung einer bereits installierten Version;
- explizite Logic-Ausfuehrung ueber die bestehende isolierte Workergrenze;
- kanonische JSON-Berichte, reproduzierbare Tests und dokumentierte Formate.

### Nicht-Ziele

- Netzwerkdownloads oder URL-basierte Installation;
- automatische Updatepruefung oder automatische Aktivierung;
- Paketloeschung, Garbage-Collection oder Speicherbereinigung;
- systemweite Root-Installation;
- Hintergrunddienst, zentrale Verwaltung oder Mehrbenutzerbetrieb;
- Marketplace, Wasm oder hostgeladene Shared Libraries;
- Remote-Worker oder Remote-Payload-Fetch waehrend einer Ausfuehrung;
- implizite Ausfuehrung nach einer Route;
- Ersetzung der vorhandenen Built-ins durch installierbare Pakete;
- Aenderung historischer, ungebauter Cockpit-Fragmente.

## Grundentscheidungen

### Ein Katalog, zwei Herkunftsarten

Die gemeinsame Produktsicht kennt:

- `origin: "builtin"` fuer Language, NanoCell und MCP;
- `origin: "installed"` fuer lokal installierte Data- und Logic-Mods.

Built-ins bleiben Teil des Binaries. Sie koennen angezeigt, aber nicht mit
`install`, `update`, `activate`, `disable` oder `rollback` veraendert werden.
Installierte Mods bleiben signierte, unveraenderliche lokale Pakete.

### Explizite statt automatische Aktivierung

`install` legt eine neue Version inaktiv ab. `activate` schaltet ausdruecklich
um. `update` ist ein atomarer Komfortbefehl aus Pruefung, paralleler
Installation und Aktivierung. Alte Versionen bleiben installiert und koennen
ohne Neuinstallation wieder aktiviert werden.

### Lokale Pakete statt Download

Alle mutierenden Befehle arbeiten nur auf einem explizit angegebenen lokalen
Paket. BRUCE loest keine URL auf und erweitert den Vertrauensbereich nicht um
einen Downloader.

## Architektur

```text
                 compile-time feature inventory
                              |
                              v
                    Builtin catalog projection
                              |
                              +-------------------+
                                                  |
local signed data packages -> Data registry V1    |
local signed logic packages -> Durable Logic V1   +--> Unified Mod Catalog V2
                                                  |
                              +-------------------+
                              |
                              v
              explicit route / explicit logic execution
```

### Built-in-Featureinventar

Das Binary kennt immer die drei stabilen Built-in-IDs `language`, `nanocell`
und `mcp`. Fuer jede ID wird getrennt festgehalten, ob sie in diesem Build
vorhanden ist. Nur vorhandene Mods liefern ihr vollstaendiges
`ModManifestV1`; deaktivierte Mods liefern eine begrenzte bekannte Identitaet
und `build_state: "not_built"`.

Der bestehende `builtin_mod_catalog_v1()` behaelt seine Bedeutung: Er enthaelt
nur tatsaechlich gebaute und dispatchbare Mods. Eine neue Featureprojektion
liefert die drei bekannten IDs fuer die gemeinsame Statusanzeige. Dadurch
wird ein deaktivierter Mod nicht versehentlich als ausfuehrbar katalogisiert.

### Persistenz fuer Data- und Logic-Mods

Die bestehende Data-Mod-Persistenz bleibt bytekompatibel:

- `journal.v1` bleibt Commitautoritaet fuer Data-Mods;
- `registry.v1` bleibt der wiederherstellbare Data-Snapshot;
- bestehende `packages/<digest>.brucemod` bleiben unveraendert lesbar.

Logic-Mods erhalten eine parallele, aber durch dieselbe Lifecycle-Schicht
bediente Persistenz:

- `logic-journal.v1` als dauerhafte Commitautoritaet;
- `logic-registry.v1` als wiederherstellbarer kanonischer Snapshot;
- `logic-packages/<digest>.brucemod` fuer unveraenderliche Umschlagbytes;
- `logic-workers/<digest>/<worker-name>` fuer die geprueften, privaten
  ausfuehrbaren Workerbytes.

Data- und Logic-Operationen verwenden denselben vorhandenen exklusiven
`registry.lock`. Vor jedem Commit wird der prospektive gemeinsame aktive Baum
aus Core, aktiven Data-Mods und den eingebetteten Data-Fragmenten aktiver
Logic-Mods aufgebaut. ID-, Mount-, Template- und Golden-Case-Konflikte werden
dadurch vor der Zustandsaenderung abgewiesen.

Diese Trennung vermeidet eine stille Umschreibung bestehender V1-Registrybytes.
Die gemeinsame Katalogsicht ist eine Projektion beider Registryzustaende und
kein drittes persistiertes Zustandsformat.

### Gemeinsame Identitaetsregeln

- Built-in-IDs sind fuer installierte Pakete reserviert.
- Eine installierte Mod-ID darf nicht gleichzeitig Data- und Logic-Mod sein.
- `(mod_id, version)` ist innerhalb seiner Paketart eindeutig.
- Derselbe `(mod_id, version)` mit anderem Paketdigest ist ein Konflikt.
- Paketart, ID, Version und Digest werden aus geprueften Paketbytes abgeleitet,
  nie aus einem Dateinamen.
- Versionen werden als bestehende kanonische Identifikatoren behandelt; BRUCE
  fuehrt keine implizite SemVer-Sortierung oder `latest`-Auswahl ein.

## Gemeinsamer Katalog V2

`mods list` erzeugt einen kanonischen `bruce-mod-catalog-v2`-Bericht. Die
Eintraege sind stabil nach ID, Herkunft, Art und Version sortiert.

Jeder Eintrag enthaelt mindestens:

| Feld | Bedeutung |
| --- | --- |
| `id` | stabile Mod-ID |
| `version` | Built-in- oder Paketversion; bei nicht gebautem Built-in die bekannte Featureversion |
| `origin` | `builtin` oder `installed` |
| `kind` | `builtin`, `data` oder `logic` |
| `build_state` | `available` oder `not_built` fuer Built-ins, sonst null |
| `lifecycle_state` | installierter Zustand, fuer Built-ins null |
| `package_digest` | fuer installierte Mods, sonst null |
| `capabilities` | kanonisch sortierte deklarierte Faehigkeiten |

`mods list` ohne `--mod-root` zeigt nur Built-ins. Mit `--mod-root` wird ein
bereits vorhandener Mod-Root read-only geoeffnet und ergaenzt. Der Befehl legt
keine Registry und keine Lockdatei neu an. `mods status MOD_ID` verwendet
dieselbe Projektion, filtert auf eine ID und lehnt eine unbekannte ID mit einem
festen Fehler ab.

## Builder und Signaturgrenze

### Oeffentliche Builderoberflaeche

```text
bruce-mod-builder data SOURCE_DIR \
    --output PACKAGE.brucemod \
    --trust-store-output TRUST.pub \
    --signing-env FILE

bruce-mod-builder logic SOURCE_DIR \
    --output PACKAGE.brucemod \
    --trust-store-output TRUST.pub \
    --signing-env FILE
```

Der bestehende Data-Mod-Aufruf mit `--manifest`, `--tree`, `--golden`,
`--output` und `--trust-store-output` bleibt als Kompatibilitaetsform erhalten
und verwendet weiterhin `.env` im aktuellen Projektverzeichnis.

Neue Aufrufe verlangen eine explizite `--signing-env`-Datei. Sie muss regulaer,
owner-only, ohne Symlink und mit genau einem Hardlink vorliegen. Ihr Inhalt
bleibt die einzelne, nicht durch eine Shell ausgewertete Zuweisung
`BRUCE_MOD_ED25519_SEED_HEX=<64 lowercase hex>`.

### Data-Quellverzeichnis

Ein Data-Quellverzeichnis enthaelt exakt die erwarteten Eingaben:

- `manifest.json`;
- `tree.json`;
- `golden-cases.json`.

Unbekannte regulaere Dateien werden nicht in das Paket aufgenommen. Die drei
Quellen muessen die bestehenden kanonischen V1-Vertraege erfuellen.

### Logic-Quellverzeichnis

Ein Logic-Quellverzeichnis enthaelt:

- `data.brucemod`, ein bereits signiertes Data-Mod-Paket;
- `logic.json`, die bestehende kanonische Logic-Metadatei;
- genau den in `logic.json` benannten, bereits gebauten Worker.

Der Builder kompiliert keinen Quellcode, startet keine Shell und fuehrt kein
Buildskript aus. Er prueft den Worker als owner-eigene regulaere Datei, bindet
seine exakten Bytes und erzeugt den vorhandenen Logic-Mod-Umschlag. Der aus
dem Seed abgeleitete oeffentliche Schluessel muss zum `signing_key_id` des
eingebetteten Data-Pakets passen und beide Signaturen pruefen koennen.

### Ausgabe und Geheimnisse

- Paket und Truststore werden exklusiv und ohne Ueberschreiben publiziert.
- Ordinary failures rollen nur Dateien dieser Builderinvokation zurueck.
- Der Seed wird nach Verwendung bereinigt und nie ausgegeben.
- Erfolgsausgaben enthalten nur Vertrag, Art, ID, Version, Key-ID und Digest.
- Fehler enthalten weder Pfade noch verschachtelte Kryptographie- oder
  Dateisystemdiagnosen.

## CLI

### Inventar und Paketpruefung

```text
bruce mods list [--mod-root ROOT]
bruce mods status MOD_ID [--mod-root ROOT]
bruce mods inspect PACKAGE
bruce mods verify PACKAGE --trust-store TRUST
```

`inspect` erkennt Data- und Logic-Umschlaege strukturell und zeigt begrenzte
Metadaten, behauptet aber keine Vertrauenspruefung. `verify` erkennt die
Paketart, prueft alle zugehoerigen Signaturen und Schemata und gibt einen
kanonischen Verifikationsbericht aus.

### Lebenszyklus

```text
bruce mods install PACKAGE --mod-root ROOT --trust-store TRUST
bruce mods update PACKAGE --mod-root ROOT --trust-store TRUST
bruce mods activate MOD_ID VERSION --mod-root ROOT
bruce mods disable MOD_ID --mod-root ROOT
```

`install` publiziert eine neue Version als `installed_inactive`. Es veraendert
eine bereits aktive Version derselben ID nicht.

`activate` prueft Paket, Truststore, Workergrenze, gemeinsamen Baum und Golden
Cases frisch. Danach wird genau die gewaehlte Version `active`; eine vorher
aktive Version derselben ID wird `disabled`.

`disable` deaktiviert die aktive installierte Version. Built-ins sind kein
gueltiges Ziel.

`update` fuehrt unter einem gemeinsamen Lock aus:

1. Paketart erkennen und alle Paketbytes begrenzt lesen.
2. Signaturen, Schemata, Identitaet und Truststore pruefen.
3. Prospektiven gemeinsamen Katalog und aktiven Baum einschliesslich Golden
   Cases validieren.
4. Paket und bei Logic-Mods Workerbytes inhaltsadressiert und exklusiv
   publizieren.
5. Neue Version installieren, aktiv setzen und eine bisher aktive Version
   derselben ID deaktivieren.
6. Genau einen neuen Journalzustand der betroffenen Paketart committen.

Scheitert ein Schritt vor dem Journalcommit, bleibt der sichtbare aktive
Zustand unveraendert. Nach einem Prozessabbruch duerfen inhaltsadressierte,
nicht referenzierte Paket- oder Workerdateien verbleiben; sie werden nicht
automatisch geloescht und autorisieren keine Ausfuehrung. Nach erfolgreichem
Journalcommit rekonstruiert der bestehende Recovery-Stil den kanonischen
Snapshot.

Wird exakt dasselbe bereits aktive Paket erneut an `update` uebergeben, ist
die Operation idempotent erfolgreich und erhoeht die Generation nicht. Ist
dasselbe Paket installiert, aber inaktiv, aktiviert `update` es in einer
Zustandstransaktion. Ein abweichender Digest fuer dieselbe ID und Version wird
abgewiesen.

Rollback verwendet absichtlich keinen separaten Befehl und keine versteckte
Historie:

```text
bruce mods activate MOD_ID OLD_VERSION --mod-root ROOT
```

### Routing und Logic-Ausfuehrung

Der bestehende `route`-Befehl und `route-report-v1` bleiben unveraendert. Eine
neue Option erzeugt bei Bedarf die gemeinsame V2-Projektion:

```text
bruce route --text TEXT --mod-root ROOT --mod-details
```

Diese Form liefert `route-report-v2` und ergaenzt den ausgewaehlten Besitzer,
`origin` und `kind`. Ohne `--mod-details` bleiben bestehende Ausgabe- und
Replaybytes V1-kompatibel.

Eine Route fuehrt weiterhin nichts aus. Ein aktiver Logic-Mod wird nur
ausdruecklich gestartet:

```text
bruce mods run MOD_ID --template TEMPLATE --input TEXT --mod-root ROOT
```

`run` verlangt eine aktive installierte Logic-Version. Es oeffnet Registry,
Truststore, Paket und extrahierten Worker frisch, prueft ihre gebundene
Identitaet und fordert danach eine neue unveraenderliche `LogicModBindingV1`
an. Ausgefuehrt wird ausschliesslich die bestehende isolierte Workergrenze.
Data-Mods und Built-ins sind fuer `mods run` ungueltig. Built-ins behalten ihre
eigenen CLI-Befehle.

Die vorhandene `logic-mod-plugin-v1 session` bleibt kompatibel, wird aber in
der aktuellen Nutzerdokumentation als Low-Level-/Kompatibilitaetsroute statt
als dauerhafter Installationsweg eingeordnet.

## Fehler- und Sicherheitsvertrag

- Erfolgreiche CLI-Befehle schreiben genau ein kanonisches JSON-Objekt mit LF
  auf stdout und enden mit Exitcode 0.
- Erwartbare Argument-, Paket-, Trust-, Registry- und Zustandsfehler schreiben
  ein kompaktes JSON-Fehlerobjekt auf stderr und enden mit Exitcode 2.
- Builderfehler behalten ihre bewusst redigierte Fehlergrenze und geben keine
  geheimen oder vom Angreifer kontrollierten Pfade wieder.
- Mutierende Operationen verwenden den gemeinsamen exklusiven Lock.
- Mod-Root, Unterverzeichnisse, Journale, Snapshots, Pakete und Worker werden
  ohne Symlinkverfolgung und mit owner-only Anforderungen geoeffnet.
- Workerdateien bleiben regulaer, owner-eigen, single-link und nur in den
  bestehenden zulaessigen Ausfuehrungsmodi.
- Trust wird bei Erstellung des Mod-Roots explizit geliefert und privat
  gepinnt. Spaetere mutierende Befehle muessen denselben Truststore liefern;
  `open_existing` verwendet nur dessen bereits gepinnte oeffentliche Daten.
- Keine Ausgabe enthaelt Seeds, private Schluessel, rohe Workerbytes,
  Payloadbytes, Frames oder interne Hostpfade.
- Built-in- und Paket-IDs koennen sich nicht gegenseitig ueberschreiben.
- Eine Paketdatei, ein Worker oder ein Snapshot allein autorisiert nichts;
  nur ein gueltiger aktiver Registryzustand und eine frische Bindung erlauben
  eine Logic-Ausfuehrung.

## Built-in-Abschluss

### Hilfe und Dispatch

Jede optionale Befehlsfamilie steht unter derselben Compile-Time-Bedingung in
Hilfe und Dispatch. Insbesondere wird der heute unbedingte Language-Hilfetext
an `BRUCE_BUILD_MOD_LANGUAGE` gebunden. Nicht gebaute Befehle liefern
`UnknownCommand` und werden nicht als aufrufbar beworben.

### Gueltige Buildmatrix

| Language | NanoCell | MCP | Erwartung |
| ---: | ---: | ---: | --- |
| OFF | OFF | OFF | gueltig |
| ON | OFF | OFF | gueltig |
| OFF | ON | OFF | gueltig |
| ON | ON | OFF | gueltig |
| OFF | ON | ON | gueltig |
| ON | ON | ON | gueltig |

`MCP=ON, NanoCell=OFF` bleibt ungueltig und muss bei der Konfiguration mit
`McpRequiresNanoCell` scheitern.

### MCP-Abgrenzung

Der Built-in `mcp` bleibt der begrenzte lokale HTTP-Inspector. Er wird im
Built-in-Katalog als solcher beschrieben. Der Stdio-Adapter
`tools/bruce_mcp.py` bleibt ein getrennt konfiguriertes Integrationswerkzeug
mit seinen sieben MCP-Tools. Weder Katalog noch Dokumentation duerfen beide
Serverarten als identisch darstellen.

Die explizit historischen und ungebauten Cockpit-Fragmente bleiben an ihren
vorhandenen Pfaden unveraendert. Sie werden weder registriert noch in MCP-Code
zurueckmigriert.

## Dokumentation

- README, Architektur-, Implementierungs-, Contract- und CLI-Dokumentation
  unterscheiden Built-ins, installierte Data-Mods, installierte Logic-Mods,
  HTTP-MCP-Inspector und Stdio-MCP-Adapter.
- Neue Katalog-, Lifecycle-, Builder- und Logic-Ausfuehrungsvertraege werden
  in `docs/cli/commands.md` und `docs/cli/formats.md` dokumentiert.
- Konzeptdokumente bleiben Konzept und werden nicht als Implementierungsbeleg
  umformuliert.
- Die generierten Dateien `docs/cli/source-index.md`, `examples.json` und
  `validation.md` werden ausschliesslich durch `verify_examples.py` in einer
  vollstaendigen temporaeren Kopie erzeugt und anschliessend byteweise
  verglichen.
- Der Default-Build bleibt die Quelle fuer den kanonischen CLI-Snapshot.
  OFF-Builds erhalten eigene featureabhaengige Hilfe- und Dispatchtests; sie
  fuehren nicht blind den Default-MCP-Beispielsnapshot aus.

## Teststrategie

Die Umsetzung folgt testgetriebenen, einzeln abnehmbaren Schritten.

### Built-ins

- Hilfe und Dispatch stimmen fuer alle gueltigen Featurekombinationen ueberein.
- `mods list/status` melden `available` und `not_built` korrekt.
- Die bekannte OFF-Abweichung von `cli_documentation_determinism` wird zuerst
  als gezielter roter Regressionstest festgehalten und danach geschlossen.
- Die ungueltige MCP-ohne-NanoCell-Konfiguration bleibt rot bei CMake und gruen
  als erwarteter Testfall.

### Katalog und Persistenz

- kanonische Sortierung und Serialisierung;
- reservierte IDs und Data-/Logic-Kollisionen;
- leere, bestehende und korrupte Roots;
- vollstaendige und abgeschnittene Journale;
- Snapshot-Recovery und Journal-Digestketten;
- Idempotenz ohne Generationsaenderung;
- Aktivierungswechsel und Rollback;
- Abbruch vor und nach den einzelnen Publikationsgrenzen;
- prospektive Baum-, Mount- und Golden-Case-Konflikte.

### Builder

- Data- und Logic-Normalfall mit realer Signaturpruefung;
- fehlende, falsche, zu grosse oder nichtkanonische Quellen;
- unsichere Rechte, Symlink-, Hardlink-, FIFO- und Verzeichnistauschfaelle;
- Worker-/Capability-/Templateabweichungen;
- bereits vorhandene Ausgaben und kollidierende Ausgabeziele;
- Nachweis, dass keine Shell und kein Compiler gestartet werden;
- redigierte Fehler ohne Seed oder Pfadinhalt.

### CLI und Worker

- Normalfall, kleinste gueltige Eingabe und alle Pflichtargumente;
- falsche Paketart und unbekannte Built-in-/installierte IDs;
- Trustwechsel, Paketmutation, Workerersetzung und Registrymutation;
- erfolgreicher Updatewechsel, fehlgeschlagenes Update und Rollback;
- Data-Mod kann nicht ausgefuehrt werden;
- Built-in kann nicht mutiert oder ueber `mods run` gestartet werden;
- Logic-Ausfuehrung verwendet eine frische Bindingidentitaet und die bestehende
  isolierte Prozessgrenze;
- stdout, stderr, Exitcode und JSON-Vertragsfelder werden semantisch geprueft.

### Gesamtpruefung

1. Frischer Default-Release-Build mit allen Tests.
2. Frischer Debug-Build mit relevanten Tests und anschliessender Vollsuite,
   soweit Laufzeit und Umgebung dies tragen.
3. Alle sechs gueltigen Featurekombinationen konfigurieren, `bruce` bauen und
   ihre gezielten Katalog-/Hilfe-/Dispatchtests ausfuehren.
4. Ungueltige MCP/NanoCell-Kombination als erwarteten Konfigurationsfehler
   pruefen.
5. CLI-Dokumentation in einer temporaeren vollstaendigen Kopie regenerieren
   und byteweise vergleichen.
6. Lokale Markdown-Links, `git diff --check` und Git-Status pruefen, soweit
   funktionsfaehige Git-Metadaten vorhanden sind.
7. Socketabhaengige Tests bei einer Sandboxablehnung ausschliesslich gezielt
   mit der erforderlichen Berechtigung wiederholen.

## Umsetzungsetappen

### Etappe 1: Built-ins abschliessen

Featureinventar, bedingte Hilfe, Built-in-Katalogbericht, Buildmatrix und
featureabhaengige Dokumentationstests. Diese Etappe behebt die bereits
reproduzierte OFF-Build-Abweichung, ohne Paketformate anzufassen.

### Etappe 2: Gemeinsamer read-only Katalog

`bruce-mod-catalog-v2`, `mods list`, `mods status` und `mods inspect`. Data-V1
bleibt unveraendert; Logic-Persistenz wird noch nicht mutierend freigegeben.

### Etappe 3: Produktionsfaehiger Builder

Explizite Data-/Logic-Modi, sichere frei waehlbare Signing-Datei,
Paketinspektion und gemeinsame Verifikation. Der alte Data-Aufruf bleibt
kompatibel.

### Etappe 4: Dauerhafter Logic-Lebenszyklus

Logic-Journal, Snapshot, Paket-/Workerpublikation, gemeinsame Baumpruefung,
Aktivierung und explizites `mods run`.

### Etappe 5: Atomare Updates

`mods update`, Idempotenz, Crashgrenzen und Rollbackabnahme fuer beide
Paketarten.

### Etappe 6: Gesamtabnahme und Dokumentation

Vollstaendige Builds, Tests, generierte Artefakte, Links, Sicherheitsmatrix
und evidenzbasierte Aktualisierung der Implementierungsdokumentation.

## Abnahmekriterien

Das Vorhaben ist erst abgeschlossen, wenn:

- alle bestehenden und neuen relevanten Tests bestehen;
- jede gueltige Buildkombination ihr korrektes Hilfe-, Dispatch- und
  Built-in-Inventar liefert;
- je ein Data- und Logic-Mod gebaut, signiert, inspiziert, verifiziert,
  installiert, aktiviert und aktualisiert werden kann;
- ein fehlgeschlagenes Update den vorherigen sichtbaren Zustand unveraendert
  laesst;
- ein Rollback ohne Neuinstallation funktioniert;
- Built-ins und installierte Mods gemeinsam, aber mit eindeutiger Herkunft
  angezeigt werden;
- bestehende Data-V1-Pakete, Registrybytes, CLI-Aufrufe und Route-V1-Reports
  kompatibel bleiben;
- keine geheimen Werte, Payloads, Workerbytes oder internen Pfade in
  oeffentlichen Berichten erscheinen;
- die Default-CLI-Dokumentation deterministisch und alle lokalen Links gueltig
  sind;
- verbleibende Konzepte weiterhin ausdruecklich als Konzept gekennzeichnet
  sind.

## Nachfolgender Planungsschritt

Nach schriftlicher Freigabe dieser Spezifikation wird ein detaillierter
Implementierungsplan erstellt. Er zerlegt jede Etappe in testgetriebene,
ueberschaubare Aufgaben mit konkreten Dateien, roten Tests, Implementierung und
Verifikationsbefehlen. Vor dieser Planfreigabe wird kein Produktivcode
geaendert.
