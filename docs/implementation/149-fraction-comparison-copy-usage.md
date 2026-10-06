# Blockweise FractionComparison-Serialisierung

`pack_fraction_comparison_records(run, digest_checkpoint, records_checkpoint)` serialisiert die Aufgaben- und Ergebnisdaten blockweise bis 64 KiB. `pack_application` sowie das gemeinsame Sitzungsbudget reichen den Callback für `FractionComparisonRun` weiter.

Es wird keine neue Bundleversion eingeführt. Der Pfad kontrolliert Kopiergrenzen und ist kein zusätzlicher mathematischer Beweis.
