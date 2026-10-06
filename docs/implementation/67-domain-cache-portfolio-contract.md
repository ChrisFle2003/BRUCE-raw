# Bereichscache im Portfolio und Erfahrungskontext – Core 0.32

## Umfang

Die freigegebene Fortsetzung erlaubt `--domain-cache equal-64` für `portfolio-expression` mit `--equivalence rational-q-modular`. Alle drei Ausdrucksplaner sowie feste, adaptive und erfahrungsbasierte Reihenfolge sind unterstützt. Der eigenständige V9-Ausdruckspfad bleibt unverändert. Standard bleibt `none`.

Abnahmeziel: vollständige Forschung und begrenzte Teilbefunde mit Originalerhalt, gemeinsame Kosten-/Spurgrenzen, reproduzierbarer Export, geeignete Erfahrungsquellen, Widerruf und Z-Auswahl. Der Cache bleibt lauflokaler Zwischenzustand, keine gespeicherte Aufgaben-/Antwortdatenbank.

## Zwei getrennte Caches

Das Ausdruckskind verwendet seinen bestehenden [Core-0.31-Cache](65-domain-cache-contract.md). Die anschließende Vereinigung besitzt eine eigene Cacheinstanz, die über die fünf Familien hinweg erhalten bleibt. Ihr Startzustand ist leer; sie wird nicht aus dem Ausdruckskind vorgewärmt. Die ersten vier Suchverfahren erhalten `domain_cache=none` und bleiben mathematisch unverändert.

Beide Caches verwenden `equal-64-key4096-v1`: je 64 feste Plätze, vollständige geordnete Schlüsselprüfung, höchstens 4096 Schlüsselbytes pro Eintrag und Speicherung ausschließlich vollständig bewiesener Bereichsgleichheit. UNKNOWN und DIFFERENT bleiben ungecached. Ein Cachetreffer ersetzt nur den Bereichsvergleich; reduzierte Bruchwerte werden davor weiterhin geprüft. Die feste nachgelagerte Vereinigungsreihenfolge und Vertreterwahl bleiben erhalten.

Die getrennten Instanzen halten die Cacheentscheidungen der Vereinigung unabhängig von der Suchreihenfolge. Bei identischen vollständig berechneten Teilbefunden entstehen dieselben Gruppen. Bei Budgetabbruch darf sich der verfügbare Teilbefund unterscheiden.

## Budgetierung und Statistik

Es gilt unverändert:

`operations_used = ordering_operations + Summe(Teilrun-Operationen) + union_operations`.

Cacheverwaltung in der Vereinigung belastet vor jedem Schritt das gemeinsame Operationsbudget und ist zugleich Teil von `union_operations` und `domain_comparison_operations`. Ausdrucksverwaltung ist bereits im Teilrun enthalten. Die Cachezähler aggregieren Statistik und buchen keine zusätzlichen Operationen.

Die Felder `domain_cache_hits`, `domain_cache_misses`, `domain_cache_bypasses`, `domain_cache_inserts`, `domain_cache_evictions`, `domain_cache_entries`, `domain_cache_key_bytes` und `domain_cache_operations` summieren Ausdruckskind und Vereinigung. Zusätzlich weisen `union_domain_cache_hits` und `union_domain_cache_operations` deren Vereinigungsanteil aus. Auch unvollständige Vergleiche behalten ihre bereits verbrauchte Verwaltungsarbeit.

`domain_cache_operations` ist Teil von `domain_comparison_operations`, und der Vereinigungsanteil ist zugleich Teil von `union_operations`. Diese Werte nicht addieren. `domain_modular_certificates` zählt wie bisher neu berechnete modulare Seitenzertifikate, keine Cachetreffer.

Einträge und Schlüsselbytes auf Portfolioebene sind die Summe der **abschließenden Speicherstände beider Phasen**: höchstens 128 Einträge und 524288 Schlüsselbytes. Sie sind weder die Belegung eines gemeinsamen 128er-Caches noch ein gemessener RAM-Spitzenwert. Das Ausdruckskind gibt seinen eigentlichen Cache nach der Suche frei; das Portfolio behält davon die Statistik.

Die bisherigen 128 Originaltreffer, Spur-/Bytegrenzen, Quellenherkünfte, Teilbefundmarkierungen und Abbruch-Checkpoints gelten weiterhin. Eine während der Cacheprüfung unterbrochene Vereinigung behält den neuen Ursprung separat als INCOMPLETE. Nachgelagerte Vereinigungsereignisse werden weiterhin vor der Teilrun-Budgetvergabe reserviert.

## Export und Auswahl

| Reihenfolge | Export | Engine | Planer |
| --- | --- | --- | --- |
| Fest | `BRUCE_PORTFOLIO_BUNDLE_V29` | `bruce-portfolio-v29` | `five-family-q-cache-v1` |
| Adaptiv | `BRUCE_PORTFOLIO_BUNDLE_V30` | `bruce-portfolio-v30` | `sample-shape-v10` |
| Erfahrung | `BRUCE_PORTFOLIO_BUNDLE_V31` | `bruce-portfolio-v31` | `experience-order-v10` |

Ausdruckskind: V9. Metadaten ergänzen `domain_cache_policy equal-64-key4096-v1` und `domain_cache_scope expression-and-union-separate-v1`. Die bestehenden Gleichheits-, modularen Vergleichs-, Operator-, Teilbaumcache-, Potenz- und Budgetpolitiken bleiben explizit gebunden. Replay baut beide Caches neu auf und vergleicht vollständigen Inhalt einschließlich Statistik, Kosten und Teilbefunden. Es gibt keine externe Cachedatei.

Auswahl erkennt eigene Herkunft `portfolio-q-cached-v29/30/31-…`. Unvollständige Forschung, Polstellen, nichtganzzahlige Werte und Int64-Überlauf bleiben gesperrt. Eingebettete Quellen ermöglichen Replay ohne Originaldateien. Bestehende Exporte werden nicht überschrieben.

## Experience V10

Nur unabhängige vollständige V29-/V30-Läufe mit Hold-out dürfen neue Quellen sein. V31-Läufe dürfen keine weiteren Erfahrungsstimmen erzeugen. Speicherformat `BRUCE_EXPERIENCE_V10`, Engine `bruce-experience-v10`, Politik `routing-experience-v10`; beim Lesen wird die tatsächliche zweistellige Headerlänge berücksichtigt.

Kontext `routing-context-v10-five-q-cache` bindet die Cachepolitik und den getrennten Geltungsbereich zusätzlich zu den bisherigen Regeln, Zahlen-/Suchgrenzen, Operatoren, Planern und Aufgabenformen. Quellen ohne Bereichscache beeinflussen diesen Kontext nicht und umgekehrt. Unterschiedliche Ausdrucksplaner bleiben getrennt.

Gemischte historische Speicher behalten Ereignisse, Kennungen und Widerrufe. Quellenprüfung, acht Quellen, 16 Ereignisse, 1 MiB, höchstens eine Million deklarierte Suchoperationen pro Quelle, Ausschluss eigener Lernpaare und globale Duplikatprüfung bleiben unverändert. Widerruf entfernt künftigen Planungseinfluss; frühere Exporte behalten ihren eingebetteten damaligen Speicherstand.

## Abnahme und Grenzen

Tests prüfen reale Treffer der Familienvereinigung, alle Ausdrucksplaner/Reihenfolgen und Operatorauswahlen, kubische Gruppierung mit Originalerhalt, jeden Budget-/Checkpointabbruch kleiner vollständiger Aufgaben, Abbrüche während eines bereits verfügbaren Cachetreffers, exakte Statistikaggregation, V10-Kontexttrennung, gemischte Speicher, Widerruf, Auswahl und Manipulationen.

Cacheverwaltung kann Arbeit sparen oder zusätzliche Arbeit verursachen. Referenzmessungen müssen deshalb beide Fälle zeigen. Keine automatische Behauptung höherer Laufzeitgeschwindigkeit, kein allgemeines Lernen und kein Beweis einer unbekannten Datenbeziehung.

Weitere Suchoptimierungen, vollständigere Nullstellenentscheidungen, externe Evidenz und weitergehende Experience-Korrekturketten bleiben offen; ebenso Sprache, NanoCell, Cockpit, Phase-DAG, GPU und BigInt im Grid/Z. Kein vollständiger M2-/M4-/Gesamtprojektabschluss.
