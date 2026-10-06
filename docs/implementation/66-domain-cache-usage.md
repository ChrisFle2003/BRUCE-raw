# Bewiesene Bereichsgleichheiten wiederverwenden – Core 0.31

Bruce kann bereits abgeschlossene Bereichsbeweise innerhalb einer Ausdruckssuche wiederverwenden. Der optionale Cache behält höchstens 64 Einträge. Er speichert ausschließlich bewiesene Gleichheit; offene Fälle und echte Definitionslücken bleiben erhalten.

## Gemessener Vergleich

```bash
cmake --build build
./build/bruce research --strategy expression \
  --planner cached-grammar --equivalence rational-q-modular \
  --domain-cache equal-64 --operators '-^/' \
  --min 3 --max 3 --max-nodes 9 --max-operations 10000000 \
  --train examples/q-domain-zero-training.csv \
  --holdout examples/q-domain-zero-holdout.csv \
  --output bereichscache.bruce
./build/bruce replay bereichscache.bruce
```

Mit sonst gleichen Einstellungen und `--domain-cache none` benötigt das Beispiel 461889 logische Einheiten. Mit Cache sind es **460519**: 1370 weniger bei denselben fünf Formelgruppen und allen 128 Originaltreffern. 117 Cachetreffer verwenden frühere Bereichsbeweise erneut. Die 3957 Einheiten für Cacheverwaltung sind bereits im Gesamtwert enthalten.

Der Cache hält hier sechs Schlüssel mit zusammen 85 Bytes Inhalt. Das ist kein Gesamt-RAM-Messwert. Im Allgemeinen gelten höchstens 64 Schlüssel mit jeweils 4096 Bytes; größere Schlüssel werden regulär ohne Cache geprüft.

**Die Suche bleibt `INCOMPLETE / ResultBudgetExceeded`.** Sie erreicht weiterhin die Grenze von 128 Originaltreffern. Wiederverwendung macht aus diesem Teilbefund keine abgeschlossene Forschung. Die gemessene Einsparung gilt für dieses Beispiel; andere Aufgaben können durch die Cacheverwaltung auch mehr Arbeit benötigen.

`cached-grammar` speichert kleine Ausdrucksteilbäume. `--domain-cache equal-64` speichert dagegen bewiesene Bereichsgleichheiten. Beide sind getrennte Optionen; der Bereichscache funktioniert auch mit `grammar` und `indexed-grammar`.

## Vollständige Forschung und Auswahl

```bash
./build/bruce research --strategy expression \
  --planner cached-grammar --equivalence rational-q-modular \
  --domain-cache equal-64 --min 1 --max 1 --max-nodes 7 \
  --train examples/expression-training.csv \
  --holdout examples/expression-holdout.csv \
  --output cache-vollstaendig.bruce
./build/bruce replay cache-vollstaendig.bruce

./build/bruce compatibility --rules examples/quadratic-fraction-selection.rules \
  --from-run cache-vollstaendig.bruce --at 0 --output cache-auswahl.bruce
./build/bruce replay cache-auswahl.bruce
```

Dieser Suchlauf schließt mit `VERIFIED_ON_SAMPLES` ab und liefert dieselben Gruppen wie ohne Bereichscache. Die Auswahl bei 0 ergibt Wert 0. Die Polstelle −1 bleibt gesperrt. Eine durch Hold-out widerlegte Formel wird durch den Cache nicht bestätigt.

`VERIFIED_ON_SAMPLES` bestätigt weiterhin nur die geprüften Datenpaare. Die Gleichheit konkreter Formeln über Q ist kein allgemeiner Beweis einer unbekannten Datenbeziehung.

## Zähler und Grenzen

`domain_cache_hits` zählt wiederverwendete Beweise; `domain_modular_certificates` zählt neu berechnete modulare Seitenzertifikate. Letzterer Wert darf bei gleichen Ergebnissen sinken, weil Cachetreffer keine neuen Beweise vortäuschen.

`domain_cache_operations` ist bereits in `domain_comparison_operations` und in der Gesamtarbeit enthalten. Für Details zu Fehltreffern, Ersetzungen und Speicherinhalt siehe den Vertrag.

Standard bleibt `none`. `equal-64` ist vorerst nur für die eigenständige Ausdruckssuche im Modus `rational-q-modular` verfügbar. Aktivierte Läufe schreiben V9; Replay baut den Cache aus den Eingaben neu auf. Frühere Exportversionen bleiben unverändert. Vorhandene Dateien werden nicht überschrieben.

## Weitere Roadmap

Die begrenzte Wiederverwendung ist in der eigenständigen Ausdruckssuche implementiert. Portfolio-/Experience-Einbindung, weitere Suchoptimierungen, vollständigere Nullstellenentscheidungen, externe Evidenz und weitergehende Korrekturketten bleiben offen. Kein vollständiger M2-/Gesamtprojektabschluss.

[Vertrag](65-domain-cache-contract.md) · [Abnahme](acceptance/domain-cache-v1-2026-09-22.md)
