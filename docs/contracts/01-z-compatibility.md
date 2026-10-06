# Z-Kompatibilitaetsraum

## Zweck

`ZCompatibilityEngineV1` bewertet Kandidaten in einem versionierten und deterministischen Kompatibilitaetsraum. Sie setzt keine mathematische Wahrheit fest; sie erzeugt eine nachvollziehbare Auswahl oder sichtbar bleibende Mehrdeutigkeit.

```text
CoordinateContext(x, y, z)
  -> CompatibilitySnapshot
  -> Filtergruppen
  -> Score und Widersprueche
  -> Auswahl / AmbiguitySet
  -> SelectionTrace
```

## Datenmodelle

| Modell | Zweck | Pflichtfelder |
| --- | --- | --- |
| `CompatibilityRuleV1` | einzelne pruefbare Regel | ID, Version, Bedingung, Gewicht, Quelle |
| `CompatibilityFilterGroupV1` | bis zu 64 boolesche Merkmale/Kandidaten | Gruppen-ID, Bitmaske, Semantik |
| `CompatibilitySnapshotV1` | immutable Regelzustand | `rule_version`, Filtergruppen, Digest |
| `CandidateScoreV1` | Bewertung eines Kandidaten | Kandidat, Score, Widersprueche, Evidenzanzahl |
| `CandidateSelectionV1` | Gewinner oder Mehrdeutigkeit | Kandidaten, Entscheidung, Tie-Break |
| `SelectionTraceV1` | vollstaendige Auswahlbegruendung | Kontext, Regeln, Masken, Scores |
| `NegativeEvidenceV1` | Gegenbeispiel oder harter Ausschluss | Evidenz-ID, Regel-ID, Grund |
| `RuleSourceV1` | Herkunft einer Regel | manuell, abgeleitet, extern bestaetigt |
| `ExperienceCorrectionV1` | nachvollziehbare Korrektur | alt, neu, Begruendung |

## Filtergruppen

Eine einzelne Bitmaske reicht nicht aus. Mindestens getrennt speichern:

```text
positive_match_mask       erfuellte positive Regeln
hard_reject_mask          unzulaessige Kandidaten
negative_evidence_mask    entkraeftete Kandidaten
context_role_mask         rollenpassende Kandidaten
```

Ein `uint64_t` repraesentiert dabei 64 boolesche Werte. Mehr als 64 Kandidaten verwenden mehrere Gruppen; die Gruppenreihenfolge ist Teil der Version.

## Deterministische Auswahl

```text
score(candidate) = Summe der Gewichte erfuellter Regeln

Vor dem Ranking:
1. harte Ausschluesse und negative Filter anwenden;
2. unvollstaendig gepruefte oder kontextwidrige Kandidaten ausschliessen.

Ranking:
1. hoeherer Score;
2. mehr nachweislich unabhaengige Evidenz, sobald ein Evidenzpruefer existiert;
3. verbleibender fachlicher Gleichstand bleibt mehrdeutig.

Stabile Kandidaten-IDs bestimmen ausschliesslich Ausgabe- und Gruppenreihenfolge.
Die numerische V1 setzt unabhaengige Evidenzzahlen auf null, da ein externer
Evidenzpruefer noch nicht implementiert ist.
```

Ein Gleichstand wird nicht still durch Zufall, Zeit oder Hash-Reihenfolge aufgeloest.

## Fehler und Budgets

| Fall | Ergebnis |
| --- | --- |
| unbekannte Regelversion | `UnknownRuleVersion` |
| Kandidatenlimit erreicht | `CandidateBudgetExceeded` |
| Trace zu gross | `TraceBudgetExceeded` |
| widerspruechliche Filtersemantik | `InvalidCompatibilitySnapshot` |
| vollstaendiger Gleichstand | `AmbiguousSelection` mit Alternativen |

## Replay

Ein numerischer V1-Replay enthaelt Domain, Layer, x/y (z wird exakt abgeleitet), Regelsprachversion, den vollstaendigen kanonischen Snapshot mit Digest, Kandidaten, Eingabefingerprint, Budgets, Abbruchposition und logischen Trace. Bei Forschungsimport kommt das Ursprungspaket hinzu. Token-/Experience-Versionen werden erst mit den entsprechenden Modulen gebraucht; sie werden nicht als erfundene Platzhalter ausgegeben. Historische z-Adressen werden nie umgeschrieben.

## Tests

- gleiche Eingabe und Snapshot liefern denselben Gewinner;
- jeder Tie-Break-Schritt wird einzeln getestet;
- vollstaendiger Gleichstand erzeugt Mehrdeutigkeit;
- negative Evidenz schliesst Kandidaten sichtbar aus;
- manipulierter Snapshot-Digest wird abgewiesen.

## Status

**Numerischer V1-Vertrag implementiert und getestet:** Snapshots, ganzzahlige/modulare Bedingungen, getrennte Filtergruppen, fachliche Mehrdeutigkeit, Trace und Replay. Dateiformat, Grenzen und Nachweise stehen in [Core 0.2](../implementation/08-z-compatibility-usage.md). Die Typentabelle beschreibt weiterhin auch zukuenftige Experience-/Evidenzobjekte; deren externe Validierung und Korrekturketten sind noch nicht implementiert.

## Verbindliche Regelhoheit

Aktive Regeländerungen erfordern ausdrückliche menschliche Freigabe der konkreten Änderung. Vorschläge, Erfahrungsstimmen und Prüferfolge ändern keine aktiven Regeln. Herkunft `manual` ist kein authentifizierter Freigabenachweis. [ADR-013](../adr/013-human-rule-authority.md) definiert Geltungsbereich, geplanten Freigabeablauf und die noch offene technische Durchsetzung.
