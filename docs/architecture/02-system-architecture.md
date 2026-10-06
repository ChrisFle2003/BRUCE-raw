# Gesamtarchitektur und Modulgrenzen

## Zielarchitektur

```text
Benutzer / Datei / CLI
        |
        v
InputNormalizer + Tokenizer
        |
        v
TaskSpec + Domain + VersionBundle
        |
        v
SingleTriangleKernel: (x, y) <-> z
        |
        v
ZCompatibilityEngine
        |
        v
StrategyExecutor
  | exact math | grid/path | byte/nanocell | hypothesis |
        |
        v
Countercheck + Hold-out + Replay
        |
        v
Evidence / Experience Store
        |
        v
Explanation + Research Cockpit
```

## Verantwortlichkeiten

| Modul | Besitzt | Darf abhaengen von | Darf nicht abhaengen von |
| --- | --- | --- | --- |
| `core` | IDs, Versionen, Budgets, Status | Standardbibliothek | andere BRUCE-Module |
| `coordinate` | reversible Adressen | `core` | Token, UI, Store |
| `compatibility` | Regeln, Filter, Score, Auswahl | `core`, `coordinate` | UI, Gate-Entscheidung |
| `token` | Tabellen, Tokens, Relationen | `core`, `compatibility` | PublishGate |
| `math` | Strategien und Gegenchecks | `core`, `coordinate`, `evidence` | Cockpit |
| `evidence` | Trace, Replay, Experiences | `core` | Strategiewahl |
| `application` | Run-Orchestrierung und Gates | alle Kernmodule | UI-Details |
| `cli` | Ein-/Ausgabe | `application` | direkte Storemutation |

## Vorgeschlagener Quellbaum

```text
include/bruce/{core,coordinate,compatibility,token,evidence,math,application,cli}/
src/{core,coordinate,compatibility,token,evidence,math,application,cli}/
tests/{unit,integration,replay,property}/
apps/bruce_cli.cpp
```

## Datenflussregeln

1. `CoordinateContextV1` ist unveraenderlich.
2. Ein `CompatibilitySnapshotV1` ist versioniert und immutable.
3. Cache kann verschwinden; Evidenz darf nicht verschwinden.
4. Ein `ExperienceRecordV1` referenziert fruehere Fakten, ueberschreibt sie nicht.
5. Ein `ResultStatus` darf nur durch Gates hochgestuft werden.

## Status

**Zielarchitektur mit fortgeschrittenem mathematischem Teilkern (Core 0.81).** `core`, `coordinate`, `compatibility`, `math`, `evidence`, `application`, eine begrenzte `experience`-Implementierung und die CLI sind vorhanden. Token-/NanoCell-/Phase-DAG-/Cockpit-Runtime und allgemeine Gate-/Strategieverträge bleiben offen. Die oben gezeigte Zielpipeline ist noch nicht vollständig umgesetzt.

Der [aktuelle Architekturstand mit konkreten M0–M2-Abnahmelücken](../implementation/75-current-architecture-and-milestones.md) unterscheidet ausgeführten Code und weitere Planung. Die [Versionsauswahl des Fünferportfolios](../adr/009-five-family-wire-versions.md) ist für Ausgabe, Export und Replay vereinheitlicht.

## Fortschreibung und tatsächliche Abhängigkeiten

Die oben aufgeführten Modulgrenzen sind Zielvorgaben. Tatsächliche Abweichungen, aktuelle Gate-/Steuerungsverträge und nächste Abnahmeschritte stehen in der [Blueprint-Konsolidierung](03-blueprint-consolidation.md). Sie ersetzt keine Zielmodule durch bereits implementierte Funktionen.
