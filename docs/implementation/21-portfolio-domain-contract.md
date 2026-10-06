# Core 0.9: Vertrag des Portfolios mit Definitionsbereichen

Die freigegebene Fortsetzung integriert die bereits implementierte Bruchfunktionsfamilie in den bestehenden Portfolioablauf. `portfolio-rational` untersucht in fester Reihenfolge die adaptive Potenzfamilie, Polynome und `(a*x+b)/(x+c)`. Zahlendomäne rational, Planer `portfolio`; Grenzen für Exponenten, Grade, Koeffizientenzähler und -nenner entsprechen den Einzelstrategien.

## Identität und Zusammenführung

Polynome werden wie bisher über gekürzte rationale Koeffizienten und entfernte führende Nullkoeffizienten identifiziert. Bruchfunktionen erhalten eine eigene Identität aus `[b,a]` und dem monischen Nenner `[c,1]`. Der ausgeschlossene Wert `-c` bleibt erhalten. Kein Kürzen von Definitionslücken und keine Zusammenführung mit total definierten Polynomen. Für diese feste Bruchfamilie ist das Koeffiziententripel bei festem ausgeschlossenen Punkt eindeutig; höhergradige rationale Normalformen sind nicht Teil dieses Vertrags.

Gesamt-ID `source.id*3+strategy_index`, mit Indizes 0/1/2. Die rationalen Slots ergeben `source.id < 2^48`, also Gesamt-ID `< 3*2^48`. Die Z-Koordinaten werden bei `2^25` geteilt; beide Koordinaten sind kleiner als `2^25`, ihre Cantor-Adresse bleibt in uint64. V1 behält seine bisherigen IDs und Koordinaten.

## Gemeinsame Ressourcen und Status

Das vorhandene Portfolio übergibt ausschließlich Restbudgets. Kandidaten, Rechenoperationen, gespeicherte Quellhypothesen und sämtliche inneren/äußeren Traceereignisse zählen gemeinsam. Zusammengeführte Quellen zählen weiterhin gegen das Ergebnisbudget. Bei unvollständigem Teilrun bleibt auch der Gesamtstatus unvollständig, selbst wenn eine andere Strategie erfolgreich ist. Quellen und ihre Status bleiben sichtbar.

Abbruchcheckpoints gelten global über alle drei Strategien, einschließlich Übergängen. Export speichert den erreichten globalen Checkpoint; Replay reproduziert dieselbe Abbruchposition. Unterschiedliche fachliche Status derselben zusammengeführten Identität bleiben ein Konflikt. Mehrfachverwendung derselben Daten ist keine zusätzliche unabhängige Evidenz.

## Versionen und Projektion

Neuer Container `BRUCE_PORTFOLIO_BUNDLE_V2`, Engine `bruce-portfolio-v2`, Regel `domain-union-v1`, Policy `shared-budget-v1`, Planer `power-polynomial-rational-v1`. `portfolio` und V1 bleiben unverändert; historische Fixtures schützen die bisherigen Ergebnisse. Auch der vor dem Ausbau gesicherte Core-0.8-Bruchfunktionsexport bleibt replaybar.

Z wertet anhand der vollständigen Hypothese entweder das Polynom oder die Bruchfunktion aus. Eine Definitionslücke erzeugt einen expliziten Fehler für den gesamten Import; Kandidaten werden nicht still entfernt. Bruchwerte und Werte außerhalb int64 bleiben in dieser bestehenden Projektion unzulässig. Kontextkennung `portfolio-v2-rational-v1`.
