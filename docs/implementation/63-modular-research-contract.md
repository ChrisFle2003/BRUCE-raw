# Modulare Q-Gruppen in Forschung und Erfahrung – Core 0.30

## Freigegebener Umfang

Die Fortsetzung integriert den [modularen Bereichsvergleich](61-modular-comparison-contract.md) in die eigenständige Ausdruckssuche, die Vereinigung der fünf Formelfamilien und die Erfahrungsplanung. Der neue explizite Modus heißt `--equivalence rational-q-modular`. `syntax`, `rational` und `rational-q` behalten ihre bisherigen Regeln und Exportversionen.

Akzeptanz: Datenpaare → begrenzte Suche → präzisere Q-Gruppen mit allen Originaltreffern → versionierter Export/Replay → Auswahl. Unabhängige vollständige Quellen mit Hold-out dürfen im passenden Erfahrungskontext die spätere Suchreihenfolge beeinflussen. Auch unvollständige Suchläufe müssen vollständig reproduzierbare Teilbefunde liefern.

## Gruppierung und Mathematik

Alle drei Ausdrucksplaner (`grammar`, `indexed-grammar`, `cached-grammar`) verwenden ihre bisherige Grammatik, Operatorauswahl, literalen Potenzen, Stichprobenprüfung und unabhängige Gegenrechnung. Für Gruppen gelten reduzierte Bruchwerte und ursprüngliche Nennerausschlüsse. Der neue Modus verwendet bei deren Vergleich `rational-domain-gcd-modular-v1` aus Core 0.29: gemeinsame Faktoren entfernen, Restgrade bis 2 exakt entscheiden, bei höheren Graden die elf festen Primzahlen bis 31 versuchen.

Nur nachgewiesene Gleichheit über Q führt zur Zusammenführung mit dem ersten passenden bisherigen Repräsentanten. `UNKNOWN` bleibt getrennt und beweist keine Ungleichheit. Ursprüngliche Ausdrucksbäume bleiben in `expression_origins`, Familienherkünfte in `origins` und den Teilruns erhalten. Originale Definitionslücken werden nicht durch algebraisches Kürzen entfernt.

Im Portfolio bleiben die ersten vier Suchverfahren unverändert. Das Ausdruckskind verwendet den neuen Modus. Die Vereinigung erfolgt nach der Suche in fester Familienreihenfolge, unabhängig von der Suchreihenfolge. Bei identischen vollständig berechneten Quellen bleiben dadurch Gruppen und Vertreter stabil. Abbrüche können je nach Suchreihenfolge verschiedene Teilbefunde liefern.

Die stärkere Gruppierung beweist Gleichheit zwischen vorgegebenen gefundenen Formeln über Q. Der Forschungsstatus bleibt eine Aussage über geprüfte Datenpaare; `general_proof`, `learn_gate` und `publish_gate` bleiben false.

## Kosten, Herkunft und Grenzen

Es gelten die Budgets und Teilbefundregeln aus [Core 0.27](57-q-expression-contract.md) und [Core 0.28](59-q-portfolio-contract.md): maximal 128 Originaltreffer, keine zusätzlichen Ursprungsplätze durch Zusammenführung, Abbruch statt stiller Kürzung, erhaltene Originale bei unterbrochener Gruppenprüfung. Noch nicht vereinigte Portfolioquellen bleiben in `strategy_runs`.

`operations_used = ordering_operations + Summe(Teilrun-Operationen) + union_operations`.

Modulare Prüfschritte belasten dieselbe globale Arbeitsgrenze wie die übrigen Q-Vergleiche. `domain_comparison_operations` umfasst Bereichsvergleiche aus Ausdruckskind und Vereinigung; der Vereinigungsanteil ist zugleich Teil von `union_operations`. Beide Werte dürfen nicht pauschal addiert werden. Normalisierungskosten bleiben getrennt.

Neu ist `domain_modular_certificates`: Anzahl erfolgreicher **Seitenzertifikate in abgeschlossenen Bereichsvergleichen**. Ein Vergleich kann 0, 1 oder 2 beitragen. Wiederholte Nachweise werden wiederholt gezählt. Ein einseitiger Nullstellenausschluss kann gezählt sein, obwohl die andere Seite offen bleibt und keine Zusammenführung erfolgt. Der Portfoliozähler summiert Ausdruckskind und Familienvereinigung. Ein abgebrochener Bereichsvergleich liefert keinen Zählerbeitrag, auch wenn intern eine Seite bereits berechnet war.

Die Forschungsdatei speichert keine unbeschränkte Liste von Primzahlwerttabellen. Sie bindet Vergleichspolitik, Originale, Repräsentanten, Spur, Zähler und Kosten; Replay berechnet die Entscheidungen erneut. Für ein sichtbares Einzelzertifikat mit Primzahl und sämtlichen Restwerten steht `compare --domain-policy modular` zur Verfügung.

Die bisherigen Abbruch-Checkpoints zwischen Kandidaten bzw. Ursprüngen bleiben erhalten. Innerhalb der Primzahlprüfung greift vor jedem kostenpflichtigen Schritt die Operationsgrenze. Dies ist keine neue asynchrone Unterbrechung an jeder Restklassenoperation.

## Versionen und Provenienz

| Lauf | Export | Engine | Planerversion |
| --- | --- | --- | --- |
| Eigenständige Ausdruckssuche | `BRUCE_EXPRESSION_BUNDLE_V8` | `bruce-expression-v8` | bisherige Grammatikplaner |
| Festes Portfolio | `BRUCE_PORTFOLIO_BUNDLE_V26` | `bruce-portfolio-v26` | `five-family-q-modular-v1` |
| Adaptives Portfolio | `BRUCE_PORTFOLIO_BUNDLE_V27` | `bruce-portfolio-v27` | `sample-shape-v9` |
| Erfahrungsportfolio | `BRUCE_PORTFOLIO_BUNDLE_V28` | `bruce-portfolio-v28` | `experience-order-v9` |
| Erfahrungsspeicher | `BRUCE_EXPERIENCE_V9` | `bruce-experience-v9` | `routing-experience-v9` |

Gleichheitspolitik: `rational-q-modular-domain-v1`; Vergleichspolitik: `rational-domain-gcd-modular-v1`. Die Ausdruckspolicy heißt `defined-expression-samples-v8`; Zusammenführungsregeln `q-domain-binary-v1`/`q-domain-union-v1` und Portfolio-Budgetpolicy `shared-budget-v3` bleiben anwendbar. Header, Engine, Planer, Operatoren, Cache-, Potenz- und Vergleichspolitik werden strikt geprüft. Auch bei neuem Dateihash muss das vollständige Ergebnis mit dem Replay übereinstimmen.

Z-Auswahl verwendet eigene Provenienzkennungen `expression-q-modular-…` bzw. `portfolio-q-modular-v26/27/28-…`. Polstellen, nichtganzzahlige Werte, Int64-Überlauf und unvollständige Forschungsbefunde bleiben gesperrt. Eingebettete Quellen ermöglichen Replay ohne ursprüngliche Dateien.

## Erfahrungsplanung

Nur unabhängige V26-/V27-Läufe sind neue Quellen; V28-Läufe dürfen keine weiteren Stimmenquellen werden. Kontext `routing-context-v9-five-q-modular` bindet die neue Vergleichspolitik, Vereinigungs-/Budget-/Potenzregel, Ausdrucksplaner, Cache, Operatoren, Zahlenbereich, Suchgrenzen, Gleichheitsmodus und Aufgabenform. `rational-q` und `rational-q-modular` beeinflussen einander nicht.

Gemischte Speicher bewahren alte Ereignisse und IDs. Quellen werden vollständig replaygeprüft. Die bestehenden Grenzen gelten weiter: acht Quellen, 16 Ereignisse, 1 MiB, höchstens eine Million deklarierte Suchoperationen pro Quelle. Selbstbeeinflussung und doppelte Lernpaare bleiben ausgeschlossen; Widerruf entfernt den Planungseinfluss. Historische Läufe behalten ihren eingebetteten damaligen Speicherstand.

## Prüfung und nächste Schritte

Abnahme prüft tatsächliche kubische Zusammenführung in allen Planern und im Portfolio, Originalerhalt bei der 128er-Grenze, höhergradige Familienvereinigung an jeder Operationsgrenze, gemeinsame Kosten und Spurgrenzen, alle bisherigen Checkpoint-/Kandidatenabbrüche in kleinen vollständigen Aufgaben, alle 31 Operatorauswahlen, V9-Kontexttrennung/Widerruf, Manipulationen und historische Replay-Dateien.

Allgemeinere Nullstellenentscheidung, effizientere Gruppenvergleiche, zusätzliche Operatoren, externe Evidenz und weitergehende Experience-Korrekturketten bleiben offen; ebenso Sprache, NanoCell, Cockpit, Phase-DAG, GPU und BigInt im Grid/Z. Kein vollständiger M2-/M4-/Gesamtprojektabschluss.
