# Grid-Import und Forschung gemeinsam begrenzen – Core 0.40

Bruce kann jetzt einen gemeinsamen logischen Arbeitsrahmen für die Prüfung der Grid-Quellexporte und die anschließende Forschung verwenden.

```bash
./build/bruce grid-batch --paths '0,2;1,2;2,2' --output budget-lernen.bruce
./build/bruce grid-batch --paths '3,2;4,2' --output budget-pruefen.bruce
./build/bruce research --strategy polynomial --domain rational --max-degree 2 \
  --grid-train budget-lernen.bruce --grid-holdout budget-pruefen.bruce \
  --max-total-operations 100000 --output budget-forschung.bruce
./build/bruce replay budget-forschung.bruce
```

Die äußere Ausgabe zeigt unter `work_budget`, wie viel für die Quellenprüfung berechnet wurde, wie viel der Forschung verbleibt und wie viel insgesamt genutzt wurde. Die Beispielpaare ergeben weiterhin `x²` mit `VERIFIED_ON_SAMPLES`.

## Was wird gezählt?

Ein logischer Importschritt je Byte der beiden vollständigen Grid-Dateien. Danach zählt die Forschung nach ihrem bisherigen Operationsmodell. Das ist eine reproduzierbare Abrechnungsregel, keine Millisekunden- oder CPU-Instruktionsmessung.

Gesamtbudget und Forschungsbudget liegen jeweils zwischen 1 und 10000000. Reicht der Gesamtrahmen nicht für alle Quellenbytes **plus mindestens eine Forschungseinheit**, wird der Auftrag ohne Export abgewiesen. Die Quellenprüfung wird als ganzer Abschnitt zugelassen; sie erzeugt keine halb geprüften Quellenexporte.

## Ein strengeres Einzelbudget bleibt wirksam

```bash
./build/bruce research --strategy polynomial --domain rational --max-degree 2 \
  --grid-train budget-lernen.bruce --grid-holdout budget-pruefen.bruce \
  --max-total-operations 100000 --max-operations 1 --output budget-teilbefund.bruce
./build/bruce replay budget-teilbefund.bruce
```

Dieser Lauf darf trotz des größeren Gesamtbudgets nur eine Forschungseinheit nutzen. Der Teilbefund bleibt `INCOMPLETE` und replaybar. Er wird nicht als fertige Lösung an die Z-Auswahl weitergegeben.

Die Option funktioniert auch mit `--grid-axis y`. Ohne `--max-total-operations` bleiben die bisherigen getrennten Grenzen und Exportformate erhalten. Mit Gesamtbudget verwendet Bruce V3; alle Quellen und die ursprüngliche X-/Y-Projektion bleiben eingebettet. Vorhandene Dateien werden niemals überschrieben.

## Grenzen

Der Rahmen umfasst diesen Quellenimport und die anschließende Forschung. Dateieinlesen, äußere Metadatenprüfung, Darstellung und spätere Z-Auswahl sind keine zusätzlich gemessenen Einzeloperationen darin. Allgemeine Phasensteuerung, unterbrechbare Quellenprüfung, Wiederaufnahme und Grid-Expansion bleiben offen. Ein vollständiger M2-Abschluss folgt daraus nicht.

[Vertrag](84-grid-budget-contract.md) · [Abnahme](acceptance/grid-budget-v1-2026-09-22.md)
