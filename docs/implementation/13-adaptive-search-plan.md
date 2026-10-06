# Adaptive Koeffizientensuche – Core 0.5

Fortsetzung der freigegebenen Roadmap im vorhandenen Forschungspfad. Ziel: weniger ausgeführte Kandidaten bei gleicher expliziter Familie `a*x^p+b`; keine gespeicherten Antworten, kein statistischer Wahrheitsentscheid.

## Vertrag

Neue optionale CLI-Einstellung `--planner adaptive` nur für `--strategy power-affine`. Standard bleibt `enumerate`. Für jeden Exponenten berechnet der Planer ausschließlich aus Lernpaaren u=x^p. Zwei verschiedene u bestimmen `a=(y1-y0)/(u1-u0)` und `b=y0-a*u0` exakt. Erzeugt wird höchstens dieses Koeffizientenpaar, wenn seine gekürzten Komponenten im deklarierten Suchraum liegen. Gleiche u mit verschiedenen y schließen die gesamte Exponentenfamilie aus. Fehlen zwei verschiedene u, wird die bestehende Suche verwendet. Konstanten mit a=0 bleiben nur bei p=1.

Planungsarithmetik verwendet rationale GMP-Werte mit derselben Bitgrenze, auch wenn Zielkoeffizienten ganzzahlig sein müssen. Jede Transformation reserviert 2*p logische Operationen, Vergleichsschritte und Ableitungsoperationen werden ebenfalls budgetiert. Ein Zahlen-/Arbeits-/Trace-Limit stoppt als INCOMPLETE. Kandidaten werden weiterhin an allen Lernpaaren, über getrennte Gegenrechnung und anschließend an Hold-out geprüft. Abgeleitete Koeffizienten sind keine allgemeine Formelbewahrheitung.

Entscheidungen (`derived`, `outside_family`, `inconsistent_projection`, `fallback`) werden mit Exponent und verwendeten Sampleindizes in begrenztem Trace festgehalten. Neue Versionen: `BRUCE_ADAPTIVE_BUNDLE_V1`, Engine `bruce-adaptive-v1`, Regel `power-affine-v1`, Policy `adaptive-samples-v1`, Planer `two-point-v1`. Historische Exportformate und Ausgaben bleiben unverändert. Abbruchposition verwendet bei adaptiven Läufen einen eindeutigen Checkpointzähler für Planungs-/Kandidatengrenzen, statt nur die Kandidatenzahl; Original-API-Abbruchschwellen bleiben lesbar.

## Umsetzung und Prüfung

- [x] Core-0.4-Export vor Änderung sichern; CLI-Abnahmen gegen fehlende Planeroption rot beobachten.
- [x] Begrenzten Planer in eigener Anwendungseinheit implementieren; bestehende Auswertung weiterverwenden und Rohslot-IDs erhalten.
- [x] CLI, versionierten Replay und Z-Ursprungsimport integrieren.
- [x] Adaptive/enumerative Ergebnisse und IDs gegen unabhängige Fraction-Referenz vergleichen; singuläre Paare, Gegenbeispiele, rationale/integrale Familien, Limits und Abbruch prüfen.
- [x] Demo mit gemessenen Kandidaten-/Operationszahlen, Anleitung und Roadmap aktualisieren; unabhängiges Review und Debug-/UBSan-Prüfung abschließen.

Keine Behauptung allgemeiner Formelsynthese, gelernten Strategierankings oder genereller Geschwindigkeitsverbesserung: dokumentiert werden konkrete Arbeitszähler der Beispiele.
