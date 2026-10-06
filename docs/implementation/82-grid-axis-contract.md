# Beide Koordinatenrichtungen als Forschungsquelle – Core 0.39

## Begrenzter Ausbau

`research --grid-axis x|y` wählt die veränderliche letzte Grid-Koordinate. Standard bleibt `x`. Es gelten weiterhin die Quellen-, Spec-, Stopp-, Sample-, Budget- und Aussagegrenzen aus dem [Grid-Forschungsvertrag](80-grid-research-contract.md).

| Achse | Forschungseingabe | Fester Bestandteil | Zielwert | Projektion / Export |
| --- | --- | --- | --- | --- |
| `x` | letzte X-Koordinate | letzter Exponent Y | `X^Y` | `fixed-slice-leaf-x-v1`, Grid-Research-Bundle V1 |
| `y` | letzter Exponent Y | letzte Basis X | `X^Y` | `fixed-slice-leaf-y-v1`, Grid-Research-Bundle V2 |

Der normalisierte Vorfahrenpfad und die feste Koordinate müssen über Training und Holdout gleich bleiben. Doppelte oder überlappende Werte der **ausgewählten** Koordinate werden abgewiesen. Insgesamt sind höchstens acht unterschiedliche Eingaben möglich. Die Grid-Wertregel wird nicht vertauscht: Bei Y-Projektion von `2,3` ist die Eingabe 3 und der Zielwert 8, nicht 9.

Die vorhandene Forschung verwendet intern und in Formeln weiterhin den Variablennamen `x`. Bei `--grid-axis y` bezeichnet diese Forschungsvariable den letzten Grid-Exponenten Y. Die Projektionsversion macht die Zuordnung explizit; die Z-Auswahl `--at N` setzt ebenfalls diese ausgewählte Koordinate auf N.

## API, Versionen und Replay

`research_from_grid` besitzt einen zusätzlichen letzten Parameter `axis`, standardmäßig `"x"`. Bisherige Aufrufe bleiben gültig. `GridResearchRun` enthält zusätzlich `axis` und `base`; das bisherige `exponent`-Feld gehört zur X-Projektion.

- X verwendet unverändert Engine `bruce-grid-research-v1`, Bundle `BRUCE_GRID_RESEARCH_BUNDLE_V1` und Herkunftsfeld `exponent`. Der Standard und ausdrücklich `--grid-axis x` erzeugen dieselben Bytes.
- Y verwendet Engine `bruce-grid-research-v2`, Bundle `BRUCE_GRID_RESEARCH_BUNDLE_V2` und Herkunftsfeld `base`. Ein festes `exponent`-Herkunftsfeld wird hier nicht ausgegeben.
- Header, Engine und Projektionsversion müssen zusammenpassen. V2 akzeptiert ausschließlich Y; V1 ausschließlich X. Unbekannte Achsen oder Kombinationen sind Fehler.
- Die CLI-Option ist nur zusammen mit `--grid-train` erlaubt. CSV-Eingaben werden nicht umgedeutet.

Beide Varianten replayen ihre vollständigen Grid-Quellen, leiten die geordneten Samples gemäß ihrer Projektionsversion ab und vergleichen diese exakt mit den Samples der inneren Forschung. Anschließend wird die vollständige äußere Payload verglichen. Vertauschte Achsen, falsche feste Koordinaten oder gültige, aber nicht passende innere Forschungen werden nicht akzeptiert.

Forschung und Z-Auswahl bleiben ohne ursprüngliche Dateien replaybar. Der Auswahlbereich bleibt 0–7. Budgetabbrüche und Callback-Abbrüche der Forschung behalten ihre bisherigen Teilbefunde und Abbruchpositionen. Die neue Achse verändert keine Forschungsstrategie oder deren Kostenmodell.

## Zahlen- und Aussagegrenzen

Nur exakte, erfolgreich ausgewertete Grid-Blätter sind zulässig. Modulare Werte, Metriken, Stopps, gemischte Specs und unpassende Ausschnitte werden weiterhin abgewiesen. `0^0=1` gilt bei entsprechender Grid-Politik auch für die Y-Projektion der Basis null; eine Stopppolitik macht dieses Blatt nicht zu einem Sample.

Eine Exponentenprojektion fügt keine allgemeine Exponentialformelsuche hinzu. Beispielsweise liefern die ersten drei Werte von `2^Y` ein passendes quadratisches Interpolationspolynom, das an weiteren Grid-Werten scheitert. Dieser Kandidat bleibt widerlegt. Eine vollständig geprüfte begrenzte Suchfamilie ohne überlebende Hypothese beweist nicht, dass überhaupt keine Formel existiert.

Offen bleiben zweidimensionale Projektionen, variable Vorfahrenpfade, gemeinsame Phasenbudgets, allgemeine Expansion, Experience-Import der Grid-Forschungswrapper und universelle Aufgaben-/Strategie-/Gate-Verträge. Kein vollständiger M2-Abschluss.

[Benutzung](83-grid-axis-usage.md) · [Abnahme](acceptance/grid-axis-v1-2026-09-22.md)
