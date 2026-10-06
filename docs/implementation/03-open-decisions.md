# Offene Entscheidungen und ADR-Backlog

**Historischer Backlog:** Umgesetzte Teilverträge und verbleibende Lücken sind in der [aktuellen Architekturmatrix](75-current-architecture-and-milestones.md) zugeordnet. Die neue [ADR-009](../adr/009-five-family-wire-versions.md) dokumentiert die implementierte gemeinsame Versionsauswahl.

Diese Punkte muessen vor der jeweiligen Implementierungsphase entschieden und als ADR dokumentiert werden.

| ID | Entscheidung | Optionen | Empfehlung | Phase |
| --- | --- | --- | --- | --- |
| ADR-001 | Zahlentyp fuer grosse z-Werte | 64-Bit, BigInt | klare Ueberlaufgrenze zuerst | 1 |
| ADR-002 | Persistenz | JSONL, CBOR, SQLite, Event Store | JSONL/CBOR-MVP | 3 |
| ADR-003 | Scoregewichtung | Boolesch, Integer, Rational | Boolesch/Integer-MVP | 1 |
| ADR-004 | externe Evidenz | manuell, signiert, Pruefdienst | manuell plus Replay zuerst | 3 |
| ADR-005 | Unicode-Normalisierung | enge Domäne, vollstaendige NFC/NFKC | dokumentierte enge Domäne | 2 |
| ADR-006 | UI | CLI, Desktop, Web | CLI zuerst | 1/5 |
| ADR-007 | Parallelitaet | sequenziell, Threadpool | sequenzieller Referenzpfad zuerst | nach 4 |
| ADR-008 | LLM-Anbindung | keine, Vorschlagsassistent | nur begrenzter Vorschlagsassistent | nach 5 |

## ADR-Vorlage

```markdown
# ADR-NNN: Titel

## Status
Vorgeschlagen | Akzeptiert | Verworfen | Ersetzt

## Kontext
Welche konkrete Entscheidung steht an?

## Entscheidung
Welche Option wird verwendet?

## Konsequenzen
Welche Vorteile, Kosten und Replay-Folgen entstehen?

## Tests und Migration
Welche Tests sichern die Entscheidung? Wie bleiben alte Traces lesbar?
```

## Status

**Offene Entscheidung.**
