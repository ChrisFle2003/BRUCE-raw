# Core 0.60: Grid-Bundle beim Inhaltsaufbau abbrechen

**Fortschreibung Core 0.67:** Die Elternkopie im Fortsetzungs-Payload besitzt jetzt [zusätzliche Kontrollpunkte zwischen 64-KiB-Blöcken](140-resume-copy-blocks-usage.md). Die unten beschriebenen ursprünglichen Grenzen sind insoweit überholt; andere Kopien und Speicherreservierungen bleiben teilweise atomar.

Grid-Stapel und Fortsetzungen können jetzt schon beim Zusammenstellen ihrer Evidenz auf einen Abbruch reagieren:

```bash
./build/bruce grid-batch --paths '0,1;1,1;2,1' --max-query-steps 1 \
  --verification replay --serialization-cancellation grid-record-checkpoints \
  --verification-cancellation grid-query-boundaries --output teil.bruce

./build/bruce grid-resume --from-run teil.bruce --additional-queries 2 \
  --verification replay --serialization-cancellation grid-record-checkpoints \
  --verification-cancellation grid-query-boundaries --output fortsetzung.bruce
```

Die neue Option prüft zwischen gespeicherten Pfaden, Trace-Einträgen und Ergebniszeilen auf Abbruch. Sie enthält außerdem die blockweise Unterbrechung der äußeren Prüfsumme. Ohne Abbruch bleiben Ergebnisse, Cache-Zähler und native Bundlebytes gleich; der Sitzungsbericht verwendet V9.

Strg+C wird an diesen Prüfpunkten erkannt. Ein Abbruch beim Inhaltsaufbau beendet die Sitzung in SERIALIZING ohne Export und ohne neue Teilberechnung. Die normale Replayprüfung und ihr Bytebudget bleiben kombinierbar.

Große Einzelzeilen, das Kopieren der Eltern-Evidenz bei Fortsetzungen sowie das Zusammenfügen des fertigen Bundles bleiben ununterbrechbar. Es gibt keine garantierte maximale Wartezeit. Für andere Auftragstypen bleibt `digest-checkpoints` verfügbar; `grid-record-checkpoints` wird dort ausdrücklich abgelehnt.

## Weitere Roadmap

Der Inhaltsaufbau für Grid-Stapel und Fortsetzungen besitzt jetzt eigene Abbruchpunkte. Weitere Ergebnistypen, blockweises Kopieren großer Textteile, Erstlaufunterbrechung und gemeinsame Operationsbudgets bleiben offen. Dies ist kein vollständiger Serialisierungs- oder Architekturabschluss.

[Vertrag](124-grid-serialization-records-contract.md) · [Abnahme](acceptance/grid-serialization-records-v1-2026-09-23.md)
