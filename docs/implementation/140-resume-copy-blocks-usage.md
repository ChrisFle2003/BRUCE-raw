# Große Vorgänger beim Export unterbrechbar kopieren / Core 0.67

Bei Grid-Fortsetzungen enthält die neue Ergebnisdatei ihren geprüften Vorgänger. Große Vorgänger werden beim Inhaltsaufbau jetzt in 64-KiB-Blöcken kopiert. Zwischen den Blöcken greifen die vorhandenen Kontrollpunkte.

## Benutzung

Ein vorhandenes unvollständiges Grid-Ergebnis fortsetzen und die gesamte Prüfsitzung begrenzen:

```bash
./build/bruce grid-resume --from-run runs/partial.bruce --additional-queries 2 \
  --verification replay --max-session-checkpoints 100000
```

Alternativ oder zusätzlich Signale beim Aufbau der Grid-Evidenz beachten:

```bash
./build/bruce grid-resume --from-run runs/partial.bruce --additional-queries 2 \
  --verification replay --serialization-cancellation grid-record-checkpoints
```

Die Eingabedatei muss eine gültige unvollständige Quelle sein. Die Zahlen sind Beispielgrenzen, keine Zusage für beliebige Vorgänger. Bei einem erkannten Abbruch während der Elternkopie steht im Fehlerbericht `failed_at: SERIALIZING`; kein Ergebnis wird freigegeben oder exportiert. Eine bereits vorhandene Ausgabedatei bleibt erhalten.

## Was sich verändert

- Elterninhalte bis einschließlich 65.536 Bytes behalten ihre bisherigen Kontrollpunkte.
- Größere Inhalte erhalten vor jedem weiteren Kopierblock einen zusätzlichen Punkt.
- Das gemeinsame Prüfpunktbudget zählt diese zusätzlichen Stellen in SERIALIZING. Alte Limits können für große Eltern daher zu klein sein.
- Mit ausreichendem Budget bleiben native Ergebnis- und Bundlebytes unverändert. Mathematik, Cache und gespeicherte Fortsetzungshistorie ändern sich nicht.

## Was noch offen ist

Speicherreservierungen, abschließende Kopien beim Zusammenbau und andere Payloadfamilien bleiben teilweise atomar. Die Änderung garantiert keine maximale Signallatenz oder Arbeitsspeichergrenze. Die Auflösung dieser weiteren atomaren Arbeiten und separate Byte-/Speicherverträge bleiben nächste Konsolidierungsschritte.

[Vertrag](139-resume-copy-blocks-contract.md) · [Architekturstand](75-current-architecture-and-milestones.md) · [M0–M2-Abnahmematrix](136-m0-m2-acceptance-matrix.md)
