# Basis oder Exponent untersuchen – Core 0.39

Bruce kann jetzt beide letzten Grid-Koordinaten als Forschungseingabe verwenden:

- `--grid-axis x`: Basis verändern, Exponent festhalten. Das ist der bisherige Standard.
- `--grid-axis y`: Exponent verändern, Basis festhalten.

## Den Exponenten verändern

```bash
./build/bruce grid-batch --paths '2,0;2,1;2,2' --output exponent-lernen.bruce
./build/bruce grid-batch --paths '2,3;2,4' --output exponent-pruefen.bruce
./build/bruce research --strategy polynomial --domain rational --max-degree 2 \
  --grid-axis y --grid-train exponent-lernen.bruce \
  --grid-holdout exponent-pruefen.bruce --output exponent-forschung.bruce
./build/bruce replay exponent-forschung.bruce
```

Die Lernpaare sind `(0,1)`, `(1,2)` und `(2,4)`. Das quadratische Polynom `1 + x/2 + x²/2` trifft diese drei Werte. An Eingabe 3 liefert es aber 7, während das Grid 8 liefert. Bruce meldet daher `REFUTED`. Die Variablenbezeichnung `x` im Forschungsbefund steht hier für den ausgewählten Grid-Exponenten Y.

Das ist ein korrekter negativer Befund für diesen Kandidaten. `2^Y` ist weiterhin eine bekannte Regel; die gewählte Polynomstrategie hat keine allgemeine Exponentialfunktion entdeckt.

## Positiven Ablauf und Auswahl prüfen

```bash
./build/bruce grid-batch --paths '1,0;1,1;1,2' --output eins-lernen.bruce
./build/bruce grid-batch --paths '1,3;1,4' --output eins-pruefen.bruce
./build/bruce research --strategy polynomial --domain rational --max-degree 2 \
  --grid-axis y --grid-train eins-lernen.bruce --grid-holdout eins-pruefen.bruce \
  --output eins-forschung.bruce
./build/bruce compatibility --rules examples/quadratic-fraction-selection.rules \
  --from-run eins-forschung.bruce --at 7 --output eins-auswahl.bruce
./build/bruce replay eins-auswahl.bruce
```

Hier findet Bruce die konstante Formel 1 und bestätigt sie an den zusätzlichen Werten. `--at 7` bedeutet in dieser Projektion Exponent 7. Auswahl und Forschung behalten die vollständigen Quellexporte; Replay benötigt deren ursprüngliche Dateien nicht mehr.

## Grenzen

Präfix, feste Basis und vollständige GridSpec müssen gleich bleiben. Der ausgewählte Exponent darf innerhalb und zwischen Lern-/Prüfquelle nicht doppelt vorkommen. Stopps und ungeeignete Rechenmodi sind keine Samples. Eingaben und Auswahl bleiben auf 0–7 begrenzt.

Die neue Y-Richtung verwendet einen eigenen V2-Forschungsexport. Die X-Richtung bleibt bei V1; alte Exporte funktionieren unverändert. Die üblichen Forschungsbudgets und `INCOMPLETE`-Befunde gelten weiterhin. Gemeinsame Phasenbudgets, allgemeine Expansion und M2-Gesamtabschluss bleiben offen.

[Vertrag](82-grid-axis-contract.md) · [Abnahme](acceptance/grid-axis-v1-2026-09-22.md)
