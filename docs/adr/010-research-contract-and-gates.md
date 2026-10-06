# ADR-010: Gemeinsamer Vertrag der eingebauten Formelforschung

## Status

Akzeptiert und implementiert mit Core 0.46 innerhalb des freigegebenen Architektur-Konsolidierungsschwerpunkts.

## Kontext

CLI und Exact-Engine prüften Strategie-/Planerbeziehungen getrennt. Die Engine hielt den gesamten Auftragsvalidator neben dem eigentlichen Suchalgorithmus. Experience und Z prüften ihre Forschungsfreigaben jeweils lokal. Eine Erweiterung konnte dadurch nur auf einer Seite berücksichtigt werden.

## Entscheidung

`ResearchStrategy` beschreibt jede der zehn eingebauten Strategie-IDs mit Standardplaner, erlaubten Planern und typisierter Ausführungsfamilie. CLI und Engine verwenden denselben Katalog und dieselbe Routenprüfung. Die Engine wählt ihren Executor über die Ausführungsfamilie. Die eigentlichen Algorithmen bleiben unverändert.

`validate_exact_task` bildet die gemeinsame Vertragsgrenze für `ExactResearchTask`: Routen, Domänen, numerische Parameter, Samples, Dubletten, Ressourcen und Fähigkeitskombinationen. CLI-, Grid- und Replay-Aufträge gelangen über die Exact-Engine durch diese Grenze. Legacy-Int64-, Grid-, Vergleichs- und Z-Aufträge behalten ihre eigenen fachlichen Typen und Validierung.

`ResearchGateFacts` und `ResearchGateDecision` vereinheitlichen die Entscheidungen an Forschungsübergängen. Auswahl setzt einen bekannten abgeschlossenen Forschungsstatus und Stopgrund `Completed` voraus. Experience setzt zusätzlich unabhängige Herkunft, vollständiges zulässiges Portfolio, Holdout und eine auf Samples bestätigte Hypothese voraus. Herkunftsprüfung, Quellen-Replay, Versionszulassung und Deduplizierung bleiben vorgeschaltete Aufgaben der jeweiligen Adapter. Die Facts sind keine externe Evidenz und kein Ersatz für diese Prüfungen.

Regelpromotion, Veröffentlichung und allgemeiner Beweis bleiben geschlossen. Diese Gates betreffen die Forschungsentscheidung, nicht das Speichern einer lokalen Laufdatei. `bruce contracts` zeigt den eingebauten Katalog und seine Grenzen als JSON.

## Kompatibilität und Grenzen

Bestehende mathematische Ausführung, Operationszählung, JSON-Ergebnisse und Bundles bleiben unverändert. Bei mehrfach ungültigen Eingaben kann die zentrale Routenprüfung eine andere Fehlerpriorität liefern; bekannte Fehlerkategorien bleiben erhalten. Katalog und Gatefunktionen sind keine allgemeine Plugin-Schnittstelle, kein beliebiger Task-Container und keine Phasen-/Ast-Zustandsmaschine.

Neue Strategien erfordern weiterhin einen Executor, spezifische Parameter-/Replayregeln und Tests. Die Registrierung allein implementiert keinen neuen Algorithmus. Eine spätere Erweiterung auf weitere Auftragsarten benötigt eigene Abnahmen.

[Plan und Abnahmeumfang](../implementation/96-research-contract-consolidation-plan.md) · [Benutzung](../implementation/97-research-contract-usage.md)
