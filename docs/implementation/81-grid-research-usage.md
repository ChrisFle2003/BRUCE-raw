# Vom Grid zur Formelforschung – Core 0.38

Bruce kann jetzt Werte aus einem festen Grid-Ausschnitt an seine vorhandene Formelforschung übergeben. Die Quelldaten bleiben vollständig im Forschungsexport enthalten.

## Grid-Werte erzeugen und untersuchen

```bash
./build/bruce grid-batch --paths '0,2;1,2;2,2' --output grid-lernen.bruce
./build/bruce grid-batch --paths '3,2;4,2' --output grid-pruefen.bruce
./build/bruce research --strategy polynomial --domain rational --max-degree 2 \
  --grid-train grid-lernen.bruce --grid-holdout grid-pruefen.bruce \
  --output grid-forschung.bruce
./build/bruce replay grid-forschung.bruce
```

Die Lernpaare sind `(0,0)`, `(1,1)` und `(2,4)`. Die zusätzlichen Prüfpaare sind `(3,9)` und `(4,16)`. Die Polynomstrategie findet `x²` mit Koeffizienten `0,0,1`; der Status lautet `VERIFIED_ON_SAMPLES`.

Die Grid-Regel `x^y` war bereits bekannt. Das Beispiel zeigt die durchgängige Verbindung von Quelle, Suche, Prüfung und Replay; es behauptet keinen neuen allgemeinen Beweis.

## Andere bestehende Strategien verwenden

```bash
./build/bruce research --strategy power-affine --planner adaptive --max-power 2 \
  --min 0 --max 1 --grid-train grid-lernen.bruce --grid-holdout grid-pruefen.bruce
```

Die üblichen Strategie- und Budgetoptionen bleiben verfügbar. Das äußere Ergebnis enthält die Grid-Herkunft; unter `research` steht der bisherige Forschungsbefund. Cacheeinstellungen der Quellexporte ändern die Lernwerte nicht.

## Auswahl mit erhaltener Herkunft

```bash
./build/bruce compatibility --rules examples/quadratic-fraction-selection.rules \
  --from-run grid-forschung.bruce --at 5 --output grid-auswahl.bruce
./build/bruce replay grid-auswahl.bruce
```

Die gefundene Hypothese liefert an Eingabe 5 den Wert 25. Für Grid-Forschung ist `--at` auf 0–7 begrenzt. Der Wert ist eine Auswertung der Hypothese mit erhaltenem Forschungsstatus, keine automatisch neu geprüfte Grid-Abfrage.

Forschungs- und Auswahl-Replay funktionieren auch ohne die ursprünglichen Quelldateien. Exportdateien werden niemals überschrieben.

## Welche Eingaben passen zusammen?

Die letzte X-Koordinate wird zur Forschungseingabe, der exakte Grid-Wert zur Ausgabe. Präfix, Exponent und GridSpec müssen über alle Lern- und Prüfwerte übereinstimmen. Beispielsweise passen `1,1/2,3` und `1,1/4,3` zusammen. `1,1/2,3` und `1,1/4,2` haben unterschiedliche Exponenten und werden abgewiesen.

Keine doppelten oder überlappenden X-Eingaben, keine modularen Werte, Metriken oder gestoppten Blätter. Eine ausdrücklich angegebene leere Prüfdatei ist ein Fehler. Ohne `--grid-holdout` bleibt ein Auftrag ohne zusätzliche Prüfquelle möglich; Bruce erfindet keine Prüfwerte.

Pro Quelle gelten höchstens 2 MiB und die bisherigen Grid-Grenzen. Forschung behält ihre eigenen Budgets und replaybaren Teilbefunde. CSV- und Grid-Dateien dürfen nicht im selben Auftrag gemischt werden.

## Weitere Roadmap

Die erste feste Grid-Scheibe ist jetzt mit exakter Forschung, Replay und Z-Auswahl verbunden. Andere Projektionen, allgemeine Expansion, gemeinsames Arbeitsbudget über alle Phasen, Experience-Import dieser Herkunft und der gemeinsame Aufgaben-/Strategievertrag bleiben offen. Kein vollständiger M2-Abschluss.

[Vertrag](80-grid-research-contract.md) · [Abnahme](acceptance/grid-research-v1-2026-09-22.md)
