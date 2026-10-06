# ExactResearch-Kopiervertrag

Core 0.70 serialisiert `experience_snapshot` in `ExactResearch` über denselben maximal 64-KiB-Blockpfad wie die bestehenden Grid-Payloads. Vor jedem kopierten Block wird der optionale Records-Callback aufgerufen.

Die Feldnamen, Längen, Digestberechnung und Replaybytes bleiben unverändert. Bei einer Callback-Ausnahme wird vor dem Hashing abgebrochen. Der gemeinsame Sitzungsbudgetpfad zählt diese Kontrollstellen automatisch.
