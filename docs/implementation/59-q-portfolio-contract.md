# Q-Gruppen im Portfolio und Erfahrungskontext – Core 0.28

## Umfang

Die freigegebene Fortsetzung schaltet `--equivalence rational-q` für `portfolio-expression` frei. Der Ausdrucksteilrun nutzt Core 0.27/V7. Alle fünf Familien werden gemeinsam über Q gruppiert. Die anderen vier Suchverfahren bleiben unverändert. `rational` bleibt Standard und behält sein bisheriges Verhalten.

Die feste, adaptive und erfahrungsbasierte Suchreihenfolge sowie `grammar`, `indexed-grammar` und `cached-grammar` sind kombinierbar. Die bereits vorhandenen Operatorauswahlen und literalen Potenzen bleiben erhalten.

## Feste Reihenfolge der Zusammenführung

Bei `rational-q` finden zuerst die Teilruns in der gewählten Suchreihenfolge statt. Anschließend werden die vorhandenen Teilruns in fester Reihenfolge vereinigt: Potenz, Polynom, linearer Bruch, quadratischer Bruch, Ausdruck. Auch die Ursprünge innerhalb einer Familie behalten ihre bestehende Reihenfolge.

Dies ist nötig, weil ein begrenzter Bereichsvergleich bei höheren Restgraden offen bleiben kann. Die Auswahl des Repräsentanten soll bei vollständig durchlaufenen Suchen nicht von der Suchplanung abhängen. Es wird keine vollständige Äquivalenzklassenzerlegung behauptet: Unentschiedene Paare bleiben getrennt, und bereits bestehende Gruppen werden nicht nachträglich rückwirkend vereinigt.

Eine neue Quelle wird mit den vorhandenen vollständigen Repräsentanten verglichen. Nur dieselbe gekürzte rationale Funktion und ein abgeschlossener Q-Bereichsnachweis `EQUAL` erlauben die Zusammenführung. Der Vergleich verwendet `rational-domain-gcd-quadratic-v1`. `UNKNOWN` ist keine Ungleichheitsbehauptung. Unvollständige Gruppen sind keine Vergleichsziele.

Die erste passende Gruppe behält ID, Normalform und Repräsentanten. Ihre Referenzen auf Familienquellen werden ergänzt. Die vollständigen Ausdrucksbäume bleiben in den Teilruns erhalten. Der Auswahlimport verwendet den Gruppenrepräsentanten; nachgewiesene Gleichheit über Q umfasst alle ganzzahligen Projektionsstellen.

## Gemeinsame Budgets und Teilbefunde

Es gilt weiter:

`operations_used = ordering_operations + Summe(Teilrun-Operationen) + union_operations`.

Q-Vergleiche werden vollständig vor den jeweiligen Rechenschritten belastet. `domain_comparison_operations`, `domain_comparisons` und `domain_unknown` auf Portfolioebene summieren Ausdruckskind und Familienvereinigung. Vergleichskosten der Vereinigung sind **auch Teil von `union_operations`**, aber nicht zusätzlich Teil von `normalization_operations`. Normalisierung zählt Kind-Normalisierung plus die übrige Normalisierungs-/Verwaltungsarbeit der Vereinigung. Diese teilweise überlappenden Messgrößen dürfen nicht pauschal addiert werden.

Die nachgelagerte Vereinigung muss vom verbleibenden Arbeitsbudget leben. Vollständige Teilruns können daher einen unvollständigen Gesamtbefund ergeben. Unvollständige Quellen und ein beim Vereinigen unterbrochener Ursprung bleiben als Teilbefund markiert. Noch nicht vereinigte Quellen bleiben in `strategy_runs` erhalten, auch wenn sie noch nicht in den Gesamtgruppen erscheinen.

- Höchstens 128 Originaltreffer über alle Familien. Ausdrucksgruppen sparen keine Ursprungsplätze.
- Ausstehende UNION-Ereignisse werden schon bei der Vergabe der Teilrun-Spurbudgets berücksichtigt.
- Reservierter Rahmen für Portfolio-Spurbytes: 1792 bei fester Reihenfolge, 2048 bei adaptiver/erfahrungsbasierter Reihenfolge. Das ist eine konservative Reserve, keine tatsächlich verbrauchte Bytezahl.
- Ein UNION-Ereignis je begonnenem Vereinigungsteil enthält Gruppenanzahl, kumulierte Vereinigungsarbeit, begonnene Bereichsprüfungen und offene Entscheidungen dieses Teils.
- Bestehende globale Kandidaten-/Checkpoint-Abbrüche funktionieren auch bei der nachgelagerten Vereinigung und sind replaybar. Die Kontrollpunkte sind vor Teilplanung/Kandidaten und vor jedem zu vereinigenden Ursprung, nicht innerhalb einer Polynomoperation.
- Ein partieller Suchlauf darf je nach Suchreihenfolge andere Teilbefunde behalten. Die Reihenfolgenunabhängigkeit der Gruppierung betrifft dieselben vollständig berechneten Quellen.

## Versionierung und Replay

| Reihenfolge | Portfolio | Engine / Planer |
| --- | --- | --- |
| fest | `BRUCE_PORTFOLIO_BUNDLE_V23` | `bruce-portfolio-v23` / `five-family-q-v1` |
| adaptiv | `BRUCE_PORTFOLIO_BUNDLE_V24` | `bruce-portfolio-v24` / `sample-shape-v8` |
| Erfahrung | `BRUCE_PORTFOLIO_BUNDLE_V25` | `bruce-portfolio-v25` / `experience-order-v8` |

Gemeinsam: `q-domain-union-v1`, `shared-budget-v3`, `rational-q-domain-v1`, `rational-domain-gcd-quadratic-v1`. Operatoren, Ausdrucksplaner, Cachepolicy (`none` oder `direct-map-64-size3-v1`) und `literal-2-to-8-v1` sind explizit gespeichert. Die ersten vier Kindformate bleiben unverändert, das Ausdruckskind verwendet V7.

Replay berechnet Quellen, Suchplanung, Vereinigung, Abbrüche, Kosten, Herkunft und Trace erneut und vergleicht die vollständige kanonische Nutzlast. Historische Formate und Kontexte bleiben unverändert. Z-Import und eingebettetes Auswahl-Replay akzeptieren V23–V25 mit eigener Herkunft `portfolio-q-v<Version>-ops<Maske>-<Ausdrucksplaner>-rational-v1`. Die bisherigen Sperren für unvollständige Forschung, Polstellen, nichtganzzahlige Ergebnisse und Int64-Überläufe bleiben bestehen.

## Experience V8

Q-Quellen erzeugen `BRUCE_EXPERIENCE_V8`, `bruce-experience-v8`, `routing-experience-v8`. Nur unabhängig geplante V23/V24-Quellen werden aufgenommen. V25 wird als neue Quelle ausgeschlossen. Aufnahme und jede Nutzung lesen/replayen den gespeicherten Quellenstand nach den bisherigen Quellenbudgets.

Der Kontext `routing-context-v8-five-q` bindet Bereichs-, Vereinigungs-, Budget-, Exponenten- und Cachepolicy sowie Ausdrucksplaner, Operatoren, Zahlendomäne, Parametergrenzen, Bit-/Nenner-/Potenz-/Grad-/Knotengrenzen, Gleichheitsmodus und Formhinweis. Alte rationale Kontexte und andere Ausdrucksplaner vergeben im Q-Kontext keine Stimmen.

Globale Trainings-Deduplizierung, Ausschluss eigener Lernpaare, Widerruf und eingebettete Snapshots bleiben unverändert. Gemischte Speicher dürfen alte und neue Quellen enthalten; alte Ereignisse und Kennungen werden nicht neu geschrieben. Die Grenzen bleiben 8 Quellen, 16 Ereignisse, 1 MiB Speicher und höchstens 1 Million deklarierte Suchoperationen pro Quelle. Die Quellenprüfung wird separat als zusätzliche Arbeit ausgewiesen.

Die Stimmen beeinflussen ausschließlich die Suchreihenfolge. Alle fünf Familien bleiben enthalten; es gibt weder mathematische Regelpromotion noch allgemeines Lernen.

[Benutzung](60-q-portfolio-usage.md) · [Abnahme](acceptance/q-portfolio-v1-2026-09-22.md)
