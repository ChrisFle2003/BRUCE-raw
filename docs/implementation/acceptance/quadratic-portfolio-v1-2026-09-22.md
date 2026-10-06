# Abnahme Viererportfolio / Core 0.13 – 22.09.2026

## Lieferung

`portfolio-quadratic` integriert die vier vorhandenen Familien unter gemeinsamen Budgets, mit festen, datenabhängigen oder erfahrungsgewichteten Reihenfolgen. Quadratische Nenner bleiben vollständig in Identität, Erklärung, Gegenprüfung, Replay und Z-Projektion erhalten. Neue Replay-Versionen V5/V6/V7 sowie Experience V2 erhalten die alten Versionen und erlauben gemischte Quellspeicher mit getrennten Planungskontexten.

## Ausgeführte Prüfungen

```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure -j4
cmake --build build-ubsan -j4
ctest --test-dir build-ubsan --output-on-failure -j4
python3 scripts/demo.py
```

**24 CTest-Suiten bestanden**, sowohl Debug als auch UndefinedBehaviorSanitizer (`-fsanitize=undefined -fno-sanitize-recover=all`); keine Sanitizer-Meldung. C++ mit `-Wall -Wextra -Werror`. Sechs neue CLI-Tests und eine direkte C++-Suite; die gesamte Demo ist erfolgreich durchgelaufen. Die neue CLI-Anforderung scheiterte vor Implementierung erwartungsgemäß an `UnknownResearchStrategy` bzw. der alten Planerbindung.

Geprüfte Fälle:

- vier tatsächliche Teilruns; exakter Treffer für `1/(x²+1)`, erhaltene Nennerkoeffizienten und Definitionsbedingung;
- gleiche vollständige Hypothesen zwischen fester und datenabhängiger Reihenfolge im Referenzfall;
- Erfahrungsquelle `(x+2)/(x²+1)` beeinflusst die neue Suche `(2*x+3)/(x²+1)` mit Stimmen `[0,0,0,1]`;
- positiver früher Treffer bei Kandidatenlimit ergibt weiterhin Gesamtstatus `INCOMPLETE`;
- aktuelle Hold-out-Werte verändern Plan und Stimmen nicht;
- Widerruf entfernt den Planungseinfluss; historischer V7-Replay funktioniert ohne ursprüngliche Quell- und Storedateien;
- gemischter V1/V2-Speicher erhält alte Datei und Ereignis-ID; alte und neue Quellen beeinflussen nur die jeweils passende Familienmenge;
- Selbstverwendung gleicher Lerndaten zählt nicht; andere Grad-/Potenzgrenzen passen nicht;
- manipulierte eingebettete Evidenz wird trotz neuem äußerem Digest abgewiesen; erfahrungsgeplante Quellen bleiben ausgeschlossen;
- verschiedene Nennerdarstellungen bleiben auch bei gleichen Werten getrennte Kandidaten;
- Kandidaten-, Operations-, Traceereignis- und Tracebytegrenzen mit unvollständigem, identisch replaybarem Befund;
- alle globalen Checkpoints, Callback-Grenzen und jede kleinere positive Operationsgrenze für vier Datenformen und alle drei Planer; exakt ausreichende Operations-/Tracebudgets;
- größtmögliche Quellslotkonfiguration mit Koeffizienten `2047/16`, Gesamt-ID `2^62−1`, erfolgreicher Z-Projektion und eingebettetem Auswahl-Replay;
- neue Slotgrenze weist unadressierbar große Familien bereits vor Ausführung ab;
- gesicherter unveränderter Core-0.12-Export und alle vorherigen Replay-Fixtures bleiben identisch reproduzierbar.

## Review und Korrekturen

Ein Planungstest deckte einen echten Hinweisgleichstand auf: Die ersten vier Werte von `1/(x²+1)` passen auch zu einem quadratischen Polynom. Die neue Heuristik bevorzugt in diesem Fall ausdrücklich den quadratischen Kehrwerthinweis. Dies ändert nur Reihenfolge und Planungsarbeit; alle Familien werden weiterhin unabhängig geprüft. Ein späterer Fehlschlag betraf ausschließlich einen doppelten Gradparameter im Testhelfer und wurde dort korrigiert.

Das unabhängige lesende Review fand keine blockierenden Korrektheits- oder Kompatibilitätsprobleme. Zusätzliche Reviewer-Prüfungen: 30 reproduzierbare zufällige quadratische Bruchfälle mit gleichen vollständigen Befunden bei fester/adaptiver Reihenfolge; Maximal-ID mit Cantor-Adresse `9223372032559808512` und Auswahl-Replay; fünf bestehende Portfolio-/Bruch-Fixtures unverändert. Die empfohlene dauerhafte Maximal-ID-Prüfung wurde aufgenommen.

## Sichtbarer Nutzen und Kosten

Die Gesamtdemo zeigt das Viererportfolio mit `rational-quadratic` an erster Stelle für `1/(x²+1)`. Bei der neuen Aufgabe `(2*x+3)/(x²+1)` beginnt die Grundheuristik mit Potenzen, die passende Erfahrung mit quadratischen Bruchfunktionen. Der vollständige fachliche Befund ist identisch. Gemessen: **662 logische Einheiten für die neue Suche**, **648 separat für den Erfahrungsnachweis**. Kein allgemeiner Geschwindigkeitsvorteil wird daraus abgeleitet.

## Verbleibende Grenzen

Vier begrenzte Familien, keine freie Ausdruckssynthese. Strukturell unterschiedliche rationale Darstellungen werden nicht algebraisch zusammengeführt und können auch bei gleicher Funktion mehrdeutig bleiben. Die Erfahrung beeinflusst nur die Reihenfolge, keine Wahrheitsentscheidung; Learn-/PublishGate bleiben geschlossen. Keine allgemeine Beweisaussage und kein vollständiger M2-/M4-Abschluss. [Vertrag](../29-quadratic-portfolio-contract.md), [Benutzung](../30-quadratic-portfolio-usage.md).
