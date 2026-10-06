# Core 0.44: Erfahrungen nachvollziehbar ersetzen

Bruce kann eine gespeicherte Erfahrung durch einen neu geprüften Forschungsbefund ersetzen. Das ist beispielsweise nützlich, wenn für dieselben Lernpaare zusätzliche Prüfwerte vorliegen.

```bash
./build/bruce experience inspect erfahrung.bruce
./build/bruce experience replace --store erfahrung.bruce \
  --id ALTE_DATENSATZ_ID --reason 'Mit zusätzlichen Prüfwerten erneut geprüft' \
  --from-run neue-forschung.bruce --output korrigierte-erfahrung.bruce
./build/bruce experience inspect korrigierte-erfahrung.bruce
```

`ALTE_DATENSATZ_ID` ist durch die angezeigte `records[].id` eines aktiven Datensatzes zu ersetzen. `neue-forschung.bruce` muss ein abgeschlossener unabhängiger Portfoliolauf mit Holdout und mindestens einer auf Samples bestätigten Hypothese sein. Auch geprüfte Grid-Forschungsquellen sind zulässig.

Im neuen Snapshot ist der Vorgänger inaktiv. Der neue aktive Datensatz verweist über `replaces` auf ihn; unter `corrections` stehen Grund und Nachfolger. Der alte Snapshot bleibt unverändert. Ungültige Ersatzquellen erzeugen keinen neuen Export.

Für die nächste Forschung wird der neue Snapshot wie gewohnt mit `--planner experience-order --experience korrigierte-erfahrung.bruce` verwendet. Nur der aktive Nachfolger liefert Stimmen. Die vollständige Vorgeschichte bleibt trotzdem erhalten und ist ohne Originaldateien prüfbar.

Mehrere Ersetzungen dürfen dieselben Lernpaare innerhalb dieser Kette verwenden. Ein zweiter unabhängiger Datensatz mit denselben Lernpaaren bleibt ausgeschlossen. Die Obergrenze von acht historischen Datensätzen gilt auch für Ersetzungen. Soll eine Erfahrung nur entfernt werden, bleibt `experience revoke` verfügbar.

## Weitere Roadmap

Begrenzte Korrekturketten mit geprüftem Nachfolger, Herkunft und Widerruf sind implementiert. Automatische Korrekturentscheidungen, externe Evidenz und Regelpromotion bleiben offen. Ebenso allgemeine Aufgaben-/Strategie-/Gate-Verträge, gemeinsame Audit-/Suchbudgets, fortsetzbare Forschung, Sprache, NanoCell, Cockpit, Phase-DAG, GPU und BigInt im Grid/Z. Dies ist kein allgemeines Lernen und kein vollständiger M4-Abschluss.

[Vertrag](92-experience-replacement-contract.md) · [Abnahme](acceptance/experience-replacement-v1-2026-09-22.md)
