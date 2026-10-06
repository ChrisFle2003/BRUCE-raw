# Abnahme Forschungsverträge / Core 0.46 – 22.09.2026

## Lieferung

Gemeinsame Strategie-/Planerprüfung und Standardplaner für CLI und Engine, zentraler Exact-Aufgabenvalidator, typisierte Dispatchfamilien und gemeinsame Gates für Forschungsübergänge in Z/Experience. `bruce contracts` zeigt den tatsächlich verwendeten Strategiekatalog. ADR-010 und M0–M2-Abgleich grenzen den erreichten Architekturstand ab.

## Prüfungen

```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure -j4
cmake --build build-ubsan -j4
ctest --test-dir build-ubsan --output-on-failure -j4
python3 scripts/demo.py
```

**82 von 82 Testgruppen bestanden**, sowohl Debug (64,72 s) als auch UndefinedBehaviorSanitizer (116,23 s; `-fsanitize=undefined -fno-sanitize-recover=all`). Keine Sanitizer-Meldung. Gesamtdemo erfolgreich. Alle 26 historischen Bundle-Fixtures liefern unveränderte Ergebnisse. Lokale Links in 154 Markdown-Dateien geprüft. 13 gezielte Vertrags-/Experience-/Grid-/Kompatibilitätstestgruppen bestehen ebenfalls.

Der neue CLI-Katalogtest scheiterte vor Implementierung am unbekannten Befehl. Der neue Unit-Test prüft sämtliche Kombinationen aus zehn Strategie-IDs und zehn bekannten Planern, gültige Standardaufträge aller Strategien, überlappende Samples, Nullbudget und unbekannte Routen. Gatefälle decken bekannte vollständige Status, Teilbefunde, unbekannte Status, Abbruch, fehlenden Holdout, fehlende Unabhängigkeit, unvollständiges Portfolio und fehlende bestätigte Hypothese ab. Allgemeinbeweis, Regelpromotion und Veröffentlichung bleiben geschlossen.

19 gültige Strategie-/Planerfälle wurden gegen Core 0.45 verglichen: identisches JSON, bytegleiche Exporte und identischer Replay alter Bundles. Darunter sind sämtliche erlaubten Routen einschließlich erfahrungsgeleiteter Portfolios.

## Unabhängiges Review

Keine konkreten Regressionen. Der Contract-Unit-Test bestand unabhängig. 43 zusätzliche gültige Fälle gegen Core 0.45 – alle erlaubten Routen bei zwei Budgets und fünf Expression-Äquivalenzen – lieferten identische Ausgaben, Exitcodes und Bundles. Dispatcher, Defaults, Katalog und tatsächliche Gateintegration wurden im Code geprüft. Nicht jede Kombination mehrfach ungültiger Parameter wurde verglichen.

## M0–M2-Abgleich

- **M0 teilweise konsolidiert:** gemeinsame Exact-Auftragsvalidierung und Routenregeln; universelle Verträge für andere Auftragstypen bleiben offen.
- **M1 teilweise konsolidiert:** gemeinsame Forschungsfreigaben für Z/Experience und erhaltene Reproduzierbarkeit; keine allgemeine Gate-Zustandsmaschine oder Releaseverwaltung.
- **M2 teilweise konsolidiert:** Katalog steuert reale eingebaute Strategien; kein dynamisches Plugin-System oder allgemeiner Strategie-Port.

Die bisherigen Zahlen-, Sample-, Fähigkeits- und Budgetprüfungen bleiben bestehen. Bei mehrfach ungültigen Eingaben kann sich die zuerst sichtbare Fehlermeldung ändern. Gemeinsame Facts sind interne geprüfte Eingaben der Übergänge; sie ersetzen keine Quellenprüfung. Keine pauschale M0–M2-Gesamtabnahme.

[Architekturentscheidung](../../adr/010-research-contract-and-gates.md) · [Benutzung und Abgleich](../97-research-contract-usage.md) · [Plan](../96-research-contract-consolidation-plan.md)
