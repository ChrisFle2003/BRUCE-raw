# Blockweise GridResearch-Serialisierung

`pack_grid_research(run, digest_checkpoint, records_checkpoint)` unterstützt nun auch `records_checkpoint`. `pack_application` reicht diesen Callback für `GridResearchRun` weiter; `VerificationControl.serialization_grid_records` und das gemeinsame Sitzungsbudget beobachten damit Training, Holdout und das eingebettete Forschungsbundle.

Die Liefergrenze ist bewusst klein: Es gibt keine neue Wire-Version und keine Behauptung über maximale Signallatenz. Der Callback markiert Kopiergrenzen bis 64 KiB; das abschließende Hashing bleibt ein eigener Digest-Schritt.

Abnahme: `grid_serialization_control` prüft Bytegleichheit, Replay, Digest-Reihenfolge und Abbruch an jedem Forschungsblock. Die übrigen GridResearch- und Sitzungsbudgets bleiben regressionsgeprüft.
