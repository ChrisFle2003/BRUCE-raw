# Grid-Aufträge speichern und wiederholen – Core 0.36

Die vorhandene Grid-Abfrage kann ihren vollständigen Auftrag jetzt exportieren. Replay verwendet die gespeicherten Parameter einschließlich der `0^0`-Regel und benötigt keine aktuellen CLI-Voreinstellungen.

## Exakt rechnen

```bash
cmake --build build
./build/bruce grid --path 2,6/4,3 --output grid-exakt.bruce
./build/bruce replay grid-exakt.bruce
```

Ergebnis: Wert `64`, eine Auswertung, Tiefe 2. Entscheidend ist das letzte Paar `4,3`, also `4³`. Der Pfad `2,6` liefert ebenfalls 64, besitzt aber eine andere Adresse. Die bisherigen JSON-Ausgaben bleiben unverändert; der Export enthält zusätzlich Auftrag und vier feste Prüfschritte.

## Modulus und Größenangabe

```bash
./build/bruce grid --path 2,6/4,3 --mode modular --modulus 7 --output grid-modular.bruce
./build/bruce replay grid-modular.bruce
./build/bruce grid --path 2,6/4,3 --mode metric --output grid-metrik.bruce
./build/bruce replay grid-metrik.bruce
```

Modulo 7 ist der Wert 1. Der Metrikmodus liefert die exakte Bitlänge 7 des lokalen Werts 64. Dies ist keine Schätzung für beliebig große Grid-Werte.

`--max-bits` liegt in allen Modi zwischen 1 und 64. Nur im exakten Modus begrenzt es den Potenzwert; beispielsweise scheitert `2^6` mit sechs Bits und gelingt mit sieben. Im modularen und metrischen Modus gelten die bisherigen jeweiligen Rechenregeln.

## Stopp speichern

```bash
./build/bruce grid --path 2,6/0,0 --zero-zero stop --output grid-stopp.bruce
./build/bruce replay grid-stopp.bruce
```

Der Lauf meldet `STOPPED`, `BRANCH` und null Auswertungen. Mit dem Standard `--zero-zero one` liefert dieses Blatt stattdessen 1. Nur das letzte Koordinatenpaar entscheidet; ein früheres `(0,0)` stoppt die spätere Blattauswertung nicht.

## Grenzen

Bis zu 64 Pfadschritte mit jeweils Koordinaten 0–7 sind zulässig. Auch bei Tiefe 12 wird nur das angefragte Blatt ausgewertet, nicht ein Raum von `64^12` Einträgen aufgebaut. Ungültige Eingaben und Bitgrenzenfehler erzeugen keinen Export. Vorhandene Ausgabedateien werden nicht überschrieben.

Neue Dateien heißen intern `BRUCE_GRID_BUNDLE_V1`. Geänderte Metadaten, widersprüchliche Prüfschritte und falsche Ergebnisse werden bei Replay abgewiesen. Die Datei beweist keine externe Urheberschaft.

## Weitere Roadmap

Der Grid-Einzelaufruf ist jetzt Teil der Anwendungs- und Replayarchitektur. Als Nächstes fehlen die vollständige GridSpec-/Cacheidentität und die Einbindung in die Forschungsorchestrierung. BigInt im Grid/Z, allgemeine Expansion und Aststeuerung bleiben weitere Arbeit; der vollständige M2-Meilenstein ist damit noch nicht abgeschlossen.

[Vertrag](76-grid-replay-contract.md) · [Abnahme](acceptance/grid-replay-v1-2026-09-22.md)
