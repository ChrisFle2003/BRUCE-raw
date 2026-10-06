# Core 0.74: nachvollziehbare Forschungszulassung

## Zweck und Umfang

Die vorhandene zentrale Funktion `research_gates` entscheidet bereits über die Auswahl abgeschlossener Forschung und die fachlichen Voraussetzungen für Erfahrungsstimmen. Core 0.74 ergänzt typisierte Gründe. Die bestehenden Ja/Nein-Regeln und Fehlermeldungen der Auswahl- und Experience-Verbraucher bleiben erhalten. Es entstehen keine neuen Bundlebytes und keine automatische Promotion.

## Vertrag abrufen

```bash
bruce gate-contracts
```

Ohne Argumente liefert der Befehl JSON mit `contract: research-gate-policy-v1`, den akzeptierten Abschlussstatus, der Reihenfolge der Ablehnungsgründe und den dauerhaft gesperrten Befugnissen. Zusätzliche Argumente führen zu Exit 2. Der Befehl beschreibt die Regeln; er prüft keinen konkreten Lauf und autorisiert keinen Import.

## Entscheidungen und Gründe

`ResearchGateDecision` ergänzt `selection_reason`, `experience_base_reason` und `experience_reason` vom Typ `ResearchGateReason`. `research_gate_reason_name` liefert stabile Textcodes. Jede zugehörige Entscheidung ist genau dann true, wenn ihr Grund `Allowed` lautet. Bei mehreren Hindernissen gilt die erste passende Zeile:

| Priorität | Code | Bedingung |
| --- | --- | --- |
| 1 | `StatusNotCompleted` | Status ist weder CANDIDATE, NO_CANDIDATE, REFUTED, AMBIGUOUS noch VERIFIED_ON_SAMPLES |
| 2 | `StopNotCompleted` | Stopgrund ist nicht Completed |
| 3 | `HoldoutMissing` | Kein Holdout vorhanden |
| 4 | `DependentSource` | Quelle nicht unabhängig |
| 5 | `PortfolioIncomplete` | Portfolio unvollständig |
| 6 | `VerifiedHypothesisMissing` | Keine auf Stichproben bestätigte Hypothese |

Auswahl prüft die ersten zwei Bedingungen, die Experience-Basis die ersten fünf, Erfahrungszulassung alle sechs. `selection_checks`, `experience_base_checks` und `experience_checks` geben diese Präfixlängen im CLI-Vertrag an. Auswahlzulassung besagt nicht, dass ein Kandidat existiert; auch eine vollständig abgeschlossene negative Suche darf ausgewertet werden.

## Vertrauensgrenze und Kompatibilität

Die Funktion wertet vom Aufrufer gelieferte Fakten aus. Sie führt selbst keinen Replay und keine Quellenprüfung durch. Die vorhandenen Verbraucher behalten ihre Quellen-, Format-, Größen-, Budget- und Replayprüfungen. Insbesondere ersetzt `Allowed` weder die Experience-Importprüfung noch eine Kandidatenprüfung. `assess-run` bleibt unverändert und enthält diese Gateentscheidungen noch nicht.

`learn_promotion`, `publish` und `general_proof` bleiben false. Stichprobenbestätigung ist weiterhin kein allgemeiner Beweis. Die gemeinsame Ergebnisbewertung aus Core 0.73 und die fachliche Forschungszulassung sind unterschiedliche Verträge: ein vollständig bearbeiteter Lauf kann fachlich nicht zugelassen sein.

## Nachweis und weitere Architektur

Die Vertragstests prüfen 224 Kombinationen aus sieben Statuswerten und fünf booleschen Fakten, einschließlich unbekanntem Status, fehlendem Holdout, abhängiger Quelle, Teilportfolio und Abbruch. CLI-Tests prüfen Katalog und Argumentablehnung. Die bestehenden Auswahl- und Erfahrungsworkflows sichern die bisherigen Import- und Auswahlgrenzen ab.

Offen bleiben die gemeinsame Ableitung dieser Fakten aus allen Anwendungsergebnisarten, replaygeprüfte Gateinspektion pro Lauf, weitere Geltungsbereiche und allgemeine Gatezustandsmaschinen. Optimierung bleibt nachrangig. Dies ist kein vollständiger M0–M2-Abschluss.

## Ausgeführte Abnahme am 25.09.2026

- Lokaler Debug-Build: 108/108 Testgruppen bestanden, einschließlich der erweiterten Vertragstests.
- Per SCP auf Bruce-AI übertragen: Release-Build, 108/108 Testgruppen, Installation und TGZ-Paketprüfung für 0.74.0 erfolgreich.
- CLI-Dokumentation lokal und remote: 88 erfolgreiche Beispielaufrufe, 63 dokumentierte Optionen, 323 Diagnosen und interne Links geprüft.
- Die neuen Tests scheiterten vor Implementierung an der fehlenden Reason-API beziehungsweise `UnknownCommand: gate-contracts`.

Die Prüfnachweise gelten für die untersuchten Linux-Umgebungen; das Archiv wurde nicht öffentlich veröffentlicht oder signiert.
