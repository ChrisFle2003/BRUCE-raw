# Begrenzter Teilbaumcache – Core 0.24

Freigegebene Fortsetzung von Core 0.23: Die eigenständige Ausdruckssuche erhält den optionalen Planer `cached-grammar`. Er erweitert den bestehenden Grammatikindex um einen deterministischen, auf 64 Einträge begrenzten Zwischenspeicher für Teilbäume. Der deklarierte Suchraum, seine Rangfolge und die mathematischen Prüfungen bleiben unverändert.

## Speicher- und Kostenvertrag

Die Policy `direct-map-64-size3-v1` speichert ausschließlich **echte Teilbäume mit genau drei Knoten**. Ganze Kandidaten, einzelne Terminals und größere Teilbäume werden nicht gespeichert. Dadurch belegen selten wiederkehrende größere Strukturen keine Cacheplätze. Der Planer unterstützt alle bisherigen Operatorauswahlen, einschließlich literaler Potenzen, und beide Gleichheitsmodi.

Der Schlüssel besteht aus Teilbaumgröße und größenlokalem Grammatikrang. Innerhalb einer Grammatikinstanz sind Konstantenvorrat und Operatoren unveränderlich. Der Slot ist `(rank*5+size)%64`; der vollständige Schlüssel wird vor Verwendung verglichen. Eine Kollision ersetzt den bisherigen Eintrag. Es gibt keine versteckte globale Tabelle, keine Stichprobenwerte und keine gespeicherten Forschungsergebnisse.

Jeder Cacheeintrag enthält eine eigenständige Postorder-Kopie. Beim Speichern werden Kindindizes relativ zum Teilbaumanfang verschoben; beim Einfügen in einen neuen Baum werden sie auf dessen Position umgesetzt. Für einen Dreiknotenbaum sind dies ein binärer Wurzelknoten und zwei Terminals. Der Cache enthält höchstens 64 Bäume beziehungsweise 192 Baumknoten, zusätzlich zur begrenzten Verwaltungsstruktur. Die Literale unterliegen den bestehenden Konstanten- und Bitgrenzen. Diese Knotengrenze ist keine pauschale Byteangabe für GMP oder Speicherverwaltung.

Logische Arbeit wird vollständig dem bestehenden Planungsbudget zugerechnet:

| Aktion | Zusätzliche logische Einheiten |
| --- | ---: |
| Slotzugriff samt Schlüsselvergleich | 1 |
| Treffer: Kopie und Indexumsetzung | 3 |
| Fehlzugriff: Aufbau | bisherige indizierte Aufbaukosten |
| Einfügen: Kopie und Verwaltung | 4 |
| Verdrängung eines belegten Slots | 1 |

Kopier- und Einfügekosten werden vor der jeweiligen Mutation angefordert. Ein Abbruch kann bereits vollständig erzeugte frühere Einträge erhalten, aber keinen halbfertigen Cacheeintrag. Jeder neue Forschungs- oder Replaylauf beginnt mit leerem Cache. Die Grammatikinstanz ist für die bestehende sequenzielle Ausführung vorgesehen; der Cachezustand hängt von der bisherigen Zugriffsspur ab.

`cache_hits` zählt erfolgreich kopierte Treffer, `cache_misses` erfolgte Zugriffe ohne passenden Schlüssel, `cache_evictions` abgeschlossene Ersetzungen und `cache_entries` die belegten Slots. Bei Abbruch können Fehlzugriffe ohne anschließendes Einfügen vorliegen. Kandidaten bleiben einzeln geprüft: Hauptrechnung, unabhängige Gegenrechnung, Hold-out und optionale Normalisierung nutzen unverändert ihre bisherigen Wege.

## Schnittstelle und Versionsvertrag

Die CLI verwendet `research --strategy expression --planner cached-grammar`. Der Standardplaner bleibt unverändert. Für das Fünferportfolio und seine Experience-Kontexte ist dieser neue Planer noch nicht freigeschaltet. Die direkte Grammatik-API verlangt bei aktiviertem Cache zugleich den Index.

Neue Exporte verwenden ausschließlich:

| Feld | Wert |
| --- | --- |
| Container | `BRUCE_EXPRESSION_BUNDLE_V6` |
| Engine | `bruce-expression-v6` |
| Regel | `cached-binary-v1` |
| Prüfpolicy | `defined-expression-samples-v6` |
| Planer | `subtree-cache-v1` |
| Cachepolicy | `direct-map-64-size3-v1` |
| Exponentenpolicy | `literal-2-to-8-v1` |

Operatorauswahl und Gleichheitsmodus sind explizit gespeichert. Die Exponentenpolicy ist auch bei einer Auswahl ohne `^` festgelegt; sie fügt der Auswahl keine Potenzen hinzu. Replay rekonstruiert den Cache aus dem leeren Anfangszustand und vergleicht den gesamten Lauf einschließlich Kosten, Zählern, Abbruchpositionen, Befunden und Trace. Der Cacheinhalt wird nicht als extern benötigte Datei gespeichert. Unbekannte Regeln oder Policies, nichtkanonische Operatoren und manipulierte Ergebnisse werden zurückgewiesen.

Historische Ausdrucks- und Portfolioformate bleiben unverändert. V6 bezeichnet hier das Ausdrucksformat und ist unabhängig vom Experience-V6-Format. Z-Import und Auswahl-Replay akzeptieren Ausdrucks-V6. Die Herkunftskennung enthält `expression-cached-`, Operator-Bitmaske, gegebenenfalls Normalisierungsmerkmal und Zahlendomäne. Definitionslücken, Int64-Projektionsgrenzen und die Sperre unvollständiger Forschungsergebnisse bleiben erhalten.

## Abnahme und Aussagegrenze

[Benutzung](52-cached-expression-usage.md) und [Abnahmebericht](acceptance/cached-expression-v1-2026-09-22.md) dokumentieren die Messung und Tests. Der Referenzlauf untersucht unverändert 2802 Ausdrücke; die Planungsarbeit sinkt von 56843 auf 50406, die gesamte logische Arbeit von 93297 auf 86860 Einheiten.

Ein Cache kann bei wenigen Wiederholungen oder vielen Kollisionen zusätzlichen Aufwand verursachen. Der Beispielvorteil ist keine allgemeine CPU-Beschleunigung. Portfolio-/Experience-Einbindung, numerisches Zwischenspeichern, weitergehende Suchoptimierung und allgemeinere Synthese bleiben offen. Der vollständige M2-Abschluss wird nicht abgeleitet.
