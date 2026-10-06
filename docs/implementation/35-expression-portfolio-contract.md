# Fünferportfolio mit Ausdruckssynthese – Core 0.16

Freigegebene Fortsetzung der algebraischen Ausdruckszusammenführung: Die neue Strategie `portfolio-expression` erweitert den bestehenden Portfolio-Runner um die Ausdruckssuche. Alle fünf Familien werden innerhalb gemeinsamer Grenzen ausgeführt. Es entsteht kein zweiter Forschungs- oder Evidenzpfad.

## Umfang und Reihenfolge

Feste Reihenfolge: `power-affine` mit adaptiver Koeffizientenableitung, `polynomial` mit Interpolation, `rational-linear`, `rational-quadratic` und schließlich `expression` mit rationaler Normalisierung. Zahlenbereich rational, Portfolio-Planer `portfolio`, Gleichheitsmodus `rational`. Andere Planer und Syntaxmodus werden für dieses Paket zurückgewiesen.

Gemeinsame Zähler-/Nennergrenzen gelten für die begrenzten Familienparameter und die Ausdruckskonstanten. Polynominterpolation behält ihren bisherigen Vertrag: Ihre abgeleiteten Koeffizienten werden nicht auf diesen Parameterbereich beschränkt. Power- und Gradgrenzen bleiben 1–8 beziehungsweise 0–8; Ausdrucksgröße bleibt ungerade 1–9. Der gesamte Parameterbereich muss die engere Ausdrucksgrenze von höchstens 64 Konstanten-Rohslots erfüllen. CLI-Standard: Zähler 0–1, Nennergrenze 1, Potenzgrenze 4, Polynomgrad 3, sieben Baumknoten.

Die bisherige Suche nach drei oder vier Familien bleibt unverändert. Adaptive Reihenfolge und Erfahrung gelten weiterhin nur für deren eigene versionierte Familienmengen. V8-Quellen werden vom Experience Store ausdrücklich abgewiesen, bis ein passender Vertrag für fünf Familien implementiert ist.

## Gemeinsame Identität und Herkunft

Alle vollständig geprüften Lernkandidaten werden in eine gemeinsame rationale Normalform übersetzt. Potenz-/Polynomkandidaten erhalten Nenner 1 und keine Ausschlüsse. Bei den beiden Bruchfamilien bleiben sämtliche Nullstellen des ursprünglichen Nenners ausgeschlossen, auch wenn Faktoren gekürzt werden. Ausdrucksgruppen übernehmen ihre bereits geprüfte Normalform einschließlich innerer Definitionslücken.

Der Schlüssel enthält gekürzten Zähler, monischen Nenner, monisches quadratfreies Ausschlusspolynom und Kennzeichnung des leeren Definitionsbereichs. Damit können zum Beispiel `1/(x+1)` aus der Bruchstrategie und derselbe Ausdruck aus der Grammatik zusammengeführt werden. `1`, `x/x` und `(x+1)/(x+1)` bleiben wegen ihrer verschiedenen Ausschlüsse getrennt. Ein Faktor ohne rationale Nullstelle wird weiterhin nicht entfernt; unterschiedliche Schlüssel beweisen keine Funktionsungleichheit.

`origins` nennt Strategie, Quell-ID und ursprünglichen Status. Der zugehörige vollständige Befund steht unter `strategy_runs`. Bei Ausdrucksgruppen führt dieser Verweis weiter zu deren `expression_origins` mit sämtlichen ursprünglichen Bäumen. Mehrere Herkünfte aus denselben Daten erhöhen nicht die Zahl unabhängiger Belege.

Die erste Quelle bestimmt die Gruppen-ID `5*source_id+family_index`. Die feste Ausführungsreihenfolge macht dies reproduzierbar. Koordinaten werden bei `2^31` geteilt. Auch die maximale Grammatik-ID bleibt innerhalb des bestehenden UInt64-Adressvertrags. Unvollständige Quellen werden separat als Teilbefunde aufbewahrt und nicht als bestätigte Gleichheit zusammengeführt.

## Gemeinsame Budgets und Abbrüche

Der bestehende Runner reicht verbleibende Kandidaten-, Operations-, Ergebnis- und Tracebudgets an jede Familie weiter. Nach jedem Teilrun zählt die Übernahme und Normalisierung ebenfalls zum globalen Operationsbudget. Die neuen Normalformen werden auf allen gelieferten x-Werten mit den ursprünglichen Polynombrüchen verglichen, einschließlich widerlegter Prüfwerte und Polstellen. Die Ausdrucksstrategie führt diesen Zusatzvergleich bereits selbst durch.

`--max-results` zählt ursprüngliche gespeicherte Lernkandidaten: bei der Ausdrucksstrategie sämtliche Baumherkünfte, bei den übrigen Familien deren einzelne Hypothesen. Eine zusammengeführte Gruppe spart daher kein Ergebnisbudget. Unterbrochene Normalisierung lässt die originalen Quellen im Teilrun stehen; ein begonnener globaler Teilbefund wird `INCOMPLETE`.

Zählerbeziehungen:

- `operations_used` = Summe aller Teilrun-Operationen + `union_operations`.
- `normalization_operations` = Summe der Teilrun-Normalisierung + `union_operations`; dies ist eine Teilmenge von `operations_used`.
- `stored_source_hypotheses` zählt die gespeicherten Ursprünge vor familienübergreifender Zusammenführung.
- `checkpoints` umfasst Teilrun-Checkpoints und einen zusätzlichen Kontrollpunkt vor jeder Quellenübernahme.
- Traceereignisse und Tracebytes umfassen äußere Spur und sämtliche Teilspuren. Ein `UNION`-Eintrag pro begonnenem Teilrun beschreibt Gruppenzahl, kumulierte Zusammenführungsarbeit und gegebenenfalls deren Stoppgrund.

Trace-Reservierungen bleiben konservativ: Für den Fünfer-Wrapper werden 1024 Bytes zurückgehalten; sehr kleine Tracebyte-Budgets können schon vor dem ersten Teilrun stoppen. Jeder Teilrun benötigt ein positives Restbudget, auch wenn er anschließend ohne Kandidat endet.

Callback-Abbrüche werden auf globale Checkpointpositionen eingefroren. Bit-, Operations- und Definitionsformgrenzen während der Zusammenführung halten den Gesamtlauf unvollständig. Der globale Abschluss setzt vollständige Enumeration/Ableitung und vollständige Zusammenführung voraus. Ein früher positiver Treffer schließt die noch offenen Familien nicht aus.

## Replay und Projektion

Container `BRUCE_PORTFOLIO_BUNDLE_V8`, Engine `bruce-portfolio-v8`, Regel `normal-form-union-v1`, Policy `shared-budget-v2`, Planer `five-family-v1`, Gleichheitsregel `rational-domain-v1`. Baumgröße und alle bisherigen Such-/Budgetgrenzen werden mitgespeichert. Replay berechnet Teilruns, globale Normalformen, Herkünfte, Gruppierung, Zähler und Abbrüche erneut.

Die Z-Projektion wertet die globale Normalform mit Ausschlusspolynom aus. Unvollständige Gesamtläufe bleiben gesperrt, Pole werden abgewiesen, Ergebnisse müssen weiterhin ganzzahlige Int64-Werte sein. Der Auswahl-Replay enthält den Ursprungsrun und benötigt keine frühere CSV-, Forschungs- oder Regeldatei. Sämtliche älteren Replay-Pfade bleiben unverändert.

## Abnahme und weitere Pakete

Neue CLI-Anforderung zuerst rot; fünf Familien durchgängig; neue Ausdrucksfunde und familienübergreifende Zusammenführung; getrennte Definitionsbereiche; Herkunfts-/Arbeits-/Tracebuchhaltung; sämtliche kleinen Operations-, Kandidaten- und Checkpointgrenzen; Callback-Abbruch während Zusammenführung; Z-Auswahl und Replay ohne Quelldateien; manipulierter V8-Inhalt und historische V2-Ausdrucksfixture; unabhängiges Review; gesamte Debug-/UBSan-Suite und Demo. Nachweise im [Abnahmebericht](acceptance/expression-portfolio-v1-2026-09-22.md).

Nächstes eigenes Paket ist die versionierte adaptive und erfahrungsbasierte Planung dieser fünf Familien. Allgemeinere Grammatiken, vollständige Definitionsmengenentscheidungen und der Gesamtabschluss von M2 bleiben offen.
