# Forschungsbundle → Z-Auswahl – Core 0.102.0

[Handbuch](README.md) · [Direkte Auswahl](commands.md#compatibility--numerische-auswahl-mit-regeln) · [Aktive Regeln](active-rules.md)

## Ablauf

Die Übergabe bindet ein Forschungsbundle, die kanonische Regelfassung, eine Auswertungsstelle und feste Auswahlbudgets. Planung und Ausführung verwenden denselben bestehenden Kompatibilitätsausführer einschließlich Quellenreplay und Projektion der Forschungshypothesen. Der Plan ist kein Freigabenachweis und ersetzt keine spätere Prüfung.

```text
bruce compatibility-plan --from-run SOURCE --rules RULES --at X
    [--max-source-bytes N] [--max-candidates N] [--max-checks N]
    [--max-trace N] --output PLAN
bruce compatibility-apply PLAN --from-run SOURCE --rules RULES [--output RUN]
```

Bei beiden Befehlen kann `--rule-store STORE` anstelle von `--rules RULES` verwendet werden. Beide Optionen zugleich sind verboten. Der aktive Speicher wird vollständig geprüft und seine aktuelle Regelfassung einmal eingelesen. Es gibt keine automatische Regelaktivierung.

## Parameter

| Parameter | Geltung / Grenze |
| --- | --- |
| `--from-run SOURCE` | Beide Befehle, Pflicht: ein vom bestehenden Kompatibilitätsimport unterstütztes Forschungsbundle |
| `--rules RULES` | Regeldatei, maximal 1048576 Bytes; alternativ aktiver Speicher |
| `--rule-store STORE` | Alternativ zu `--rules`; bestehender aktiver lokaler Regelspeicher mit dessen 8-MiB-/128-Entscheidungen-Grenzen |
| `--at X` | Nur Planung, Pflicht: dezimale Int64-Auswertungsstelle. Grid-Projektionen verlangen weiterhin 0..7 |
| `--max-source-bytes N` | Nur Planung, 0..33554432, Standard 33554432 |
| `--max-candidates N` | Nur Planung, 1..4096, Standard 4096 |
| `--max-checks N` | Nur Planung, 1..1048576, Standard 1048576 |
| `--max-trace N` | Nur Planung, 1..65536, Standard 65536 |
| `--output` | Planung: Pflicht, neue Plandatei. Ausführung: optionales neues mathematisches Ergebnisbundle |
| `PLAN` | Bei Ausführung erster Positionsparameter; maximal 4096 Bytes |

Keine Kandidaten-CSV, keine anderen Strategieparameter, kein allgemeiner Sitzungscontroller und keine zusätzlichen Replayoptionen. Die separate [Transfersteuerung](transfer-control.md) ergänzt seit Core 0.94 optionale Prüfpunkte und Signalprüfung bei der Ausführung. Die Auswahlbudgets zählen die bestehende Z-Auswahlarbeit; sie begrenzen nicht pauschal Quellenreplay, Projektion, Prozessspeicher oder Laufzeit. Native Importgrenzen gelten zusätzlich. Ohne Ausgabepfad liefert die Ausführung nur den JSON-Bericht.

## Was gebunden und erneut geprüft wird

1. Die Quelle wird über SHA-256 ihrer **vollständigen ursprünglichen Bytes** gebunden.
2. Regeln werden geparst und kanonisiert. Gebunden wird SHA-256 der **kanonischen Regelbytes**, einschließlich Snapshotkennung und Reihenfolge. Reine Unterschiede in Abständen bleiben damit zulässig.
3. Auswertungsstelle und die vier Grenzen stehen unveränderlich im Plan. Bei `compatibility-apply` können sie nicht überschrieben werden.
4. Ausführung vergleicht Quelle und Regelfassung und ruft anschließend den nativen Kompatibilitätsausführer erneut auf. Ein neu berechneter Plandigest macht eine ungeeignete Quelle nicht gültig.

Die Bindung enthält keinen Dateipfad und keine Journalgeneration. Andere Eingabepfade mit gleichen gebundenen Inhalten sind zulässig. Ist die aktive Regelfassung inzwischen eine andere, wird die Ausführung abgelehnt. Ein Rückwechsel auf exakt denselben kanonischen Inhalt erfüllt die Inhaltsbindung wieder. Der Plan ist wiederverwendbar und keine einmalige Zustimmung.

Nach Einlesen bleiben die Eingaben für diesen Lauf fest. Ein späterer Wechsel im Regelspeicher verändert den laufenden Auftrag nicht. Das ist keine Transaktion über sämtliche Eingabepfade; direkte externe Dateischreibzugriffe werden nicht gesperrt.

## Planung ist keine Zusage vollständiger Auswahl

`compatibility-plan` führt den nativen Auftrag als Probelauf im Speicher aus. Parser-, Replay-, Projektions- und Eignungsfehler verhindern den Planexport. Ein regulärer fachlicher Teilbefund `INCOMPLETE`, etwa bei zu kleinem Prüfbudget, ist dagegen ein gültiges Ergebnis und verhindert den Plan nicht. Planung garantiert weder einen Gewinner noch Vollständigkeit. Der Planbericht enthält Bindungen und Budgets; den tatsächlichen Auswahlbefund liefert die Ausführung.

Auch `NO_COMPATIBLE_CANDIDATE` und Gleichrangigkeit sind reguläre Ergebnisse. Der native Status, Stopgrund, Kandidatenbewertungen, Masken und Trace bleiben maßgeblich. Ein Samplebefund wird durch Übergabe nicht zum allgemeinen mathematischen Beweis.

## Ausgabe und Export

Planung: Exit 0 und JSON `compatibility-transfer-v1` mit `source_digest`, `rules_digest`, `at` (Dezimalstring), `max_source_bytes`, `max_candidates`, `max_checks`, `max_trace`.

Ausführung: Exit 0 und JSON `compatibility-transfer-result-v1` mit:

- `plan_digest`: SHA-256 der gesamten kanonischen Plandatei;
- `source_digest`, `rules_digest`: gebundene Eingaben;
- `result`: unveränderter nativer Kompatibilitätsbericht.

`--output` bei Ausführung erzeugt das bestehende Kompatibilitätsbundle mit eingebetteter Quelle und Regelfassung. Seine Bytes entsprechen der direkten Auswahl mit denselben Parametern. Es lässt sich mit `replay`, `inspect-run`, `assess-run` und `gate-run` im Rahmen ihrer bestehenden Verträge prüfen. Die äußere Transferberichtshülle und der Plan werden nicht in das mathematische Bundle eingebettet. Für die Übergabeprovenienz Plan und JSON-Bericht zusätzlich aufbewahren.

Alle Exporte sind exklusiv: bestehende Dateien werden nicht überschrieben; Elternverzeichnisse müssen vorhanden sein. Kein bestehender Regelspeicher oder Erfahrungssnapshot wird verändert. Normale Fehler liefern JSON auf stderr und Exit 2. Erfolgsausgaben sind reines JSON auf stdout. Betriebssystemterminierung besitzt hier keinen besonderen Teilbefundvertrag.

## Vollständiges Beispiel

Vom Projektstamm aus:

```bash
selection_dir=$(mktemp -d)
printf 'x,y\n0,1\n1,3\n' > "$selection_dir/train.csv"
printf 'x,y\n2,5\n' > "$selection_dir/hold.csv"
printf 'BRUCE_RULES_V1\nsnapshot auswahl\nrule positiv positive value ge 0 0 1 manual -\n' > "$selection_dir/rules.txt"
./build/bruce research --train "$selection_dir/train.csv" --holdout "$selection_dir/hold.csv" --output "$selection_dir/source.bruce"
./build/bruce compatibility-plan --from-run "$selection_dir/source.bruce" --rules "$selection_dir/rules.txt" --at 3 --output "$selection_dir/selection.plan"
./build/bruce compatibility-apply "$selection_dir/selection.plan" --from-run "$selection_dir/source.bruce" --rules "$selection_dir/rules.txt" --output "$selection_dir/selection.bruce"
./build/bruce replay "$selection_dir/selection.bruce"
```

Für aktive Regeln bei beiden Übergabebefehlen `--rules ...` durch `--rule-store ...` ersetzen. Nach einer inhaltlichen Regeländerung einen neuen Plan erstellen. Der Ablauf braucht keine Regelbestätigung, da er ausschließlich vorhandene Regeln liest und anwendet.

## Planformat

Die Datei enthält SHA-256 des folgenden Körpers, LF und anschließend den Körper. Jede Körperzeile endet mit LF:

```text
BRUCE_COMPATIBILITY_TRANSFER_V1
<source_digest>
<rules_digest>
<at als vorzeichenbehaftete Dezimalzahl>
<max_source_bytes>
<max_candidates>
<max_checks>
<max_trace>
```

Digests bestehen aus 64 kleingeschriebenen Hexzeichen. Zahlen sind kanonisch dezimal. Zusätzliche Felder, veränderte Schreibweisen oder andere Versionen werden abgelehnt. Quelle und Regeln sind nicht im Plan enthalten und müssen bei Ausführung verfügbar bleiben. Die äußere Prüfsumme ist keine Signatur.

## Fehler

| Fehler / Präfix | Bedeutung |
| --- | --- |
| `UseRulesOrRuleStore` | Beide Regelquellen zugleich angegeben |
| `CompatibilityTransferInvalidBudget` | Grenze außerhalb des zulässigen Bereichs; direkt im C++-Einstieg auch nicht unterstütztes `cancel_after_candidates` |
| `InputByteLimit` | CLI-Dateilesen überschreitet Eingabegrenze |
| `CompatibilityTransferByteLimit` | Anwendungseinstieg erhält eine Quelle oberhalb der Plangrenze |
| `CompatibilityTransferPlanByteLimit` | Direkter Plandecoder erhält mehr als 4096 Bytes |
| `CompatibilityTransferDigestMismatch` | Äußere Planprüfsumme fehlt oder stimmt nicht |
| `InvalidCompatibilityTransfer` | Version, Felder, Digests oder kanonisches Format ungültig |
| `CompatibilityTransferSourceChanged` | Quelldigest weicht ab |
| `CompatibilityTransferRulesChanged` | Kanonische Regelfassung weicht ab |
| Native Import-/Projektionsfehler | Beispielsweise ungeeigneter/unvollständiger Forschungslauf, keine projizierbaren Kandidaten, unzulässige Grid-Stelle oder nicht darstellbarer Ergebniswert |
| Bestehende Datei-/Exportfehler | Eingaben nicht lesbar oder Ausgabe bereits vorhanden/nicht erzeugbar |

Die tatsächlich zuerst erreichte Prüfung bestimmt den Fehler. Die CLI liest die Regelquelle vor dem Plan; eine ungültige Regeldatei kann daher vor einem beschädigten Plan gemeldet werden. Ein Budget-Teilbefund ist kein solcher Parserfehler: `result.status` und `result.stop_reason` auswerten.

Die Funktion ist eine zweite konkrete Übergabe neben [Forschung → Erfahrung](experience-transfer.md), keine allgemeine Pipeline- oder Phase-DAG-Laufzeit.

## Zusätzliche Ausführungssteuerung

`compatibility-apply` akzeptiert seit Core 0.94 `--max-transfer-checkpoints N` und `--transfer-cancellation none|checkpoints`. Die Parameter des Plans bleiben unverändert. Bei Angabe einer dieser Optionen umschließt `controlled-transfer-v1` den bisherigen Bericht. [Details und Exportgrenze](transfer-control.md).

Seit Core 0.95 gelten dieselben [Steuerungsoptionen](transfer-control.md) auch für die Planung. Das Konto gilt pro Aufruf und wird nicht im Plan gespeichert.
