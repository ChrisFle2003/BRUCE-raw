# Forschungsbundle → Erfahrungsimport – Core 0.102.0

[Handbuch](README.md) · [Erfahrungsbefehle](commands.md#experience--unveränderliche-erfahrungssnapshots)

## Zweck und Ablauf

Die Übergabe beschreibt eine einzige Zielaktion: einen geprüften Forschungsbefund zu einem bestimmten Erfahrungssnapshot hinzufügen. Sie bindet die vollständigen Eingabebytes über SHA-256 und hält die erlaubten Eingabegrößen fest. Ein Plan ist weder eine Freigabe noch ein zwischengespeicherter Prüfnachweis. Seine Ausführung führt die fachlichen Quellen- und Zielprüfungen erneut aus.

```text
bruce experience plan-add --from-run RUN [--store PREVIOUS]
    [--max-source-bytes N] [--max-store-bytes N] --output PLAN
    [--compatibility-family transfer]
bruce experience apply-add PLAN --from-run RUN [--store PREVIOUS] --output NEW
    [--compatibility-family transfer]
```

`plan-add` führt den vorhandenen Importweg als Probelauf vollständig im Speicher aus: einschließlich Quellenreplay, Eignung, Duplikaten, bestehendem Snapshot und Ergebnisgrenzen. Nur bei Erfolg schreibt es einen neuen Plan. Der errechnete Erfahrungssnapshot wird dabei nicht exportiert. Kein vorhandener Speicher wird verändert.

`apply-add` liest den Plan, dann die ausdrücklich angegebenen Eingaben unter dessen Bytegrenzen. Es vergleicht zuerst den Quelldigest, anschließend den Digest des bisherigen Snapshots. Danach führt es den bestehenden Importweg erneut aus und exportiert einen neuen Snapshot exklusiv. Ein positiver Gatebericht und ein früher erfolgreicher Probelauf ersetzen keine Prüfung.

## Argumente und Grenzen

| Argument | Standard / Bedeutung |
| --- | --- |
| `--from-run RUN` | Pflicht; mathematisches Quellbundle, das der bestehende Erfahrungsimport akzeptiert |
| `--store PREVIOUS` | Optional; bisheriger Erfahrungssnapshot. Ohne Angabe sind die Basisbytes leer und bedeuten einen neuen Speicher |
| `--max-source-bytes N` | Nur bei `plan-add`; 0..1048576, Standard 1048576 |
| `--max-store-bytes N` | Nur bei `plan-add`; 0..1048576, Standard 1048576 |
| `--output` | Pflicht; neuer Plan bei `plan-add`, neuer Erfahrungssnapshot bei `apply-add` |
| `PLAN` | Bei `apply-add` genau vor den Optionen; maximal 4096 Bytes |
| `--compatibility-family transfer` | Optional auf beiden Routen; einziger gültiger Wert `transfer`; read-only Bestätigung vor dem jeweiligen Datei-/Plan-/Storezugriff |

Alle Optionen benötigen einen Wert. Pfade mit Leerzeichen zitieren. Eingabe-, Plan- und Ausgabepfade sind keine Identitätsmerkmale der Übergabe: andere Pfade mit exakt gleichen Bytes sind zulässig. Die Zielbasis wird durch ihren Inhalt gebunden. Der Ausgabepfad wird bei der Ausführung gewählt und muss neu sein; Elternverzeichnisse müssen existieren.

## Common-Runtime-Familienbestätigung

**IMPLEMENTIERT UND BELEGT:** `plan-add` und `apply-add` akzeptieren optional
exakt `--compatibility-family transfer`. Nach dem bestehenden Optionsparsing
wird der read-only Transfer-Descriptor validiert, bevor Quelle, Basis, Plan,
Regelspeicher oder Ausgabe gelesen beziehungsweise angelegt werden. Falsche
bekannte Familien (`session`, `gate`, `management`), unbekannte Werte,
Duplikate und fehlende Werte werden kontrolliert abgewiesen; ein vorhandener
Ausgabepfad bleibt dabei unverändert.

Die Bestätigung ändert keine Eingabeprüfung, Plan-/Snapshotbytes,
`controlled-transfer-v1`, Checkpointzählung, Cancellation, Fehlerhülle,
exklusive Veröffentlichung oder Ausgabe. Erfolgreiche Aufrufe bleiben
bytegleich zum Legacy-Aufruf ohne Option und enthalten keinen Descriptor oder
neuen Wrapper. Die gleiche Transferbestätigung ist außerdem auf den drei
weiteren mutierenden Experience-Routen `add`, `revoke` und `replace` sowie den
beiden `compatibility-*`-Routen verfügbar.

Eine Quellgrenze 0 blockiert jedes nichtleere Bundle. Eine Storegrenze 0 erlaubt die leere Basis. Die bisherigen fachlichen Grenzen des Erfahrungsimports gelten zusätzlich, einschließlich dessen Quellen-/Audit- und Snapshotgrenzen. Die neuen Optionen begrenzen Bytes, keine einheitlichen mathematischen Operationen, Prozess-RAM oder Laufzeit. Die allgemeinen Sitzungsoptionen werden nicht angeboten. Für die Ausführung gibt es seit Core 0.94 die separate [Transfersteuerung](transfer-control.md). Der Probelauf und die spätere Ausführung rechnen jeweils erneut und können entsprechend Aufwand verursachen.

## Ausgaben

`plan-add`: Exit 0 und ein JSON-Objekt `experience-add-transfer-v1` mit:

- `action`: immer `experience-add`;
- `source_digest`: SHA-256 der gesamten Quelldatei;
- `previous_digest`: SHA-256 der gesamten Basisdatei beziehungsweise der leeren Bytefolge;
- `max_source_bytes`, `max_store_bytes`: festgelegte Grenzen.

`apply-add`: Exit 0 und `experience-transfer-result-v1` mit `source_digest`, `previous_digest`, `plan_digest` (SHA-256 der vollständigen Planbytes), `snapshot_digest` (SHA-256 des exportierten Snapshots) und `result` (unveränderter nativer Erfahrungsbericht). Der Snapshot selbst besitzt das bestehende Experienceformat und kann mit `experience inspect` gelesen werden.

Erfolgreiche Berichte stehen als reines JSON auf stdout; normale Fehler als JSON auf stderr mit Exit 2. Beide Befehle sind nichtinteraktiv. Sie aktivieren oder ändern keine Regeln.

## Vollständiges Beispiel

Vom Projektstamm aus:

```bash
transfer_dir=$(mktemp -d)
printf 'x,y\n0,2\n1,3/2\n2,4/3\n3,5/4\n' > "$transfer_dir/train.csv"
printf 'x,y\n5,7/6\n' > "$transfer_dir/hold.csv"
./build/bruce research --strategy portfolio-rational --max-degree 2 --train "$transfer_dir/train.csv" --holdout "$transfer_dir/hold.csv" --output "$transfer_dir/source.bruce"
./build/bruce experience plan-add --from-run "$transfer_dir/source.bruce" --max-source-bytes 1048576 --max-store-bytes 0 --output "$transfer_dir/import.plan"
./build/bruce experience apply-add "$transfer_dir/import.plan" --from-run "$transfer_dir/source.bruce" --output "$transfer_dir/new.experience"
./build/bruce experience inspect "$transfer_dir/new.experience"
```

Für eine bestehende Basis bei beiden Schritten dieselben Snapshotbytes über `--store` angeben und eine ausreichende Storegrenze wählen. Ein zwischenzeitlich geänderter Store erfordert einen neuen Plan. Das Beispiel nutzt einen neuen temporären Ordner und überschreibt keine vorhandenen Dateien.

## Planformat

Der Textkörper besteht aus fünf LF-terminierten Zeilen:

```text
BRUCE_EXPERIENCE_ADD_TRANSFER_V1
<source_digest>
<previous_digest>
<max_source_bytes als Dezimalzahl>
<max_store_bytes als Dezimalzahl>
```

Die Datei beginnt mit SHA-256 dieses Körpers, gefolgt von LF und dem Körper. Digests sind 64 kleingeschriebene Hexzeichen. Der Decoder verlangt exakt diese kanonische Form; zusätzliche Felder, andere Versionen und abweichende Darstellung werden abgelehnt. Das Format enthält weder Quelldaten noch Dateipfade oder einen Gatebericht. Eingabedateien müssen zur späteren Ausführung verfügbar bleiben.

## Fehler und Änderungsfälle

| Fehler / Präfix | Bedeutung |
| --- | --- |
| `ExperienceTransferInvalidBudget` | Gewählte Bytegrenze überschreitet 1 MiB; negative/ungültige Zahlen scheitern bereits im Zahlenparser |
| `InputByteLimit` | Datei überschreitet beim CLI-Lesen die gewählte Grenze beziehungsweise 4096 Planbytes |
| `ExperienceTransferByteLimit` | Anwendungseinstieg erhält Eingabebytes über der Plangrenze |
| `ExperienceTransferPlanByteLimit` | Direkter Anwendungseinstieg erhält mehr als 4096 Planbytes |
| `ExperienceTransferDigestMismatch` | Äußere Planprüfsumme fehlt oder stimmt nicht |
| `InvalidExperienceTransfer` | Falsche Version, Felder, Digests, Grenzen oder nichtkanonisches Format |
| `ExperienceTransferSourceChanged` | Gelesene Quelle entspricht nicht der gebundenen Bytefolge |
| `ExperienceTransferBaseChanged` | Gelesener bisheriger Snapshot entspricht nicht der gebundenen Basis |
| Bestehende Replay-/Experiencefehler | Quelle oder Ziel ungeeignet, unvollständig, abhängig, doppelt oder Kapazität überschritten; kein Export |
| `ExportExistsOrUnavailable: …` | Ausgabe existiert oder kann nicht exklusiv veröffentlicht werden |

Die zuerst erreichte Prüfung bestimmt den Fehler. Ein verändertes Bundle über der Bytegrenze scheitert beispielsweise schon beim Lesen statt am Digestvergleich. Exportfehler ändern keine vorhandenen Eingabedateien. Betriebssystembeendigung kann andere Exitcodes liefern.

## Abgrenzung

Der Plan ist keine Signatur oder Einmalberechtigung. Auch ein veränderter Plan mit neu berechneter äußerer Prüfsumme muss bei Ausführung die vollständigen fachlichen Prüfungen bestehen. Derselbe gültige Plan kann mit unveränderten Eingaben zu mehreren neuen Ausgabedateien ausgeführt werden.

Der bisherige Snapshot wird einmal eingelesen und als unveränderliche Eingabe verwendet. Das ist keine Transaktion über einen gemeinsam beschreibbaren Dateipfad: spätere fremde Änderungen an dieser Eingabedatei werden nicht in den erzeugten separaten Snapshot übernommen. Es gibt keine globale Store-Aktivierung und keine automatische Weiterleitung oder Regelpromotion.

Das Paket umfasst ausschließlich Hinzufügen. Widerruf, Ersetzung, Z-Auswahl, allgemeine Auftragsketten, universelle Arbeitsbudgets und eine Phase-DAG sind keine Bestandteile dieser Übergabe. Direkte `experience add`-Aufrufe bleiben verfügbar. Ein Samplebefund wird durch die Übergabe nicht zu einem allgemeinen Beweis.

## Zusätzliche Ausführungssteuerung

`apply-add` akzeptiert seit Core 0.94 `--max-transfer-checkpoints N` und `--transfer-cancellation none|checkpoints`. Seit Core 0.102 gelten sie auch für direktes `experience add`; dabei steuern sie Quelle, optionale Basis, fachlichen Snapshotaufbau und exklusive Veröffentlichung gemeinsam. Dieselben Optionen gelten für die mutierenden Lebenszyklusaktionen `experience revoke` und `experience replace`: Widerruf steuert die optionale Basis, Ersetzung die optionale Basis und das Ersatzquellenbundle, danach jeweils die bestehende Experience-Operation und exklusive Veröffentlichung. Sie ändern keine Plangrenzen oder Planformate. Bei ihrer Angabe kommt die Ausgabehülle `controlled-transfer-v1` hinzu. Die oben beschriebene unmittelbare Ausgabe gilt ohne diese Optionen. [Details](transfer-control.md).

Seit Core 0.95 gelten dieselben [Steuerungsoptionen](transfer-control.md) auch für die Planung. Das Konto gilt pro Aufruf und wird nicht im Plan gespeichert.
