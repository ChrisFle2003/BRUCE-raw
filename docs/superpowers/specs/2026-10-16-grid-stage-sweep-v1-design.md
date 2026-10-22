# Grid Stage Sweep V1 — Design

**Status: IMPLEMENTIERT UND BELEGT** — Der unten begrenzte Sweep-V1-Vertrag
ist durch native Build-, Sweep-/Grid-/CLI-/Anwendungsprüfungen und kanonisches
Replay belegt (16.10.2026). Belege: [Sweep-Verträge](../../../tests/unit/grid_stage_sweep_tests.cpp),
[CLI-Workflows](../../../tests/integration/test_cli.py) und
[Handbuch](../../cli/commands.md#grid-stage-sweep--begrenzte-stufenreihe-mit-diagnose).
Der lokale Abschlussnachweis liegt unter
`runs/grid-stage-sweep-v1/reports/final-review.md`; die unabhängige
Gesamtprüfung steht bei dieser Statusaktualisierung noch aus. Die Gesamtsuite
bleibt **IMPLEMENTIERT, ABER NICHT AKTUELL VERIFIZIERT**, insbesondere wegen
der separat protokollierten Sandbox-/Cockpit-Socketgrenze. Dieser Status
erweitert keine der unten genannten Grenzen oder Nichtziele.

## Auftrag

BRUCE erhält einen eigenständigen Sweep für die bereits implementierte Regel
`scaled-exponential-v1`. Ein Lauf wertet eine zusammenhängende explizite
Stufenreihe innerhalb `0..57` aus und liefert genug kanonische Information,
um jede Stufe, jeden Cache-Zugriff und einen möglichen Abbruch nachzuvollziehen.

## Öffentlicher Vertrag

```text
bruce grid-stage-sweep --path X,Y --from-stage A --to-stage B
                       [--mode exact|metric|modular]
                       [--max-bits 1..64] [--modulus M]
                       [--debug] [--output RUN]
```

Der Befehl verwendet fest `scaled-exponential-v1`. `--path` bleibt eine lokale
Grid-Adresse; seine Blattkoordinaten bestimmen die Sweep-Stufe nicht.

`A` und `B` müssen in `0..57` liegen und `A <= B` erfüllen. Die Länge beträgt
höchstens 58 Einträge. Es gibt keine freie Formel, keine freie Regelwahl und
keine Erweiterung der lokalen Koordinaten.

## Ergebnis und Debug-Daten

Jeder Stage-Eintrag enthält mindestens:

```text
index, stage, stage_source=explicit,
result oder bit_length,
cache_key_digest, cache_event, status
```

Mit `--debug` enthält jeder Eintrag zusätzlich die vollständige Grid-Trace.
Ohne `--debug` bleiben Trace-Daten aus der JSON-Ausgabe und Evidence-Payload
ausgeschlossen; Werte, Cache-Digests, Ereignisse und Status bleiben sichtbar.

Der Gesamtreport enthält den aufgelösten gemeinsamen Grid-Spec, Start- und
Endstage, verlangte und verarbeitete Einträge, Cache-Statistiken, Status und
Abbruchgrund. Die Reihenfolge ist strikt aufsteigend nach Stage.

## Modi und Abbruch

- Exact nutzt den bestehenden `max_bits`-Vertrag. Überschreitet eine Stage das
  Limit, bleibt der Lauf `INCOMPLETE`, behält alle vorherigen Diagnosen und
  endet mit `IntegerBitLimit` an der ersten nicht darstellbaren Stage.
- Metric nutzt die bestehende UInt64-Auswertung und liefert exakte Bitlängen.
- Modular benötigt einen bestehenden nichtnull Modulus und liefert Reste.
- `(0,0)` ist im Sweep keine Stop-Sonderbehandlung: Der Sweep verwendet
  explizite Stages und setzt die interne Grid-Task-Politik fest auf `one`, damit
  jede angeforderte Stage diagnostizierbar ausgewertet wird.

Ungültige Grenzen ergeben `InvalidStageRange`; ungültige Modus-, Bitlimit- und
Moduluswerte nutzen die bestehenden kontrollierten Fehler.

## Cache, Evidence und Replay

Ein Sweep besitzt einen pro Lauf frischen Grid-Cache mit bestehendem
Bytebudget. Jede Stage wird als explizite GridTask evaluiert; deshalb sind
Cache-Key-Digest und Cache-Ereignis direkt mit der Stage gebunden. Eine
Wiederholung einer Stage ist in V1 nicht möglich, sodass normale Sweeps
erwartbar nur `MISS_*`-Ereignisse erzeugen; das Ereignis wird trotzdem für
vollständige Diagnose und zukünftige Vergleichbarkeit gespeichert.

Der neue Header lautet `BRUCE_GRID_STAGE_SWEEP_BUNDLE_V1`. Seine Payload bindet
den vollständigen gemeinsamen Spec, die Grenzen, Debug-Flag, kanonische
Einträge, Cache-Statistiken, Trace und den Endstatus. `replay` erkennt den
Header, rekonstruiert den Sweep ohne ursprüngliche Eingabedatei und vergleicht
die vollständige Payload. Ein neu gehashter, aber veränderter Stage-, Ergebnis-,
Cache-, Trace- oder Statistikwert führt zu `ReplayMismatch`.

Der Sweep verändert weder Grid V1/V2, Batch V1–V4 noch Resume V1/V2.

## Nichtziele

- Keine Stage-Listen, Wiederholungen oder pro-Abfrage-Konfigurationen.
- Kein Resume für Sweep V1.
- Keine gemeinsame Persistenz zwischen Sweep-Läufen.
- Keine freie Formel- oder Regel-Ausführung.
- Keine physikalische Blasenmodellierung, NanoCell-, Worker-, GPU- oder
  Netzwerkausführung.

## Abnahmekriterien

1. Exact Sweep `0..7` liefert die bekannten acht Werte und aufsteigende Stages.
2. Exact Sweep `0..57` mit 63 Bits endet kontrolliert erstmals bei 57 und
   bewahrt die Einträge 0..56.
3. Metric und Modular über `0..57` liefern referenzierbare Einträge; Modular
   mit Modulus 128 deckt den Nullrest von `2^stage` ab.
4. Ungültige Grenzen, Modulus und Budgetfehler werden kontrolliert abgewiesen.
5. `--debug` verändert nur Trace-Sichtbarkeit/Evidence, nicht Werte, Reihenfolge
   oder Cache-Identitäten.
6. Bundle-Replay reproduziert vollständige und abgebrochene Sweeps; rehashed
   Manipulationen werden abgewiesen.
7. Bestehende Grid-, Batch- und Resume-Ausgaben bleiben bytegleich.
8. CLI-Hilfe, Handbuch und zentrale Replay-Dispatch-Dokumentation beschreiben
   nur den implementierten V1-Umfang.
