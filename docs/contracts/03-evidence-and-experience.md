# Evidenz, Replay und Append-only Experience Store

## Grundregel

Erfahrungen werden nicht unsichtbar ueberschrieben. Eine Korrektur erzeugt ein neues Ereignis, das die alte Erfahrung referenziert und den Grund dokumentiert.

```text
Run -> EvidenceRecord -> Countercheck -> Replay
    -> ExperienceRecord -> Review -> Rule/Experience-Version
```

## ExperienceRecordV1

| Feld | Zweck |
| --- | --- |
| `experience_id` | unveraenderliche Ereignis-ID |
| `sequence` | stabile Reihenfolge |
| `parent_experience_id` | Bezug auf Vorgaenger oder Korrektur |
| `domain_id`, `layer`, `x`, `y`, `z` | Koordinatenidentitaet |
| `version_bundle` | Formel-, Regel-, Token- und Policy-Version |
| `input_fingerprint` | Eingabeidentitaet |
| `trace_id` | Replay-Einstieg |
| `evidence_refs` | unabhaengige Nachweise |
| `result_status` | Ergebniszustand |
| `supersedes` | referenzierte, nicht geloeschte Erfahrung |
| `correction_reason` | Gegenbeispiel oder externer Beleg |

## Learn-Grenze

```text
CANDIDATE != VERIFIED
VERIFIED != LearnGate
ELIGIBLE_FOR_REVIEW != PublishGate
```

Positive Lernerfahrung verlangt unabhaengige Pruefung oder externes Feedback. Wiederholung desselben Inputs gilt nicht als unabhaengige Evidenz.

## Integritaet

- Jede Experience referenziert immutable Versionen.
- Trace und Ergebnis erhalten einen Digest/Fingerprint.
- Ein fehlender oder abweichender Bezug erzeugt `InvalidEvidence`.
- Cache ist nie alleinige Evidenzquelle.

## Tests

- Append fuegt Ereignis hinzu, veraendert kein altes Ereignis;
- Korrektur referenziert bestehende Erfahrung;
- selbstbestaetigender Run oeffnet kein LearnGate;
- manipulierter Trace-Digest wird abgewiesen;
- Replay mit alter Version bleibt reproduzierbar.

## Status

**Allgemeiner Vertrag weiterhin Blueprint.** [Core 0.11](../implementation/26-experience-routing-usage.md) implementiert ein ausdrücklich begrenztes Teilpaket für die Suchreihenfolge: unveränderliche Snapshots, vollständiger Quellen-Replay, Eingabe-Deduplizierung und Widerrufsereignisse. Der eigene [Routing-Vertrag](../implementation/25-experience-routing-contract.md) ersetzt nicht alle oben beschriebenen ExperienceRecordV1-Felder, externe Evidenzprüfung oder Rule-/LearnGate-Promotion.

## Verbindliche Regelhoheit

Aktive Regeländerungen erfordern ausdrückliche menschliche Freigabe der konkreten Änderung. Vorschläge, Erfahrungsstimmen und Prüferfolge ändern keine aktiven Regeln. Herkunft `manual` ist kein authentifizierter Freigabenachweis. [ADR-013](../adr/013-human-rule-authority.md) definiert Geltungsbereich, geplanten Freigabeablauf und die noch offene technische Durchsetzung.
