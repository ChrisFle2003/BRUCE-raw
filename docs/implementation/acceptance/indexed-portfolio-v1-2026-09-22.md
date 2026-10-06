# Abnahme indiziertes Fünferportfolio / Core 0.19 – 22.09.2026

## Lieferung

Der Grammatikindex ist im festen, adaptiven und erfahrungsbasierten Fünferportfolio auswählbar. Gemeinsame Budgets, vollständige Herkunft, getrennte Erfahrungskontexte, gemischte V4-Speicher mit Widerruf, V11–V13-Replay und Z-Projektion sind integriert. Der bisherige Planer bleibt Standard.

## Ausgeführte Prüfungen

```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure -j4
cmake --build build-ubsan -j4
ctest --test-dir build-ubsan --output-on-failure -j4
python3 scripts/demo.py
```

**36 CTest-Suiten bestanden**, sowohl Debug als auch UndefinedBehaviorSanitizer (`-fsanitize=undefined -fno-sanitize-recover=all`), ohne Sanitizer-Meldung. C++ mit `-Wall -Wextra -Werror`. Drei neue CLI-Testfälle und eine direkte C++-Suite. Die neue CLI-Anforderung wurde vor Implementierung mit `UnknownOption: --expression-planner` rot geprüft. Die erweiterte Gesamtdemo besteht.

Geprüfte Fälle:

- Feste und adaptive Reihenfolge mit unveränderten vollständigen Hypothesen, IDs, Normalformen, Herkunft und Status gegenüber `grammar`; gleiche Familienmenge und Reihenfolge.
- Im Beispiel jeweils 12221 logische Einheiten weniger; beim exakt ausreichenden Indexbudget bleibt der bisherige Lauf unvollständig.
- Fester V11- und adaptiver V12-Lauf können unabhängige Erfahrungsquelle sein; erfahrungsbasierter V13-Lauf wird als Quelle zurückgewiesen.
- V4-Mischspeicher aus alten und neuen Quellen, unveränderte alte Record-ID, jeweils nur passende Stimmen trotz gleicher Familienanzahl, Selbstausschluss und Widerruf mit Rückfall auf den Formplaner.
- Erfahrungsbasierter Lauf liefert dieselben vollständigen Hypothesen; Ausdruckssuche beginnt bei passender Stimme zuerst.
- Jede kleinere positive Operationsgrenze und sämtliche Checkpoint-/Kandidaten-Abbruchpositionen zweier kleiner Aufgaben unter allen drei Reihenfolgen liefern INCOMPLETE und identischen Replay. Callback-Abbruch, familienübergreifende Herkunft, exakte Kostenabrechnung und ausreichend bemessene Grenzbudgets bestehen.
- Operations-, Kandidaten-, Trace-, Tracebyte- und Ergebnisgrenzen werden in allen drei CLI-Modi durchgesetzt. Ungültige Planerauswahl wird in CLI und direkter API zurückgewiesen.
- Manipulierte Planerversion und Indexmetadaten werden trotz neu berechnetem äußerem Digest zurückgewiesen.
- Z-Auswahl des V13-Laufs bei x=0 liefert SELECTED mit Wert 0; x=-1 bleibt gesperrt. Forschungs-Replay nach Entfernen sämtlicher Eingabe-/Quell-/Speicherdateien sowie Auswahl-Replay nach zusätzlichem Entfernen der Forschungsdatei stimmen überein.
- Tatsächlich vor Änderung gesicherte Ausdrucks-V3-Fixture aus Core 0.18 sowie alte V8/V10 mit eingebettetem Erfahrungssnapshot bleiben unverändert replaybar. Die gesamte historische Regression besteht.

## Unabhängiges Review

Das lesende Review fand keine konkreten Fehler in Validierung, V11–V13-Versionen, alten Pfaden, Budget-/Abbruchweitergabe, Experience-V4-Kontexten, Widerruf oder Projektion.

Zusätzliche **144 begrenzte Läufe** auf zwölf deterministisch erzeugten Datensätzen prüften feste/adaptive Reihenfolge, vollständige Befundgleichheit zwischen altem und indiziertem Planer, fünf Budgetgrenzen und Replay. Alle bestanden. **Alle 18 historischen Bundles** lieferten byte-identisches Ergebnis-JSON.

Grenzen des Reviews: kein verlässlicher Git-Vergleich verfügbar; die zusätzlichen Zufallsprüfungen verwendeten maximal drei Knoten. Größere Grammatik und Erfahrungsabbrüche wurden anhand des Codes und der vorhandenen Tests geprüft, nicht durch zusätzliche Zufallsprüfungen.

## Referenzmessungen aus der Demo

| Lauf | Bisheriger Planer | Indexplaner |
| --- | ---: | ---: |
| Fünferportfolio, feste Reihenfolge | 105744 | 93523 |
| Erfahrungsbasierte Zielsuche | 102654 | 90433 |
| Separate Nachweisprüfung der adaptiven Quelle | 105838 | 93617 |

Alle Werte sind logische Operationseinheiten. Die vollständigen Befunde stimmen überein. Die Zielsuche verwendet `[0,0,0,0,1]` Stimmen und führt trotzdem alle fünf Familien aus. Im Ausdruckskind entstehen weiterhin 2802 Kandidaten bei 20 Indexeinträgen.

Das gemeinsame Operationsbudget 93523 wurde zusätzlich direkt mit den dokumentierten Beispieldateien geprüft: bisheriger Planer INCOMPLETE, Indexplaner VERIFIED_ON_SAMPLES. Beide verbrauchen höchstens das deklarierte Budget.

## Grenzen und Fortschreibung

Dies ist eine überprüfte Arbeitsersparnis im bestehenden Kostenmodell, keine gemessene CPU-/Laufzeitbeschleunigung. Der Suchraum bleibt unverändert begrenzt; der Index spart nicht in jeder kleinen Konfiguration. Erfahrungsstimmen beeinflussen nur die Reihenfolge. Sampleprüfungen sind kein allgemeiner Beweis; allgemeinere Synthese, externe Evidenz, weitergehende Experience-Korrekturen und M2-/M4-Gesamtabschluss bleiben offen.

[Vertrag](../41-indexed-portfolio-contract.md), [Benutzung](../42-indexed-portfolio-usage.md).
