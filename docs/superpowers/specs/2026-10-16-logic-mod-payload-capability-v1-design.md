# Logic-Mod Payload Capability V1

**Status:** IMPLEMENTIERT UND BELEGT — Umsetzung und fokussierte Evidenz:
`tests/unit/logic_mod_payload_v1_tests.cpp`,
`tests/unit/logic_mod_payload_worker_v1_tests.cpp` und
`tests/integration/test_logic_mod_payload_v1.py`. Der historische Entwurf
bleibt die Designbegründung; die aktuelle Bedienung und das Byteformat stehen
im [CLI-Handbuch](../../cli/commands.md#logic-mod-plugin-v1--explizite-isolierte-sitzung)
und in den [Formaten](../../cli/formats.md#logic-mod-plugin-v1-paket-und-laufrecord).

## Ziel

Payload Capability V1 erlaubt einem aktiven Logic-Mod-Worker genau ein
vollständig verifiziertes lokales Artifact-Store-Objekt als read-only Input.
Der Worker erhält weder Dateipfad noch Storezugriff; BRUCE materialisiert die
Bytes in eine versiegelte Memfd, bindet deren Identität an Request, Ergebnis
und Replay und behält die volle Autorität.

## Öffentliche Auswahl

```text
logic-mod-plugin-v1 session PACKAGE --trust-store TRUST \
  --actions install,activate,run,inspect,replay \
  --input-artifact MANIFEST_ID:OBJECT_INDEX --store LOCAL_STORE
```

Ohne beide optionalen Werte gilt der unveränderte inputfreie V1-Lauf. Beide
müssen gemeinsam exakt einmal vorkommen. Es gibt keinen Pfad, keine URL, keine
zweite Referenz und keinen freien Codec.

## Bindung

Nur Plugins mit `artifact.input.v1` dürfen die Option verwenden. BRUCE öffnet
den bestehenden lokalen Store, löst `MANIFEST_ID` über V1.2 auf und akzeptiert
nur ein kanonisches `READY`-Manifest mit vorhandenem Objekt am
`OBJECT_INDEX`. Digest, Größe und Objektintegrität werden vollständig geprüft.
Die Größe darf höchstens 1 MiB betragen.

BRUCE kopiert die geprüften Bytes in eine versiegelte Memfd und übergibt sie
read-only auf FD 6. Der kanonische Workerrequest bindet `input_present`,
`input_sha256`, `input_size_bytes` und `input_codec=artifact-bytes-v1`; nie
einen Pfad. Seccomp erlaubt für FD 6 nur read, fstat, lseek und close.

## Replay und Fehler

Der Record bindet Manifest-ID, Digest, Größe und Codec. Replay liest und prüft
das lokale Objekt erneut; jede fehlende, nicht-READY, korrupte oder abweichende
Referenz schlägt kontrolliert fehl. Alle Referenz-, Capability-, Limit- und
Storefehler entstehen vor Workerstart und liefern kein Teilergebnis.

## Abnahme und Nichtziele

Tests belegen Capability-Gate, gültigen Input, 1-MiB-Grenze, sichere FD-6-
Lesbarkeit, Verweigerung weiterer Dateiöffnungen, manipulierte/fehlende
Objekte, Replaybindung und bytegleiche inputfreie V1-Läufe. V1 erweitert nicht
mehrere Inputs, Schreibzugriff, Streams, Remote-Fetch im Lauf, Worker-
Storezugriff, Pfade, freie Codecs oder Plugin-zu-Plugin-Datenfluss.
