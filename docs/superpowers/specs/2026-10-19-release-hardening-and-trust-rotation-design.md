# Release-Haertung, Abbruchgrenze und Trust-Rotation

**Status:** VORSCHLAG — fachlich im Dialog freigegeben, schriftliche Pruefung vor der Umsetzung ausstehend\
**Datum:** 2026-10-19

## Ziel

Dieser Entwurf schliesst die nach dem vollstaendigen Mod-System verbliebenen
Produktionsgrenzen in einer kontrollierten Reihenfolge:

1. den Cancel-vor-Acceptance-Race im Common Runtime Host beheben;
2. die tatsaechliche Git-Historie sicher feststellen und die Aenderungen erst
   danach versionieren;
3. Release-, Debug- und gezielte ASan-/UBSan-Pruefungen reproduzierbar machen;
4. die Built-in-Mod-Matrix als sichtbare, verpflichtende CI-Grenze ausfuehren;
5. Core-Releases offline signieren und installierte Mod-Truststores lokal,
   transaktional und rueckrollbar rotieren.

BRUCE bleibt ein lokales Programm fuer den aufrufenden Linux-Benutzer oder
Administrator. Es gibt keinen BRUCE-Systembenutzer, keinen Daemon, keine
zentrale Mod-Verwaltung und keine Mehrbenutzerkontrolle.

## Bestehender belegter Ausgangspunkt

Die folgenden Aussagen sind **IMPLEMENTIERT UND BELEGT**:

- Der Common Runtime Host prueft Cancel- und Fehler-Marker und protokolliert
  akzeptierte Worker-Artefakte.
- Der Batch-Pfad prueft die Abbruchgrenze erneut unmittelbar vor seiner
  Annahmeentscheidung.
- Das vollstaendige Mod-System baut in seinen vorgesehenen Varianten; alle
  Mod-spezifischen Tests des letzten frischen Release-Laufs waren erfolgreich.
- CMake registriert `builtin_mod_build_matrix` bei aktiviertem Testing bereits
  als CTest-Test. Der allgemeine CI-CTest-Lauf nimmt ihn indirekt mit.
- Das Installationspruefskript kann ein erzeugtes TGZ-Paket sicher entpacken
  und dessen installierten Inhalt pruefen.
- Mod-Pakete verwenden Ed25519-Signaturen und einen kanonischen V1-Truststore.
- Data- und Logic-Mod-Registries binden ihren Mod-Root an die exakten
  Truststorebytes und weisen einen stillen Wechsel ab.

Die folgenden Aussagen sind **IMPLEMENTIERT, ABER NICHT AKTUELL
VERIFIZIERT**:

- Die sieben statisch gebauten Worker-Fixtures funktionieren in ihren
  Sandbox- und Ressourcenbegrenzungsfaellen auch im endgueltigen
  Gesamtsystemstand. Sie werden deshalb vor Abschluss erneut separat geprueft.
- Die vorhandene CPack-Konfiguration erzeugt aus dem endgueltigen Stand ein
  vollstaendig durch `verify_install.py --package` akzeptiertes TGZ.

Die folgenden Bereiche sind derzeit **DOKUMENTIERT / KONZEPT**:

- eine Core-Release-Signatur samt produktivem Offline-Signierablauf;
- Truststore-Generationen, Aktivierung, Rollback und Schluesselrotation;
- eine eigene sichtbare CI-Stufe fuer die Built-in-Mod-Matrix;
- ein ausdruecklich abgegrenztes Sanitizer-Profil fuer dynamische Ziele.

Der parallel reproduzierte Cancel-Race ist ein Produktfehler: Unter Last kann
ein Cancel-Marker nach der letzten Vorpruefung entstehen, waehrend ein bereits
fertiges Worker-Ergebnis anschliessend noch als `ARTIFACT_ACCEPTED` angenommen
wird. Die bisherige Integrationstest-Synchronisation ist zusaetzlich
nichtdeterministisch. Beide Befunde werden getrennt behandelt.

## Nicht-Ziele

- keine automatische Aktualisierung oder Online-Verteilung;
- kein Release-Upload aus CI und kein CI-Zugriff auf produktive private
  Signierschluessel;
- keine Zertifikatsinfrastruktur, Schluesselserver oder Online-Revozierungs-
  liste;
- keine nachtraegliche Toetung bereits gestarteter Mod-Prozesse;
- keine Aenderung des bestehenden Mod-Paket- oder Signaturformats;
- keine automatische Migration eines bestehenden V1-Mod-Roots;
- keine allgemeine Sanitizer-Unterstuetzung fuer statische seccomp-/RLIMIT-
  Worker-Fixtures;
- keine Reparatur oder Neuerzeugung von Git-Metadaten auf einer maskierten
  oder ungeklaerten `.git`-Sicht;
- keine Loeschung alter Mod-, Trust- oder Release-Artefakte.

## Reihenfolge und harte Gates

Die Umsetzung erfolgt in dieser Reihenfolge:

1. Vor jeder Aenderung wird ausserhalb der versionierten Quellen ein Manifest
   und ein vollstaendiges Quellarchiv des Ausgangsstands erzeugt.
2. Der Cancel-Race wird testgetrieben geschlossen und separat belastet.
3. Die Git-Historie wird read-only auf dem Host festgestellt. Erst bei
   eindeutigem Bezug wird in einem isolierten Worktree versioniert.
4. Buildprofile und CI werden erweitert.
5. Release-Signatur und Trust-Rotation werden testgetrieben implementiert.
6. Der Endstand durchlaeuft frische Release-, Debug-, Sanitizer-, Paket-,
   Dokumentations- und Sicherheitspruefungen.

Ein Gate stoppt die nachfolgenden schreibenden Schritte, wenn seine
Voraussetzungen nicht eindeutig sind. Insbesondere darf eine unklare
Git-Historie nicht durch `git init`, kopierte `.git`-Verzeichnisse oder einen
erzwungenen Commit scheinbar geloest werden.

## 1. Cancel-vor-Acceptance-Grenze

### Verbindlicher Vertrag

Die letzte Cancel-Entscheidung liegt im Runtime Host unmittelbar vor dem
dauerhaften `ARTIFACT_ACCEPTED`-Eintrag. Eine fruehere Pruefung in einem CLI-
Loop oder vor einem Worker-Poll ist nur eine Optimierung und keine
Sicherheitsgrenze.

Der erfolgreiche letzte Abort-Probe ist der lineare Annahmezeitpunkt. Ein
Marker, der erst nach diesem Probe publiziert wird, ist ein spaeter Marker und
entwertet die begonnene Acceptance nicht. Dateisystempublikation und
Journal-Append koennen nicht atomar zusammenfallen; der Vertrag behauptet
deshalb kein unmoegliches Null-Zeitfenster zwischen beiden Operationen.

Fuer ein noch nicht akzeptiertes kanonisches `READY`-Ergebnis gilt:

- Liegt unmittelbar vor der Annahme ein gueltiger Cancel-Marker vor, erzeugt
  der Host genau einen `CANCEL_REQUESTED`-Eintrag und beendet den Versuch als
  `CANCELLED`.
- Das terminale Cancel-Ergebnis enthaelt keine Artefakt-Referenz, keinen
  Artefakt-Digest und keine Artefaktgroesse.
- Ein ungueltiger Abbruchmarker fuehrt zu `PROTOCOL_ERROR`, nicht zu
  `CANCELLED` und nicht zu einer Annahme.
- Ein bereits dauerhaft akzeptiertes Artefakt wird durch einen spaeteren
  Cancel-Marker nicht rueckwirkend entwertet.
- Wiederholte Polls oder Abschlussaufrufe erzeugen weder einen zweiten Cancel-
  noch einen zweiten Acceptance-Eintrag.

### Platzierung

Die Re-Pruefung gehoert in die gemeinsame Annahmegrenze des Host-Zustands,
nicht nur in `execute_single_node_v2`. Der Single-Node-Pfad wird damit an die
bereits im Batch-Pfad verwendete Entscheidung angenaehert. Gemeinsame Logik
darf faktorisiert werden, sofern Reihenfolge, Fehlerkanal und Journalbytes
unveraendert bleiben.

Der Host prueft in dieser Reihenfolge:

1. kanonische und vollstaendige Workerantwort;
2. aktuellen Abbruchmarker;
3. Cancel- oder Protokollfehlerentscheidung;
4. erst danach Artefaktannahme und `ARTIFACT_ACCEPTED`.

### Tests

- Ein deterministischer Test positioniert den Cancel-Marker an der letzten
  Annahmegrenze und belegt `CANCELLED` ohne Artefaktdaten.
- Ein zweiter Test belegt einen ungueltigen Marker an derselben Grenze als
  `PROTOCOL_ERROR`.
- Ein Positivtest belegt weiterhin die genau einmalige Annahme ohne Marker.
- Ein Nach-Acceptance-Test belegt, dass ein spaeter Marker den akzeptierten
  Zustand nicht umschreibt.
- Ein Grenztest belegt, dass ein nach dem finalen Probe, aber vor dem
  Acceptance-Append publizierter Marker als spaet gilt.
- Der bisherige SIGSTOP-/Zombie-Test darf als Belastungsfall bleiben, ist aber
  nicht mehr der alleinige deterministische Beleg.
- Eine wiederholte parallele Stresspruefung muss ohne Race reproduzierbar
  bestehen. Ein einzelner gruener Lauf reicht nicht als Abschlussbeleg.

Produktive Test-Hooks oder kuenstliche Sleeps werden nicht eingefuehrt.

## 2. Git-Historie und sichere Versionierung

### Beobachteter Zustand

Die aktuelle Arbeitsumgebung zeigt `.git` als leere, schreibgeschuetzte
Sandbox-Maske; `git status` meldet deshalb, dass kein Repository erkannt wird.
Dieser Befund beweist weder den Verlust noch die Unversehrtheit der
Host-Metadaten. Ein benachbartes Repository besitzt zwar eine intakte
Historie, unterscheidet sich aber erheblich vom aktuellen Quellbaum und ist
kein automatischer Herkunftsnachweis.

### Vorgehen

- Vor weiteren Produktveraenderungen entstehen unter einem eindeutigen
  temporaeren Pfad ein Dateimanifest mit SHA-256-Digests und ein vollstaendiges
  Archiv des sichtbaren Quellbaums. Geheimnisdateien und lokale Buildausgaben
  werden nach dokumentierten Regeln ausgeschlossen; die Ausschlussliste wird
  mitgespeichert.
- Die echte Host-Sicht auf `.git` wird ausschliesslich read-only untersucht:
  Repositorywurzel, HEAD, Remote-Hinweis, Objektintegritaet, Worktree-Bezug und
  Status.
- Ist sie intakt und gehoert eindeutig zum aktuellen Baum, wird eine neue
  Arbeitsbranch beziehungsweise ein verwalteter Worktree von genau dieser
  Historie verwendet.
- Ist nur eine andere Historie verfuegbar, wird ihre Verwandtschaft anhand
  nachweisbarer gemeinsamer Commits und eines vollstaendigen Pfadvergleichs
  bewertet. Sie wird nie blind in den aktuellen Baum kopiert.
- Bleibt die Zuordnung unklar, werden nur Archiv, Manifest und ein
  reproduzierbarer Patchsatz uebergeben. Es erfolgt kein Commit.

### Verbotene Abkuerzungen

- kein `git init` im aktuellen Baum;
- kein Schreiben in die maskierte `.git`-Sicht;
- kein Kopieren eines benachbarten `.git`-Verzeichnisses;
- kein Reset, Checkout oder Clean gegen unbekannte Nutzerarbeit;
- keine Behauptung eines sauberen Git-Status, solange Git ihn nicht selbst
  belegen kann.

## 3. Build- und Sanitizerprofile

### Release und Debug

Release und Debug werden jeweils aus einer vollstaendigen temporaeren Kopie
unter einem nativen Pfad ohne Leerzeichen und Doppelpunkt frisch konfiguriert.
Beide Profile bauen mit Tests und fuehren ihren vollstaendigen registrierten
CTest-Satz aus. Erstlauf und gezielte Wiederholung werden getrennt berichtet;
uebersprungene oder deaktivierte Tests bleiben sichtbar.

Die Built-in-Mod-Matrix erhaelt einen expliziten Buildtyp-Parameter. Im
Release-Profil prueft sie alle vorgesehenen ON/OFF-Kombinationen als Release,
im Debug-Profil als Debug. Dadurch ist ein Debug-CTest-Lauf tatsaechlich ein
Debug-Beleg und nicht teilweise ein verschachtelter Release-Beleg.

### ASan/UBSan

Eine CMake-Option aktiviert AddressSanitizer und UndefinedBehaviorSanitizer
nur fuer dynamisch gelinkte BRUCE-Produktziele und die dazu kompatiblen
Testziele. Die Option setzt Compile- und Linkflags gemeinsam und darf weder
Release-Pakete noch installierte Metadaten veraendern.

Ausgenommen bleiben die sieben bewusst statisch gelinkten Worker-Fixtures mit
seccomp-/RLIMIT-Vertraegen. Sie werden im selben Abschlusslauf in einem
separaten, nicht instrumentierten Profil gebaut und getestet. Die Ausnahme
wird nicht als Sanitizer-Erfolg ausgegeben.

Das Sanitizer-Profil scheitert bei:

- jedem ASan- oder UBSan-Befund;
- einem unerwarteten Prozessabbruch;
- einer unterdrueckten Meldung ohne dokumentierte, eng begrenzte Ursache;
- einem Test, der nur wegen fehlender Instrumentierung als bestanden gelten
  wuerde.

Globale `CMAKE_CXX_FLAGS` fuer den gesamten Baum sind keine zulaessige
Implementierung, weil sie die statischen Sicherheits-Fixtures unbrauchbar
machen koennen.

## 4. Verbindliche CI-Mod-Matrix

### Sichtbare Stufe

Der Release-CI-Job fuehrt `builtin_mod_build_matrix` als eigenen benannten
Schritt aus. Anschliessend laeuft der allgemeine CTest-Satz mit exakt diesem
Test ausgeschlossen. Damit bleibt die Matrix verpflichtend und sichtbar,
wird aber nicht doppelt ausgefuehrt.

Der Debug-Job fuehrt denselben Aufbau mit Debug-Konfiguration aus. Das
Sanitizer-Profil verwendet seinen kompatiblen Testumfang; die statischen
Fixtures erhalten einen getrennten nicht instrumentierten Schritt.

### Ressourcen und Parallelitaet

- Verschachtelte Matrix-Builds erhalten eine feste, CI-konfigurierbare obere
  Parallelitaetsgrenze.
- Der Ccache-Launcher des aeusseren Builds wird an verschachtelte Builds
  weitergereicht.
- Tests, die den lokalen MCP-Inspector-Port verwenden koennen, und die Matrix
  teilen einen CTest-Resource-Lock oder laufen explizit seriell. Ein fester
  Loopback-Port darf nicht parallel doppelt beansprucht werden.
- Sandbox- oder Loopback-Sperren werden als Umgebungsergebnis getrennt von
  Produktfehlern ausgewiesen und nur die betroffenen Tests werden mit der
  erforderlichen Berechtigung wiederholt.

### Mutationsbeleg

Ein CI-Vertragstest oder ein lokal ausgefuehrter Workflow-Test veraendert in
einer temporaeren Kopie absichtlich eine erwartete Built-in-Konfiguration und
belegt, dass genau die Matrixstufe fehlschlaegt. Die Mutation beruehrt keine
versionierten Originalquellen.

## 5. Core-Release-Signatur

### Trennung von CI und Signierung

CI erzeugt ein unsigniertes TGZ, prueft es mit
`scripts/verify_install.py --package` und publiziert es hoechstens als
kurzlebiges Buildartefakt. CI signiert und veroeffentlicht keinen produktiven
Release.

Der produktive Ablauf ist lokal und explizit:

1. das exakt gepruefte TGZ uebernehmen;
2. seinen SHA-256-Digest bestimmen;
3. mit einem offline gehaltenen Ed25519-Schluessel signieren;
4. den abgeleiteten Public Key gegen einen unabhaengig verwahrten oder im
   Quellstand gepinnten Release-Key pruefen;
5. Signaturumschlag und eine Kopie dieses oeffentlichen Release-Keys beilegen;
6. den vollstaendigen Satz in einem frischen Verzeichnis erneut gegen den
   unabhaengigen Trust-Anchor verifizieren;
7. erst danach manuell veroeffentlichen.

### Dateien

Zu `bruce-<version>-<platform>.tar.gz` gehoeren:

- `<archive>.sha256`: lowercase SHA-256, zwei Leerzeichen, exakter
  Archivbasename und LF;
- `<archive>.sig.json`: kanonischer Signaturumschlag;
- `release-key-v1.json`: kanonische oeffentliche Schluesselbeschreibung.

Der Signaturumschlag enthaelt exakt:

| Feld | Vertrag |
| --- | --- |
| `contract` | `bruce-release-signature-v1` |
| `version` | exakte BRUCE-Version aus dem geprueften Paket |
| `archive` | exakter Archivbasename ohne Pfad |
| `archive_sha256` | 64 lowercase Hexzeichen |
| `key_id` | SHA-256 der rohen Public-Key-Bytes als 64 lowercase Hexzeichen |
| `signature_ed25519` | 128 lowercase Hexzeichen |

Die Signatur verwendet eine domain-separierte kanonische Bytefolge aus
Vertragskennung, Version, Archivbasename, Archivdigest und Key-ID. Der
Verifier berechnet den Archivdigest selbst und vergleicht Dateiname, Version,
Digest, Key-ID und Signatur. Damit ist nicht nur irgendein Text, sondern das
konkrete gepruefte Archiv an den Umschlag gebunden.

`release-key-v1.json` enthaelt Vertrag, Key-ID und Ed25519-Public-Key. Die
Key-ID ist zugleich der vollstaendige SHA-256-Fingerprint der rohen
Public-Key-Bytes; es gibt keine zweite, moeglicherweise abweichende
Fingerprintdarstellung. Die Datei enthaelt keinen privaten Wert.

Die dem Release beiliegende Datei ist nur Transportmetadatum und kein eigener
Trust-Anchor. Der Verifier verlangt zusaetzlich einen unabhaengig bezogenen,
gepinnten Public Key und vergleicht die beiliegende Datei byte- und
semantikgenau damit. Der produktive Public Key wird vor dem ersten Release in
einem getrennten Schluesselzeremonie-Schritt im Repository gepinnt; der
private Seed bleibt offline. Ohne diesen Schritt kann die Werkzeuglogik
getestet, aber kein offizieller Release als vertraut bezeichnet werden.

### Private-Key-Datei

Der Signierer akzeptiert den privaten Seed nur ueber einen expliziten Pfad.
Die Datei ist regulaer, kein Symlink, gehoert dem aktuellen Benutzer, besitzt
genau einen Hardlink und ist nur fuer den Besitzer les- und schreibbar. Ihr
kanonischer Inhalt lautet:

```text
BRUCE_RELEASE_PRIVATE_KEY_V1
<64 lowercase hex>
```

Umgebungsvariablen, Kommandozeilenwerte und stdin sind fuer den produktiven
Release-Key ausgeschlossen. Der Pfad oder Seed erscheint weder in Erfolgs-
noch Fehlerausgaben. Der Signierer ueberschreibt vorhandene Ausgaben nicht.
Testschluessel duerfen nur in Test-Fixtures liegen und werden eindeutig als
nicht produktiv gekennzeichnet.

Die zu signierende Version kommt nicht aus einem freien CLI-Argument. Der
Signierer liest Archivname, Archivdigest und Version aus dem Bericht der
erfolgreichen Installations-/Paketpruefung und gleicht Digest und Name erneut
gegen die exakten Archivbytes ab.

Der Signierer publiziert einen kompletten Release-Satz als neues Verzeichnis:
Archivkopie, Checksumme, Signatur, Public-Key-Kopie und Commit-Manifest. Alle
Dateien werden zuerst in einem privaten Geschwisterverzeichnis geschrieben
und synchronisiert; die abschliessende atomare Verzeichnisumbenennung ist der
Publikationspunkt. Ein Satz ohne Commit-Manifest ist unvollstaendig und wird
vom Verifier abgewiesen.

### Negativtests

- veraendertes Archiv;
- veraenderter Digest oder Dateiname;
- falsche Version oder Key-ID;
- falscher Public Key oder Fingerprint;
- nichtkanonisches, unvollstaendiges oder uebergrosses JSON;
- Symlink, Hardlink, falscher Besitzer oder falscher Modus beim Private Key;
- vorhandene Zieldatei;
- manipuliertes TGZ, das bereits die Installationspruefung nicht besteht.

Keine Fehlermeldung darf Seed, Signaturinternas oder beliebige lokale
Dateiinhalte offenlegen.

## 6. Lokale Mod-Trust-Rotation

### Geltungsbereich

Die Rotation gilt fuer dauerhafte installierte Data- und Logic-Mods in einem
lokalen Mod-Root. Der bestehende V1-Paketumschlag und dessen
`signing_key_id` bleiben unveraendert. Explizite session-basierte Logic-
Aufrufe mit separat angegebenem Truststore sind nicht Bestandteil dieses
persistenten Lebenszyklus.

### Unveraenderliche Generationen

Ein migrierter Mod-Root speichert Trust als unveraenderliche Generationen:

```text
trust/
  generations/
    <20-digit-generation>/
      truststore.pub
      metadata.json
  state.json
  journal.v1
```

`truststore.pub` bleibt bytegenau der vorhandene kanonische V1-Truststore.
`metadata.json` bindet Generation, Truststore-SHA-256, Erstellungszeitpunkt
aus der expliziten Operation und optional einen lokalen Kommentar. Zeit und
Kommentar beeinflussen die Vertrauensentscheidung nicht.

`state.json` benennt vorbereitete, aktive und vorherige Generation samt
Digest. `journal.v1` ist zusaetzlich die Koordinator-Autoritaet fuer einen
Trustwechsel und die damit verbundenen Data-/Logic-Aktivierungszustaende.
Snapshot und Journal werden mit
den bereits fuer Mod-Registries verwendeten privaten Dateisystemregeln,
Groessenlimits, Locking- und Crash-Recovery-Prinzipien behandelt. Pfade,
Symlinks und unerwartete Dateitypen werden vor einer Zustandsaenderung
abgewiesen.

Generationen werden nie in place geaendert. Eine bereits vorhandene
Generationsnummer mit anderen Bytes ist ein Integritaetsfehler.

Ein Trustwechsel verwendet unter dem gemeinsamen `registry.lock` eine
Root-Transaktion. Ihr Prepare-Eintrag bindet alten und neuen Trustdigest sowie
die exakten alten und neuen Data- und Logic-Snapshots. Der fsync-bestaetigte
Commit-Eintrag ist der einzige lineare Umschaltpunkt. Erst danach werden die
Data-/Logic-Journale, deren Snapshots, `state.json` und die rootweite
`truststore.pub`-Kompatibilitaetsprojektion idempotent auf den committed Stand
gebracht. Nach einem Crash vervollstaendigt der neue Host diese Projektionen
unter demselben Lock, bevor irgendeine Mod-Operation bedient wird; ein nur
vorbereiteter Wechsel wird nicht angewendet.

Der ausdrueckliche V1-Import schreibt zuerst einen fsync-bestaetigten
Coordinator-Prepare mit den exakten bisherigen Trustbytes. Danach ersetzt er
`truststore.pub` atomar durch einen exakt definierten, fuer V1 absichtlich
nicht parsebaren Migrations-Fence. Dieser eine rootweite Publikationspunkt
sperrt alte Data- und Logic-Clients gleichzeitig fail-closed. Erst danach
werden dauerhafte V2-Migrationsbarrieren in beide bestehenden Journale und der
Coordinator-Commit geschrieben. Aktuelle Parser erkennen Fence und Barrieren
und vervollstaendigen die Migration; aeltere BRUCE-Binaries lehnen den Root ab,
statt ihn unter Umgehung des Koordinators zu veraendern. Nach Commit und
vollstaendiger Projektion enthaelt `truststore.pub` wieder den aktiven
kanonischen V1-Truststore, waehrend die Journalbarrieren alte Binaryparser
weiterhin aussperren.

Ein Crash vor dem rootweiten Fence laesst den Legacy-Root unveraendert. Ab dem
fsync-bestaetigten Fence ist der Import absichtlich irreversibel und muss vom
V2-Host vervollstaendigt werden. Das gilt auch fuer Crashes zwischen den
beiden Journalbarrieren; zu keinem Zeitpunkt nach dem Fence darf ein alter
Registrytyp den Root noch erfolgreich oeffnen oder veraendern.

### CLI

```text
bruce mods trust status --mod-root ROOT
bruce mods trust import-v1 --mod-root ROOT
bruce mods trust prepare NEXT_TRUST.pub --mod-root ROOT
bruce mods trust verify --mod-root ROOT
bruce mods trust activate --mod-root ROOT [--deactivate-untrusted]
bruce mods trust rollback --mod-root ROOT [--deactivate-untrusted]
```

- `status` ist read-only und erzeugt keinen Mod-Root.
- `import-v1` ist der einzige Weg, den bereits an einen bestehenden, noch
  nicht migrierten Root gebundenen `truststore.pub` als Generation 1 zu
  uebernehmen. Ein externer Ersatztruststore ist fuer diesen Schritt nicht
  zulaessig. Stille Migration ist verboten.
- `prepare` validiert einen externen kanonischen V1-Truststore, legt die
  naechste Generation unveraenderlich an und aktiviert sie nicht.
- `verify` prueft Journal, Snapshot, alle referenzierten Generationen und die
  Integritaet aller installierten Pakete. Es liefert getrennt, welche Pakete
  dem aktiven und dem vorbereiteten Trust entsprechen. Inaktive historische
  Pakete unter einem entfernten Key sind ein Kompatibilitaetsbefund, aber kein
  Strukturfehler; nur aktive inkompatible Pakete blockieren den Wechsel
  standardmaessig.
- `activate` schaltet atomar auf die vorbereitete Generation.
- `rollback` schaltet atomar auf die vorherige Generation; die aktuell aktive
  Generation bleibt als unveraenderliches Objekt erhalten.

Alle mutierenden Befehle verwenden den gemeinsamen exklusiven Mod-Root-Lock.
Sie liefern kanonische JSON-Ergebnisse und stabile, dokumentierte Fehlercodes.
Jede installierende, aktualisierende, aktivierende, bindende oder ausfuehrende
Operation loest den aktiven Trust unter diesem Lock neu auf. Ein externer
Legacy-Truststore darf einen V2-Root weder ueberschreiben noch umgehen.

### Schutz aktiver Mods

Vor `activate` und `rollback` wird der prospektive aktive Mod-Baum vollstaendig
gegen die Zielgeneration geprueft.

- Verliert ein aktiver Mod sein Vertrauen, scheitert die Operation
  standardmaessig ohne Zustandsaenderung und listet nur Mod-ID, Version und
  Key-ID, keine lokalen Pfade.
- Mit dem ausdruecklichen Notfallflag `--deactivate-untrusted` werden alle
  betroffenen aktiven Mods und der Trustwechsel in einer einzigen
  journalgefuehrten Transaktion festgeschrieben.
- Kann diese Transaktion nicht vollstaendig committet werden, bleiben alter
  Trust und alte Aktivierungszustaende autoritativ.
- Bereits laufende Prozesse werden nicht beendet. Jeder neue Start und jede
  neue Aktivierung verwendet ausschliesslich den dann aktiven Trust.
- Neu installierte Versionen bleiben wie bisher inaktiv und muessen den
  aktuellen aktiven Trust passieren.

### Crash-Recovery

Tests unterbrechen jede dauerhafte Operation mindestens an diesen Grenzen:

1. nach Schreiben einer neuen Generation, vor Journal-Prepare;
2. nach Journal-Prepare, vor State-Snapshot;
3. nach State-Snapshot, vor Commit;
4. nach Commit, vor Aufraeumen temporaerer Dateien;
5. waehrend der atomaren Notfalldeaktivierung.

Nach erneutem Oeffnen existiert genau ein autoritativer Zustand: entweder der
alte vollstaendige Zustand oder der neue vollstaendige Zustand. Eine halbe
Trustumschaltung oder eine Mischung aus neuem Trust und alten aktiven Mods ist
unvertraglich.

Metadatenzeitpunkte verwenden kanonisches UTC-RFC-3339. Die Produktions-API
bezieht die Zeit aus einer expliziten Clock-Abhaengigkeit; Tests verwenden
einen festen Clock-Wert. Ein optionaler Kommentar ist explizite Eingabe und
wird nicht in Vergleiche einbezogen, die zeitunabhaengige Identitaet belegen.

## Fehler- und Ausgabegrenzen

Neue Fehler werden den bestehenden CLI-Konventionen zugeordnet und erhalten
stabile symbolische Codes. Mindestens unterschieden werden:

- ungueltiges Argument oder widerspruechliche Option;
- ungueltiger Release- oder Trust-Vertrag;
- kryptographische Verifikation fehlgeschlagen;
- unsicherer Dateityp, Besitz oder Modus;
- aktiver Mod verliert Vertrauen;
- keine vorbereitete beziehungsweise vorherige Generation;
- Journal-/Snapshot-Integritaetsfehler;
- Git-Historie nicht eindeutig zuordenbar;
- Umgebungs- oder Sandboxbeschraenkung.

stdout enthaelt bei maschinenlesbaren Befehlen ausschliesslich das
Vertragsergebnis. Diagnosen gehen nach stderr. Exitcode 0 erfordert immer die
semantisch validierte Ausgabe; ein leerer oder nur textueller Erfolg genuegt
nicht.

## Dokumentation

Aktualisiert werden nur die fuer die neuen Oberflaechen zustaendigen Quellen:

- CLI-Hilfe und `docs/cli/README.md` fuer Trustbefehle und Optionen;
- Mod- und Contract-Dokumentation fuer Generationen und Migration;
- Release-Dokumentation fuer TGZ-Pruefung und Offline-Signierung;
- CI-/Build-Dokumentation fuer Release, Debug, Sanitizer und Matrix;
- die bestehende Release-Signing-Konzeptseite, einschliesslich der veralteten
  Lizenzbehauptung, ohne daraus eine rechtliche Aussage abzuleiten.

Generierte CLI-Dateien werden ausschliesslich ueber
`docs/cli/verify_examples.py` in einer vollstaendigen temporaeren Kopie
erzeugt und byteweise gegen das Repository verglichen.

## Abnahme

Die Umsetzung gilt erst dann als abgeschlossen, wenn alle folgenden Belege
frisch vorliegen:

1. deterministische Cancel-Grenztests und wiederholter paralleler Stresstest;
2. dokumentierter Git-Herkunftsbefund oder ausdruecklich unversionierter
   Archiv-/Patch-Handoff;
3. vollstaendiger Release-CTest-Lauf;
4. vollstaendiger Debug-CTest-Lauf;
5. scoped ASan-/UBSan-Lauf ohne Befund;
6. separater Lauf der nicht instrumentierten statischen Worker-Fixtures;
7. eigenstaendige Built-in-Mod-Matrix und ihr Mutationsbeleg;
8. erzeugtes TGZ, erfolgreich geprueft mit `verify_install.py --package`;
9. Release-Signatur-Positiv- und Manipulationstests;
10. Trust-Prepare-, Verify-, Activate-, Rollback-, Notfall- und Crash-Tests;
11. Dokumentationsgenerator, lokale Markdown-Links und Beispielabgleich;
12. `git diff --check` und finaler Git-Status, sofern die echte Git-Historie
    nachweisbar verfuegbar ist; andernfalls ein byteweiser Vergleich gegen das
    anfangs erzeugte Manifest ohne vorgetaeuschte Git-Aussage;
13. abschliessende unabhaengige Code- und Vertragssicht.

Jeder Befund wird als Produktfehler, Testfehler,
Dokumentationsabweichung, Umgebungs-/Sandboxbeschraenkung, fehlende
Testabdeckung, nicht implementiertes Konzept oder offene Produktentscheidung
klassifiziert.

## Verbleibende offene Entscheidungen

Die folgenden Punkte sind absichtlich nicht Teil dieser Umsetzung und werden
erst nach den belegten lokalen V1-Ablaufen entschieden:

- Verteilungskanal und Aufbewahrungsdauer offizieller Release-Artefakte;
- organisatorische Verwahrung, Backup und personelle Freigabe des privaten
  Release-Schluessels;
- langfristige Release-Key-Rotation oder Mehrfachsignaturen;
- automatische Bereinigung alter Trustgenerationen oder Mod-Versionen;
- eine spaetere zentral verwaltete oder mehrbenutzerfaehige Installation.

Diese Punkte sind **OFFENE ENTSCHEIDUNG** und duerfen nicht als implementiert
oder durch den lokalen Ablauf bereits geloest beschrieben werden.
