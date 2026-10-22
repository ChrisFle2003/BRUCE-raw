# Scaled Exponential Grid Stage V1 — Design

**Status: IMPLEMENTIERT UND BELEGT** — Die begrenzte Stage-Erweiterung der
festen Regel `scaled-exponential-v1` ist implementiert; Task-1/2-Verträge,
12/12 abschließende Grid-/CLI-Prüfsuiten, 167 CLI-Verifiziererbeispiele und
repräsentative Bytevergleiche belegen den beschriebenen Umfang. Nachweis:
`runs/scaled-exponential-grid-stage-v1/reports/final-review.md`.
Die unabhängige Gesamtprüfung bleibt ein eigener Abnahmeschritt; Git-Metadaten
und die Prüfung über den Host-Dependency-Wrapper sind hier nicht verfügbar.

## Auftrag

Die feste Grid-Regel `scaled-exponential-v1` soll optional eine explizite,
begrenzte Stufe annehmen:

```text
h(stage) = 100 * (2^stage - 1)
```

Damit wird der für `uint64` sichere direkte Bereich `0..57` nutzbar, ohne die
lokalen Grid-Koordinaten oder ihre 8x8-Adressbedeutung zu verändern.

## Ziele

- `bruce grid` und `bruce grid-batch` akzeptieren für die skalierte Regel
  optional `--stage 0..57`.
- Ohne `--stage` bleibt Blatt-`y` die bisherige Stufe `0..7`.
- Mit `--stage` ist die Stufe unabhängig von Blatt-`y`; der Pfad bleibt eine
  Adresse.
- Einzel-, Batch- und Resume-Bundles binden die aufgelöste Stufe kanonisch an
  Cache, Ausgabe und Replay.
- Alte Bundles ohne neue Stage-Felder bleiben als bisherige `leaf-y`-Stufe
  replaybar.
- Der bisherige Aufruf ohne `--stage` bleibt bytekompatibel.

## Nichtziele

- Keine Erweiterung der lokalen Koordinaten über `0..7`.
- Keine freie Formel- oder Stage-Ausdruckssprache.
- Keine Änderung der Blattregel `leaf-local-power`.
- Keine pro-Abfrage-Stufen im Batch V1.
- Keine NanoCell-, Worker-, GPU-, Netzwerk- oder physikalische
  Blasenmodellierung.

## Öffentlicher Vertrag

```text
bruce grid --path X,Y --rule scaled-exponential-v1 [--stage 0..57]
bruce grid-batch --paths P1;P2;... --rule scaled-exponential-v1 [--stage 0..57]
```

| Regel / Option | Wirkung |
| --- | --- |
| `scaled-exponential-v1`, ohne `--stage` | `stage=leaf.y`, Quelle `leaf-y` |
| `scaled-exponential-v1 --stage N` | `stage=N`, Quelle `explicit` |
| `leaf-local-power`, ohne `--stage` | unverändertes `leaf.x^leaf.y` |
| `leaf-local-power --stage N` | Fehler `StageRequiresScaledExponentialRule` |
| `--stage` außerhalb `0..57` | Fehler `InvalidGridStage` |

Der Batch nutzt eine gemeinsame explizite Stufe für alle Pfade. `grid-resume`
erhält keine Stage-Option und übernimmt die kanonische Stage aus seinem
Eltern-Bundle.

## Datenmodell und Auswertung

`GridTask` und `GridSpec` führen eine optionale Stage. Für die skalierte Regel
wird vor der Auswertung eine kanonische Auflösung gebildet:

```text
stage_source = leaf-y | explicit
stage        = 0..57
```

Bei fehlender expliziter Stage wird die Blattkoordinate `y` verwendet. Bei
vorhandener Stage wird Blatt-`y` nicht zur Werteberechnung verwendet. Die
Adresse `(0,0)` mit `zero_zero=stop` bleibt eine regel- und stage-unabhängige
Sonderbehandlung und stoppt vor jeder Auswertung.

Exact begrenzt das Ergebnis weiterhin durch `max_bits`; Metric berechnet
innerhalb der bestehenden UInt64-Domäne unabhängig vom Exact-Anzeigelimit;
Modular reduziert den Regelwert modulo der vorhandenen nichtnull Modulus.

## Evidence, Cache, Replay und Versionen

Die implizite Route ohne `--stage` bleibt vollständig V1: `grid-spec-v1`,
Cache-Key, JSON, Trace und alle bestehenden Bundle-Header/Felder bleiben
bytegleich. Sie leitet die Stufe wie bisher aus Blatt-`y` ab und speichert
keine zusätzlichen Stage-Felder.

Nur eine explizite Stage verwendet die neue kanonische Spezifikation
`grid-spec-v2` mit genau den Feldern `stage_source explicit` und `stage N`.
Beide Felder sind Teil von Payload-Digest, Cache-Key, JSON-Ausgabe und Trace.
Die Stage-spezifischen Bundle-Header sind:

| Ablauf | Bisherige implizite Route | Explizite Stage |
| --- | --- | --- |
| Einzel-Grid | `BRUCE_GRID_BUNDLE_V1` | `BRUCE_GRID_BUNDLE_V2` |
| unbegrenzter Batch | `BRUCE_GRID_BATCH_BUNDLE_V1` | `BRUCE_GRID_BATCH_BUNDLE_V3` |
| begrenzter Batch, auch bei `COMPLETED` | `BRUCE_GRID_BATCH_BUNDLE_V2` | `BRUCE_GRID_BATCH_BUNDLE_V4` |
| Resume | `BRUCE_GRID_RESUME_BUNDLE_V1` | `BRUCE_GRID_RESUME_BUNDLE_V2` |

Ein V2-Grid-, V3/V4-Batch- oder V2-Resume-Bundle verlangt beide Stage-Felder
in kanonischer Reihenfolge. Fehlende, unbekannte oder widersprüchliche Felder
sind Fehler. Ein V1-Grid-, V1/V2-Batch- oder V1-Resume-Bundle wird nach dem
bisherigen Vertrag ohne Stage-Felder gelesen und nach der Rekonstruktion exakt
gegen seine bisherige Payload geprüft. Damit bleiben alte skalierte Bundles mit
`leaf-y`-Stufe replaybar, ohne ihre historischen Bytes umzudeuten.

Bei Batch bezeichnet die Headerwahl die konfigurierte Ausführungsgrenze,
nicht den Ergebnisstatus: Ein begrenzter Batch bleibt auch nach Bearbeitung
aller Pfade mit `COMPLETED` bei V2 beziehungsweise explizit bei V4.

Resume übernimmt die Stage ausschließlich über das replayte Eltern-Bundle.
Die Regel `leaf-local-power` erhält keine neuen Stage-Felder in ihrer
Standardausgabe oder ihren Standard-Bundles.

## Fehlerbehandlung

- `InvalidGridStage`: nichtnumerische oder außerhalb `0..57` liegende Stage.
- `StageRequiresScaledExponentialRule`: `--stage` mit einer anderen Regel.
- `InvalidBundleField: stage_source` oder `InvalidBundleField: stage`: fehlende,
  unbekannte oder nicht kanonische neue Stage-Felder in neuen skalierte
  Evidenzformaten.
- `ReplayMismatch`: Digest-korrekte, aber inhaltlich manipulierte Stage-,
  Ergebnis- oder Trace-Daten.

Bestehende Fehler für Modus, Bitlimit, Modulus, Pfad und Stop-Politik bleiben
unverändert.

## Test- und Abnahmekriterien

1. Stage `0`, `7` und `57` liefern unabhängig berechnete Referenzwerte in
   Exact, Metric und Modular, soweit der jeweilige Modus den Wert darstellen
   kann.
2. Exact lehnt eine explizite Stufe mit zu kleinem `max_bits` kontrolliert ab.
3. Stufe `58`, Stage mit Blattregel und ungültige Bundle-Stage-Daten werden
   kontrolliert abgewiesen.
4. Gleicher Pfad mit unterschiedlichen expliziten Stufen erzeugt getrennte
   Cache-Identitäten und Werte.
5. Einzel-, Batch- und Resume-Replay erhalten eine explizite Stage exakt;
   manipulierter, neu gehashter Inhalt wird abgewiesen.
6. Bestehende Aufrufe und Bundles ohne `--stage` bleiben bytegleich; alte
   skalierte V1-Bundles ohne Stage-Felder bleiben replaybar.
7. Explizite Einzel-, unbegrenzte/begrenzte Batch- und Resume-Ausführungen
   verwenden genau die V2-, V3-, V4- und V2-Header aus der Versionstabelle;
   der zentrale Replay-Dispatcher erkennt jede neue Variante.
8. CLI-Hilfe und Handbuch beschreiben die neue Option ausschließlich für die
   skalierte Regel und erklären die Grenzen klar.

## Betroffene Komponenten

| Bereich | Erwartete Verantwortung |
| --- | --- |
| `GridTask`, `GridSpec` | Stage aufnehmen, validieren und kanonisch auflösen |
| Grid-Auswertung | Aufgelöste Stage an die skalierte Formel übergeben |
| Cache/Batch/Resume | Stage in Spezifikation, Key, Bundle und Replay bewahren |
| zentraler Replay-Dispatcher | neue explizite Bundle-Header an die passenden Replayer leiten |
| CLI | `--stage` nur für Grid und Grid-Batch parsen |
| Tests/Dokumentation | Werte, Grenzen, Kompatibilität und Bedienung belegen |
