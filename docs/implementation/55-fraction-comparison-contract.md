# Rationale Definitionsbereiche vergleichen – Core 0.26

## Zweck und Umfang

Die freigegebene Fortsetzung ergänzt die vorhandenen rationalen Normalformen um einen eigenständigen, begrenzten Vergleich zweier expliziter Polynombrüche. `compare` entscheidet Gleichheit **als partielle Funktionen über Q**, einschließlich der Nullstellen der ursprünglichen Nenner. Es verändert weder die bisherige Ausdruckssuche noch Portfolio-Gruppen oder Experience-Kontexte. Deren konservative Gleichheitsregel und historische Replay-Versionen bleiben bestehen.

Eingaben sind vier Koeffizientenlisten in aufsteigender Potenzreihenfolge: `a0,a1,...,an`. Jeder Koeffizient ist eine exakte rationale Zahl. Pro Liste sind 1 bis 65 Einträge erlaubt (Grad höchstens 64, auch überzählige Nullkoeffizienten zählen zur Eingabegrenze). Die Nullpolynom-Nenner beschreiben einen leeren Definitionsbereich. Beide leeren partiellen Funktionen sind gleich; genau ein leerer Definitionsbereich bedeutet Ungleichheit.

## Mathematisches Verfahren

1. Beide Brüche werden mit dem bestehenden Q[x]-Verfahren gekürzt und auf einen monischen Nenner gebracht. Die monischen, quadratfreien Ausschlusspolynome der **ursprünglichen** Nenner bleiben erhalten.
2. Die Definitionsbereiche werden separat verglichen. Nach Behandlung leerer Bereiche wird der Polynom-GGT der Ausschlusspolynome entfernt. Die beiden verbleibenden Polynome sind teilerfremd; auch zum entfernten gemeinsamen Faktor bestehen wegen der Quadratfreiheit keine gemeinsamen Nullstellen.
3. Ein nichtnullkonstanter Rest besitzt keine Nullstelle. Ein linearer Rest hat genau eine rationale Nullstelle. Bei quadratischen Resten entscheidet die Diskriminante: Sie muss nichtnegativ sein, und ihr gekürzter Zähler und Nenner müssen jeweils perfekte ganzzahlige Quadrate sein. Dies entscheidet rationale, nicht reelle Nullstellen. Höhergradige Reste bleiben `UNKNOWN`.
4. Zwei nachweislich nullstellenfreie Reste ergeben gleiche Definitionsbereiche. Eine nachgewiesene rationale Nullstelle auf einer Seite beweist unterschiedliche Bereiche, auch wenn die andere Seite noch unentschieden ist. Eine solche Stelle wird in beide ursprünglichen, normalisierten Ausschlusspolynome eingesetzt: Genau eine Seite muss null ergeben. Das Ergebnis enthält die Stelle und die ausgeschlossene Seite.
5. Bei zwei nichtleeren Bereichen werden die gekürzten Brüche verglichen. Verschiedene rationale Funktionen können auf dem unendlichen gemeinsamen rationalen Definitionsbereich nicht überall gleich sein. Daher genügt ihre algebraische Verschiedenheit für Ungleichheit, auch bei unentschiedenem Bereichsvergleich.

| Gesamtstatus | Aussage |
| --- | --- |
| `PROVEN_EQUIVALENT` | Gleiche partielle Funktionen auf Q, einschließlich Definitionsbereich |
| `PROVEN_DIFFERENT` | Verschiedene Wertefunktionen oder unterschiedliche Definitionsbereiche |
| `UNDECIDED` | Gleiche gekürzte Wertefunktion, Bereichsvergleich noch offen |
| `INCOMPLETE` | Ressourcen- oder Abbruchgrenze erreicht; kein abgeschlossener Gesamtbefund |

`value_relation` und `domain_relation` sind getrennt. Bei leerem Bereich ist `value_relation=NOT_APPLICABLE`. Das Zertifikat zeigt Restpolynome, Nullstellenbefunde und gegebenenfalls eine gegenprüfte rationale Ausschlussstelle. Die vierstufige Spur enthält nur abgeschlossene Phasen. Ein unvollständiger Lauf kann bereits abgeschlossene Teilbefunde enthalten.

Der Beweisbereich ist ausschließlich `supplied-polynomial-fractions-over-Q`: Identität der eingegebenen Formeln, kein allgemeiner Beweis einer aus Daten vermuteten Formel. Insbesondere sind `(x²−2)/(x²−2)` und `1` auf Q gleich, auf R wegen der beiden irrationalen Ausschlussstellen verschieden. Es wird kein Vergleich über R angeboten.

## Grenzen, Kosten und Abbruch

- Bitgrenze: 1 bis 16384, Standard 4096. Sie begrenzt exakte Koeffizienten und mathematische Zwischenwerte. Sie definiert **keinen endlichen Eingabebereich** für den Gleichheitsbeweis.
- Arbeitsgrenze: 1 bis 1000000 logische Einheiten, Standard 1000000. GGT, Kürzung, Nullstellenprüfung, Gegenprüfung und Koeffizientenvergleich werden gezählt. Arithmetik, Vorzeichen-/Quadrattests und Wurzelbildung sind jeweils einzelne logische Einheiten; das ist keine Bitkomplexitäts- oder CPU-Zeitmessung. Das vorhandene Kostenmodell der Normalisierung bleibt unverändert.
- Vorprüfung: Die vier Listen werden vor der mathematischen Verarbeitung validiert und ihre Zahlen kanonisch gespeichert. Pro Liste gilt zusätzlich 1 MiB Textgrenze. Leere Listen, ungültige Zahlen, überlange Listen und bereits zu große Eingabekoeffizienten sind Eingabefehler (Exit 2). Validierung und JSON-/Exportdarstellung gehören nicht zum mathematischen Arbeitszähler.
- Eine Grenze bei Zwischenwerten führt zu `INCOMPLETE`, niemals zur Gleichheits- oder Ungleichheitsbehauptung. Vor jedem gezählten Schritt wird die Arbeitsgrenze geprüft.
- Ctrl+C/SIGTERM wird an diesen Grenzen abgefragt. Die tatsächlich erreichte Zahl logischer Operationen wird in der Aufgabe als Abbruchposition gespeichert. Replay reproduziert diese Position ohne Signal.
- Höchstens vier Spurereignisse, zwei Normalformen, ein Bereichszertifikat; keine unbeschränkte Such- oder Wurzeltabelle. GMP-Zwischenspeicher ist durch die bestehenden Bit-/Gradgrenzen beschränkt, nicht durch eine behauptete konstante Bytezahl.

## Export und Replay

| Feld | Version |
| --- | --- |
| Container | `BRUCE_FRACTION_COMPARISON_V1` |
| Engine | `bruce-fraction-comparison-v1` |
| Policy | `rational-domain-gcd-quadratic-v1` |

Der Export enthält kanonische Eingaben, Grenzen, optionale Abbruchposition, vollständiges JSON-Ergebnis und SHA-256. Replay berechnet die Aufgabe erneut und vergleicht die vollständige kanonische Nutzlast einschließlich Ergebnissen, Zertifikat, Kosten und Spur. Manipulationen werden auch bei neu berechnetem Hash erkannt, sofern sie nicht einen vollständig konsistenten anderen Lauf bilden. Der Hash ist keine externe Signatur.

Bestehende Exportziele werden nicht überschrieben. Replay benötigt keine externen Eingabedateien. Vergleichsexporte sind weder Forschungsquellen für Experience noch Kandidatenquellen für Z-Auswahl.

## Abnahme und nächste Grenzen

[Benutzung](56-fraction-comparison-usage.md) · [Abnahme](acceptance/fraction-comparison-v1-2026-09-22.md)

Die Einbindung dieser genaueren Bereichsentscheidung in Ausdrucksgruppen, Portfolio und Erfahrung bleibt ein eigenes Paket mit eigenen Versions- und Budgetregeln. Allgemeine rationale Nullstellen höherer Restgrade, Vergleiche über R, freiere Grammatiken und weitere Roadmap-Pakete bleiben offen. Kein vollständiger M2-/Gesamtprojektabschluss.
