# Core 0.46 – Konsolidierung der Forschungsverträge

## Auftrag und Abnahme

Freigegebener Schwerpunkt: gemeinsame Aufgaben-, Strategie- und Gate-Verträge sowie evidenzbasierter M0–M2-Abgleich. Dieses erste Paket vereinheitlicht die vorhandene exakte Formelforschung und deren Übergänge in Z und Experience. Grid-, Vergleichs- und Legacy-Aufträge behalten ihre fachlichen Typen; eine universelle Runtime wird daraus nicht abgeleitet.

Erfolg: CLI und Engine verwenden dieselbe Strategie-/Planerzulassung; sämtliche fachlichen ExactResearchTask-Prüfungen liegen an einer benannten Grenze; reale Auswahl-/Experienceentscheidungen verwenden gemeinsame Gatefunktionen. Ein aus denselben Daten erzeugter CLI-Vertragskatalog zeigt Fähigkeiten und Grenzen. Historische Ergebnisse, Budgets und Wireformate bleiben unverändert.

## Umsetzung

1. `research_contract.hpp/.cpp`: unveränderlicher Katalog der zehn vorhandenen Strategie-IDs mit Ausführungsfamilie und zulässigen Planern; gemeinsame Routenprüfung und bestehender Exact-Aufgabenvalidator. Domänen-/Parameter-/Sampleprüfung bleibt vollständig erhalten.
2. `research_gates.hpp/.cpp`: zentrale Regeln für Auswahl abgeschlossener Forschung und Aufnahme unabhängiger vollständiger Holdout-Portfolios mit bestätigter Hypothese. Regelpromotion und Veröffentlichung bleiben geschlossen. Diese Regeln erteilen keinen mathematischen Allgemeinbeweis.
3. CLI nutzt Katalogdefaults und gemeinsame Routenprüfung. Engine dispatcht über die Ausführungsfamilie. Store und Z nutzen die Gatefunktionen. `bruce contracts` macht den kompilierten Katalog sichtbar, ohne Aufgaben auszuführen.
4. Tests vor Implementierung: vollständige Strategie-/Planermatrix, Parameter-/Samplefehler, Gates mit Teilbefunden und Holdout, Integrationskatalog. Danach historische Baseline-Vergleiche, Debug/UBSan und Gesamtdemo.
5. ADR und M0–M2-Abgleich: konkrete vereinheitlichte Bereiche und weiterhin offene Phasen-/Gate-/Pluginverträge unterscheiden.

## Grenzen und Risiken

Eine eingebaute Strategieliste ist noch kein dynamisches Plugin-System. Die Gatefunktionen sind fachliche Entscheidungen an bestehenden Übergängen, keine allgemeine Zustandsmaschine. Bereits vorhandene Fehlerkategorien sollen erhalten bleiben; bei mehrfach ungültigen Eingaben kann die vereinheitlichte Prüfgrenze die zuerst sichtbare Fehlermeldung ändern. Exportdaten und erfolgreiche mathematische Abläufe dürfen sich nicht ändern. Keine pauschale M0–M2-Gesamtabnahme und keine neue Versionsverwaltung in diesem Paket.
