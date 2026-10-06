# Bereichsbeweise im Portfolio wiederverwenden – Core 0.32

Der optionale Bereichscache funktioniert jetzt auch im Fünferportfolio und mit Erfahrungsplanung. Die Ausdruckssuche und die anschließende Familienvereinigung verwenden jeweils einen eigenen begrenzten Cache. Bewiesene Bereichsgleichheiten können wiederverwendet werden; alle Originaltreffer und echte Definitionslücken bleiben erhalten.

## Vollständiges kleines Beispiel

```bash
cmake --build build
./build/bruce research --strategy portfolio-expression \
  --equivalence rational-q-modular --domain-cache equal-64 \
  --expression-planner cached-grammar --operators '+/' \
  --min 0 --max 1 --max-power 1 --max-degree 1 --max-nodes 3 \
  --train examples/q-domain-zero-training.csv \
  --holdout examples/q-domain-zero-holdout.csv \
  --output portfolio-cache.bruce
./build/bruce replay portfolio-cache.bruce
```

Dieser Lauf ist vollständig und liefert `VERIFIED_ON_SAMPLES`. Alle sieben Originaltreffer bleiben in einer gemeinsamen Formelgruppe erhalten. Es entstehen zwei Cachetreffer, einer davon während der Familienvereinigung.

**Hier kostet der Cache etwas mehr:** 3620 statt 3564 logische Einheiten. Die Verwaltung überwiegt bei dieser kleinen Aufgabe die eingesparte Prüfarbeit. `--domain-cache none` bleibt deshalb der Standard.

Im größeren kubischen Beispiel aus [Core 0.30](64-modular-research-usage.md), als Portfolio mit `cached-grammar` und aktivem Bereichscache, sinkt die Arbeit dagegen von **463660 auf 462532** Einheiten. Das sind 1128 weniger, bei identischen fünf Gruppen und 128 Originaltreffern. Alle 117 Cachetreffer entstehen dort im Ausdruckskind; die Vereinigung findet keinen wiederverwendbaren Eintrag. Dieser Lauf bleibt wegen der Originalgrenze `INCOMPLETE / ChildIncomplete`.

Das sind Messungen konkreter Suchräume, kein allgemeines Geschwindigkeitsversprechen.

## Erfahrung aus einem vollständigen Lauf

```bash
./build/bruce research --strategy portfolio-expression \
  --equivalence rational-q-modular --domain-cache equal-64 \
  --expression-planner cached-grammar --planner adaptive-order \
  --min 1 --max 1 --max-power 2 --max-degree 2 --max-nodes 7 \
  --train examples/expression-training.csv \
  --holdout examples/expression-holdout.csv \
  --output cache-quelle.bruce

./build/bruce experience add --from-run cache-quelle.bruce \
  --output cache-erfahrung.experience

./build/bruce research --strategy portfolio-expression \
  --equivalence rational-q-modular --domain-cache equal-64 \
  --expression-planner cached-grammar --planner experience-order \
  --experience cache-erfahrung.experience \
  --min 1 --max 1 --max-power 2 --max-degree 2 --max-nodes 7 \
  --train examples/expression-experience-target-training.csv \
  --holdout examples/expression-experience-target-holdout.csv \
  --output cache-ziel.bruce
./build/bruce replay cache-ziel.bruce
```

Die vollständige Quelle vergibt `[0,0,0,0,1]` Stimmen. Die Ausdruckssuche läuft zuerst; alle fünf Familien bleiben enthalten und liefern dieselben vollständigen Gruppen wie der adaptive Referenzlauf. Die Quellenprüfung wird separat als `experience_audit_operations` ausgewiesen.

Stimmen wirken nur im passenden Kontext. Quellen ohne Bereichscache oder mit anderem Ausdrucksplaner beeinflussen diesen Cachekontext nicht. Alte und neue Quellen können trotzdem gemeinsam gespeichert werden. Eigene Lernpaare und doppelte Lernquellen bleiben ausgeschlossen; ein erfahrungsbasierter Lauf darf keine neue Stimmenquelle werden.

```bash
./build/bruce experience inspect cache-erfahrung.experience
./build/bruce experience revoke --store cache-erfahrung.experience \
  --id QUELLEN_ID_AUS_INSPECT --reason "Quelle widerrufen" \
  --output cache-widerrufen.experience
```

Ein Widerruf entfernt den Einfluss bei späteren Läufen. Vorhandene Forschungsexporte behalten ihren damaligen eingebetteten Speicherstand und bleiben replaybar.

## Auswahl und Zähler

```bash
./build/bruce compatibility --rules examples/quadratic-fraction-selection.rules \
  --from-run cache-ziel.bruce --at 0 --output cache-auswahl.bruce
./build/bruce replay cache-auswahl.bruce
```

Die Beispielauswahl liefert Wert 0; die Polstelle −1 bleibt gesperrt. Auswahl-Replay benötigt die ursprüngliche Forschungsdatei nicht mehr. Unvollständige Forschung darf nicht als abgeschlossene Auswahl dienen.

- `domain_cache_hits` zählt Treffer aus beiden Phasen; `union_domain_cache_hits` zeigt den darin enthaltenen Vereinigungsanteil.
- `domain_cache_operations` ist bereits in der Gesamtarbeit und den Bereichsvergleichskosten enthalten. `union_domain_cache_operations` ist zusätzlich in `union_operations` enthalten. Nicht addieren.
- `domain_cache_entries` und `domain_cache_key_bytes` summieren die abschließenden Speicherstände beider Phasen. Jede Phase hat höchstens 64 Einträge; diese Statistik ist keine Messung des gleichzeitig belegten RAM.

Alle drei Ausdrucksplaner und alle drei Portfolio-Reihenfolgen sind unterstützt. Cacheläufe schreiben V29/V30/V31, gemischte neue Erfahrungsspeicher V10. Frühere Versionen bleiben unverändert; bestehende Dateien werden nicht überschrieben.

## Weitere Roadmap

Der Bereichscache ist jetzt in Portfolio und Erfahrungsplanung integriert. Weitere Suchoptimierungen, vollständigere Nullstellenverfahren, externe Evidenz und weitergehende Experience-Korrekturketten bleiben offen. Die Statusaussage bestätigt weiterhin Sampleprüfungen, keine allgemeine Datenbeziehung. Kein vollständiger M2-/M4-/Gesamtprojektabschluss.

[Vertrag](67-domain-cache-portfolio-contract.md) · [Abnahme](acceptance/domain-cache-portfolio-v1-2026-09-22.md)
