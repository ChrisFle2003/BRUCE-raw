# Blockweise ExactResearch-Serialisierung

`pack_exact_research_records(run, digest_checkpoint, records_checkpoint)` ergänzt den bisherigen `pack_exact_research`-Pfad. `pack_application` nutzt ihn für erfahrungsbasierte `ExactResearchRun`-Bundles, sobald ein Records-Callback oder ein gemeinsames Sitzungsbudget aktiv ist.

Damit werden große eingebettete Erfahrungsdaten unterbrechbar kopiert, ohne eine neue Bundleversion einzuführen. Das ist eine Kopier- und Prüfpunktgrenze, kein allgemeines Lernverfahren und keine Zusage maximaler Signallatenz.

Auch `GridResearch` reicht den Pfad in sein verschachteltes `research_bytes`-Bundle weiter. Dadurch bleiben Erfahrungsdaten innerhalb eines Grid-Forschungslaufs beobachtbar.
