# Ursprüngliche Umsetzungsphasen

**Historische Planung.** Der aktuelle Implementierungsstand ist in der [Architektur-/Meilensteinmatrix zu Core 0.35](75-current-architecture-and-milestones.md) festgehalten. Insbesondere existiert inzwischen eine begrenzte Erfahrungsplanung. Die folgenden Phasen sind keine aktuelle Bestandsliste.

Die [konsolidierte Entwicklungsroadmap vom 22.09.2026](04-roadmap.md) erweitert diese ursprüngliche Fünf-Phasen-Sicht um Lazy Grid, NanoCell und Phase-DAG. Sie empfiehlt einen früheren mathematischen Durchstich und benennt noch zu entscheidende Vertragskonflikte. Beide Dokumente beschreiben Planung, keine implementierte Runtime.

Die Phasen sind absichtlich vertikal: Jede Phase liefert sichtbares Verhalten, Tests und Replay statt nur abstrakter Infrastruktur.

## Phase 1: Koordinaten, Z-Kompatibilitaet und Replay

**Ziel:** Reversible Adresse, versionierter Snapshot, 64-Bit-Filter, Score, Tie-Break und Trace.

```text
Neue Bereiche:
core/, coordinate/, compatibility/, tests/unit/, tests/property/
```

**Sichtbares Ergebnis:** CLI kann `(x,y)` in `z` umrechnen, Kandidaten bewerten, Auswahl erklaeren und denselben Lauf replayen.

**Abnahme:** COORD-01 bis COORD-03 sowie COMP-01 bis COMP-03 bestehen.

**Nicht enthalten:** Sprache, Persistenz, LLM, Netzwerk, GPU, autonome Lernfreigabe.

## Phase 2: Tokens, Rollen und Erklaerung

**Ziel:** Deterministische Tokenisierung, versionierte Token-Tabelle, Rollen, Mehrdeutigkeit und Erklaerungstrace.

**Sichtbares Ergebnis:** Eine begrenzte Satzdomäne liefert Token, mögliche Rollen, Regeln und sichtbare Alternativen.

**Abnahme:** TOKEN-01, Rollen-/Mehrdeutigkeitstests und Replay mit alter Tabelle bestehen.

**Nicht enthalten:** Freies Sprachmodell als Entscheider oder allgemeines Sprachverstaendnis.

## Phase 3: Append-only Experience und LearnGate

**Ziel:** immutable Experience-Records, Korrekturketten, externe Bestaetigung und geschlossenes LearnGate.

**Sichtbares Ergebnis:** Nutzer sehen historische Erfahrungen, Korrekturen und warum eine Regelversion abgeloest wurde.

**Abnahme:** EXP-01 bis EXP-03 sowie EVID-01 bestehen.

**Nicht enthalten:** automatische Regelpromotion ohne unabhängige Evidenz.

## Phase 4: BRUCE-Math

**Ziel:** Strategie-Port, exakte Hypothesen, Gegenbeispiele, Hold-out und EvidenceGate.

**Sichtbares Ergebnis:** Eine mathematische Hypothese wird nachvollziehbar als Kandidat, widerlegt oder verifiziert dargestellt.

**Abnahme:** HOLDOUT-01, Gegenbeispieltests und Replaytests bestehen.

**Nicht enthalten:** unbelegte Krypto-/Mining-Optimierung oder allgemeine mathematische Allwissenheit.

## Phase 5: Research-Cockpit

**Ziel:** Vollständiger Benutzerworkflow mit Eingabe, Resultat, Evidenz, Experience-Historie und Replay.

**Sichtbares Ergebnis:** Ein Nutzer kann einen Run starten, verstehen und wiederholen.

**Abnahme:** E2E-01 sowie Fehler- und Budgetpfade bestehen.

## Status

**Blueprint-Vorschlag.**
