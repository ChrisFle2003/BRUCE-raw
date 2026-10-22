# Logic-Mod Artifact Chain V1

**Status:** IMPLEMENTIERT UND BELEGT — lokale Zwei-Schritt-Chain,
Core-/CLI-/Replay-Abnahme vom 2026-10-16; Gesamtsuite separat eingeschränkt.

## Ziel

Logic-Mod Artifact Chain V1 führt genau zwei signierte, lokal verifizierte
Logic-Mods seriell aus. Das akzeptierte Ergebnis des ersten Schritts wird vom
Host als Result Artifact V1 veröffentlicht, frisch aus demselben lokalen Store
aufgelöst und dem zweiten Schritt als verifizierter Payload auf FD 6
bereitgestellt. Kein Plugin erhält Zugriff auf das andere Plugin, den Store
oder eine Pfadangabe. Der bestehende Hostvalidator prüft jedes Ergebnis
gegen die jeweilige Texteingabe; dies ist kein allgemeiner Nachweis dafür,
wie B die Payloadbytes verwendet hat.

## Öffentliche Schnittstelle

```text
bruce logic-mod-chain-v1 run \
  FIRST_PACKAGE --first-trust FIRST_TRUST --first-input TEXT_A \
  SECOND_PACKAGE --second-trust SECOND_TRUST --second-input TEXT_B \
  --store LOCAL_STORE \
  --first-output-manifest MANIFEST_A \
  --second-output-manifest MANIFEST_B \
  --record CHAIN_RECORD

bruce logic-mod-chain-v1 replay \
  FIRST_PACKAGE --first-trust FIRST_TRUST --first-input TEXT_A \
  SECOND_PACKAGE --second-trust SECOND_TRUST --second-input TEXT_B \
  --store LOCAL_STORE \
  --record CHAIN_RECORD
```

`run` führt die Chain aus. `replay` prüft ausschließlich den eingebetteten
Chain-Record und die darin gebundenen lokalen Artefakte erneut; es startet
keinen Worker und benötigt deshalb keine Ausgabe-Manifeste. Jeder Positions- und
Optionswert kommt
genau einmal vor; unbekannte, leere, doppelte, umsortierte oder zusätzliche
Werte werden vor Paket-, Trust-, Store- oder Record-Zugriff abgewiesen.

`FIRST_PACKAGE`, `SECOND_PACKAGE`, beide Truststores, `LOCAL_STORE` und
`CHAIN_RECORD` sind Host-Konfiguration. Ihre Pfade erscheinen weder in
Workerrequests noch in Chain-Records, JSON-Antworten oder Fehlermeldungen.

## Autorisierung und Startreihenfolge

Schritt A benötigt die exakt sortierte Capability-Menge
`{artifact.output.v1, worker.execute}`. Schritt B benötigt die exakt sortierte
Capability-Menge `{artifact.input.v1, artifact.output.v1, worker.execute}`:
die Input-Capability berechtigt ausschließlich zum Lesen von A's READY-Objekt,
die Output-Capability ausschließlich zum Veröffentlichen von B's eigenem
Ergebnis nach `MANIFEST_B`. Beide Pakete werden separat
unverändert durch den bestehenden Discover-, Verify-, Golden-Test-, Install-
und Activate-Lebenszyklus geführt.

Der Host validiert alle CLI-Werte, beide Pakete, beide Truststores und beide
Ausgabe-Manifest-IDs vor dem ersten Workerstart. Er erteilt unmittelbar vor
jedem Start frische Ausführungs- und Artefaktberechtigungen an die jeweils
aktive Paket-/Registry-/Workeridentität.

Zuerst läuft nur A. Nach Hostakzeptanz veröffentlicht ausschließlich der Host
A's kanonisches `result` als Result Artifact V1 nach `MANIFEST_A`. Der Host
löst genau Objekt 0 dieses gerade veröffentlichten READY-Manifests frisch auf
und übergibt es B ausschließlich als bestehende Payload-Capability-V1-Memfd
auf FD 6. B darf keine andere Referenz wählen. Erst dann startet B und der
Host veröffentlicht sein akzeptiertes Ergebnis nach `MANIFEST_B`.

## Grenzen und Isolation

Die bestehenden Logic-Mod-Workergrenzen bleiben unverändert: jeder Worker
erhält nur seine festgelegten Standarddeskriptoren, den Exec-Gate-Mechanismus
und B zusätzlich die versiegelte read-only FD 6. Weder A noch B erhalten
Store-/Manifest-/Paket-/Truststore-FDs, Pfade, Netzwerk-, Shell-, Unterprozess-
oder Schreibrechte. Der Host übergibt nie A's privates `response_frame`,
sondern nur das veröffentlichte, verifizierte Ergebnisartefakt.

V1 unterstützt exakt zwei Schritte, einen lokalen Store, feste Reihenfolge,
ein Übergabeobjekt und je ein Ausgabeobjekt mit höchstens 16384 Bytes und
Codec `logic-mod-result-text-v1`. Nichtziele sind externe
Payloadinputs für A, Schleifen, Verzweigungen, mehr als zwei Schritte, Parallelität,
Retries, Netzwerk, Remote-Stores, Streams, freie Codecs, dynamische Auswahl,
Plugin-zu-Plugin-Prozesskommunikation und allgemeine Orchestrierung.

## Chain-Record und Replay

Nach dem erfolgreichen B-Ergebnis veröffentlicht der Host einen kanonischen,
privaten Chain-Record (höchstens 524288 Bytes, Modus 0600, atomar ohne
Ersetzen eines vorhandenen Ziels). Er enthält eine Chain-ID, beide eingebetteten
unveränderten Logic-Mod-Run-Records, die Paket-/Workeridentitäten beider
Schritte, `MANIFEST_A:0` als Übergabeidentität mit SHA-256/Größe/Codec sowie
`MANIFEST_B:0` als endgültige Ergebnisidentität. Die eingebetteten Run-Records
enthalten normalisierte Texteingaben,
Ergebnisse und vollständige Antwortframes; der Chain-Record ist deshalb ein
vertraulicher Klartextcontainer. Er enthält keine Hostpfade, Workerdiagnostik
oder separate Kopie der übertragenen Artifact-Payloadbytes. Nur die öffentliche
JSON-Ausgabe ist auf Status, Chain-ID und Artefaktmetadaten begrenzt.

Die Chain-ID ist SHA-256 des Headers
`BRUCE_LOGIC_MOD_ARTIFACT_CHAIN_ID_V1\n` und der längenpräfixierten
A-/B-Bindungsidentitäten, normalisierten A-/B-Eingaben und angeforderten
A-/B-Ausgabe-Manifest-IDs. Das separate kanonische Recordformat mit Header
`BRUCE_LOGIC_MOD_ARTIFACT_CHAIN_V1\n` enthält fünf längenpräfixierte Felder:
Chain-ID, A-Run-Record, B-Run-Record, A-Artefaktidentität und B-Artefaktidentität,
gefolgt von LF. Restbytes und nichtkanonische Serialisierung werden abgelehnt.
Das vollständige [Format](../../cli/formats.md#logic-mod-artifact-chain-v1-record-und-json)
und der [CLI-Vertrag](../../cli/commands.md#logic-mod-chain-v1--zwei-serielle-artifact-schritte)
beschreiben auch die privaten Dateiregeln.

Replay startet keinen Worker. Es prüft beide eingebetteten Run-Records über
die bestehenden Replayregeln, löst `MANIFEST_A:0` und `MANIFEST_B:0` frisch
aus dem lokalen Store auf und verlangt READY-Status, Digest, Größe, Codec und
die Bindung an die hostvalidierten A- beziehungsweise B-Ergebnisse. Jede
Abweichung endet kontrolliert als `CHAIN_REPLAY_MISMATCH`. Replay erzeugt
keinen Store und schreibt keine Artefakte oder Records. Es bestätigt
Transkriptkonsistenz unter aktueller Autorisierung, keine signierte historische
Ausführungsattestation.

## Fehlerverhalten

Fehler vor A verhindern jeden Start und jeden Chain-Record. Ein Fehler in A
verhindert B und den Chain-Record. Ein Fehler nach A, aber vor oder in B darf
das bereits unveränderliche A-Artefakt zurücklassen, jedoch kein B-
Ergebnismanifest und keinen Chain-Record. Scheitert der Chain-Record nach
erfolgreichem B, dürfen beide Artefakte bestehen; der Aufruf endet kontrolliert
ohne Erfolgsbericht; nach erfolgreichem atomarem Rename, aber fehlgeschlagenem
abschließendem Verzeichnis-fsync kann die Recorddatei bereits bestehen. Dies
ist keine Rollback-Transaktion über beide Artefakte und Record. Jeder
gestartete Worker wird beendet und gereapt.

## Abnahme

Tests müssen mindestens beweisen:

1. Erfolgsfall mit zwei getrennten signierten Paketen und der exakten
   A-zu-B-Artefaktidentität.
2. Capability-, Trust-, Paket- und Manifestfehler vor A ohne Workerstart.
3. A-Fehler ohne B-Start; B-Fehler ohne zweites Ergebnismanifest oder Record.
4. Manipulierte, fehlende oder nicht-READY Übergabe und Ergebnisartefakte.
5. Replay ohne Workerstart und mit vollständiger Identitätsprüfung.
6. Unveränderte Einzel-Plugin-, Payload- und Result-Artifact-Verträge.
7. Keine Pfad- oder Workerdiagnostiklecks, keine Rohbytes im öffentlichen
   JSON, keine separate Payloadkopie im Chain-Record und unveränderte
   Worker-FD-/Seccomp-Grenzen.

## Verifikationsstand 2026-10-16

**IMPLEMENTIERT UND BELEGT:** Frischer Release-Build erfolgreich; alle 20
Logic-Mod-CTest-Verträge/Workflows bestehen, einschließlich der drei
Chain-Core-/Preflight-/CLI-Targets. Ein unabhängiger frischer Gesamtreview
findet keine offenen Befunde und bestätigt 15/15 gezielt ausgewählte
Chain-/Plugin-/Worker-/Payload-/Result-Artifact-Prüfungen. Implementierungsbelege
sind der [Chain-Core](../../../src/mod/logic_mod_artifact_chain_v1.cpp),
die [CLI](../../../apps/logic_mod_artifact_chain_v1_cli.hpp),
die [Core-/Replay-/Vorabtests](../../../tests/unit/logic_mod_artifact_chain_v1_tests.cpp)
und die [Real-Binary-Workflows](../../../tests/integration/test_logic_mod_artifact_chain_v1.py).

Der vorhandene CLI-Verifier besteht ausschließlich in einer vollständigen
temporären Checkoutkopie: 167 Beispiele, 93 Optionen, 1012 Diagnosen, Links
OK. Er führt die Chain nicht aus; die generierten Originaldateien bleiben
unverändert. Unabhängiger Generator-Drift wird nicht in dieses Paket übernommen.
Die lokale Markdown-Dateilinkprüfung meldet 0 defekte Links; die geänderten
Dokumente und der lokale Abschlussbericht sind whitespacegeprüft.

**IMPLEMENTIERT, ABER NICHT AKTUELL VERIFIZIERT:** Ein vollständig grüner
Gesamtsuitenlauf wird nicht behauptet. Bestehende TLS-/HTTP-/Cockpit-Prüfungen
stoßen auf gesperrte lokale Sockets (`EPERM`). Die Gesamtsuite wurde gestartet;
ihr genauer Stand und die fehlenden Git-Metadaten stehen im lokalen Bericht
`runs/logic-mod-artifact-chain-v1/reports/final-review.md`. Der belegte Umfang bleibt die begrenzte lokale Zwei-Schritt-Chain.
