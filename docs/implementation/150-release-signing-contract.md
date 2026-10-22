# Release-Identität und Offline-Signierung V1

**IMPLEMENTIERT UND BELEGT:** `bruce-release-tool` signiert und verifiziert
einen lokalen Release-Satz mit Ed25519. Belege sind der
[Signaturvertrag](../../include/bruce/release/release_signature_v1.hpp), die
[Implementierung](../../apps/bruce_release_tool.cpp), die
[Vertragstests](../../tests/unit/release_signature_v1_tests.cpp) und die
[Werkzeug-/CPack-Tests](../../tests/integration/test_release_tool.py).
Das Werkzeug benötigt explizite Dateipfade und keinen Netzwerkdienst.
BRUCE läuft als aufrufender Linux-Benutzer oder Administrator: kein eigener
BRUCE-Benutzer, Daemon oder Mehrbenutzerdienst.

## Abnahme und Offline-Ablauf

Ein frischer Release-Build und die relevanten Tests gehen dem Signieren voraus.
Die [Installationsprüfung](../../scripts/verify_install.py) prüft eine leere,
anschließend verschobene Installation und das sicher entpackte CPack-TGZ:
Version, Build-/Quelldaten, Export/Replay, Budgetfehler ohne Export und
historische Fixtures. Sie übergibt genau die geprüften Archivbytes und einen
`bruce-install-audit-v1`-Bericht:

```sh
python3 scripts/verify_install.py --build-dir BUILD --config Release \
  --fixtures tests/fixtures --version 0.102.0 --package \
  --archive-out /private/audit/tested.tar.gz --report /private/audit/audit.json
```

`BUILD` muss ein vollständiger frischer Build sein. Archiv- und Berichtsziel
dürfen nicht existieren. Der signierte Archivbasename muss dem im Bericht
stehenden CPack-Namen entsprechen, etwa
`bruce-core-0.102.0-Linux-x86_64.tar.gz`; bei einem abweichenden Übergabenamen
werden die geprüften Bytes unter diesen Namen verschoben, ohne CPack erneut
aufzurufen. Die Version ist kein frei wählbares Signierargument.

Auf dem Offline-Signierrechner:

```sh
bruce-release-tool sign --archive /private/audit/bruce-core-0.102.0-Linux-x86_64.tar.gz \
  --audit-report /private/audit/audit.json --private-key /offline/release.seed \
  --trusted-public-key /independent/pinned-release-key.json \
  --output-dir /private/new-release
bruce-release-tool verify --release-dir /private/new-release \
  --trusted-public-key /independent/pinned-release-key.json
```

Danach wird der vollständige Satz in ein frisches Verzeichnis kopiert, dort
erneut gegen den unabhängig gepinnten Schlüssel verifiziert und erst dann
manuell veröffentlicht. CI prüft Builds, Paket und Testschlüssel-Verträge;
sie erhält keinen produktiven privaten Schlüssel und signiert oder lädt keine
produktiven Releases hoch. Der Auditbericht ist lokale Abnahmeevidenz, keine
unabhängig authentifizierte Bescheinigung; der Signierer prüft seine Felder,
Archivname, Version und den selbst berechneten Archivdigest.

## Schlüssel und vollständiger Release-Satz

**IMPLEMENTIERT UND BELEGT:** Die private Schlüsseldatei ist regulär, kein
Symlink, gehört der effektiven Benutzer-ID des Signierers, hat exakt Modus
`0600` (einschließlich Ausschluss von Sonderbits) und genau einen Hardlink.
Diese Eigenschaften und der Dateisnapshot werden vor und nach dem Lesen
geprüft; auch Pfadkomponenten werden ohne Symlink-Auflösung geöffnet.
Der Inhalt ist exakt `BRUCE_RELEASE_PRIVATE_KEY_V1`, LF, 64 kleine Hexzeichen
des 32-Byte-Seeds und abschließendes LF. Der Seed kommt ausschließlich aus
dieser expliziten Datei, nicht aus einem Argumentwert, stdin oder einer
Umgebungsvariablen. Private Schlüssel gehören außerhalb von Quellbaum,
Buildausgaben und Release-Satz; die Test-Fixtures sind öffentlich und keine
produktiven Schlüssel. Ausgaben enthalten keine privaten Pfade oder Seeds.

Fehler liefern Exitcode `2`, leeres stdout und genau ein begrenztes kanonisches
JSON-Objekt auf stderr (`contract`, `error`, `ok:false`). Die feste öffentliche
Fehlermenge lautet:

| Code | Bedeutung |
| --- | --- |
| `ReleaseToolInvalidArguments` | fehlende, unbekannte oder widersprüchliche Optionen |
| `ReleaseToolInvalidContract` | ungültige Release-, Audit- oder Schlüsseldateiformate und Bindungsfelder |
| `ReleaseToolVerificationFailed` | Digest-, Signatur-, Kryptographie- oder Trust-Prüfung fehlgeschlagen |
| `ReleaseToolUnsafeFile` | Datei-/Pfadzugriff, Dateityp, Besitz, Modus, Lesesicherheit oder Publikation abgewiesen |
| `ReleaseToolFailed` | unerwarteter interner Fehler ohne öffentliche Detaildiagnose |

Interne Exceptiontexte, Pfade, Dateiinhalte und Schlüsselmaterial gelangen
nicht in diese Fehlerausgabe.

Der Signierer publiziert ein neues Verzeichnis mit diesen fünf Dateien:

| Datei | Inhalt |
| --- | --- |
| `<archive>` | Kopie der exakt geprüften TGZ-Bytes |
| `<archive>.sha256` | SHA-256, zwei Leerzeichen, exakter Basename, LF |
| `<archive>.sig.json` | kanonischer `bruce-release-signature-v1`-Umschlag |
| `release-key-v1.json` | kanonischer `bruce-release-key-v1`-Public-Key |
| `release-set-v1.json` | kanonisches Commit-Manifest `bruce-release-set-v1` |

Der Signaturumschlag bindet Vertragskennung, Version, Archivbasename,
Archiv-SHA-256 und Key-ID in einer domain-separierten kanonischen Bytefolge.
Die Key-ID ist SHA-256 der rohen Ed25519-Public-Key-Bytes. Das Commit-Manifest
bindet Archivname, Digest, Key-ID und Version. Alle Dateien entstehen zuerst
in einem privaten Geschwisterverzeichnis und werden synchronisiert; eine
atomare, nicht überschreibende Verzeichnisumbenennung publiziert den Satz.
Vorhandene Ziele werden abgewiesen. Ein Crash kann ein unvollständiges
Staging-Verzeichnis hinterlassen; ein Satz ohne vollständiges Commit-Manifest
ist nicht verifizierbar. Der read-only Verifier prüft alle fünf Komponenten,
berechnet den Archivdigest selbst und verlangt den unabhängigen Public-Key.

Die beiliegende Public-Key-Datei ist Transportmetadatum. Sie ersetzt niemals
den unabhängig bezogenen Trust-Anchor; auch ein vollständig durch einen
Angreifer ersetzter und neu signierter Satz muss gegen diesen scheitern.

**OFFENE ENTSCHEIDUNG:** Vor einem offiziellen Release muss eine ausdrückliche
Schlüsselzeremonie den produktiven öffentlichen Schlüssel und seinen
Fingerprint unabhängig bestätigen und pinnen. Die Werkzeugtests mit
öffentlichen RFC-8032-Fixtures belegen keine produktive Schlüsselzeremonie
und keinen offiziellen Release.

**DOKUMENTIERT / KONZEPT:** Online-Updates, Schlüsselserver und automatische
Veröffentlichung sind kein Bestandteil dieses Offline-Vertrags. Eine gültige
Signatur belegt die Bindung an den gepinnten Schlüssel und konkrete Bytes,
keine Sicherheits- oder Mathematikprüfung.

Der BRUCE-eigene Repositoryinhalt steht unter der
[BRUCE Proprietary License](../../LICENSE). Rechteinhaber ist Christopher
Flegel; siehe [NOTICE](../../NOTICE) und die getrennten
[Drittanbieterhinweise](../../THIRD_PARTY_NOTICES.md).
