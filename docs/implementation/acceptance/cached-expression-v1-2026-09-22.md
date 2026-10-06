# Abnahme Teilbaumcache / Core 0.24 – 22.09.2026

## Lieferung

Optionaler Planer `cached-grammar` für die eigenständige Ausdruckssuche. Ein deterministischer Cache mit 64 Slots speichert ausschließlich echte Teilbäume mit drei Knoten. Kandidaten, Reihenfolge, IDs, mathematische Auswertung und Definitionsbedingungen bleiben unverändert. Transparente Kosten und Cachezähler, Ausdrucks-V6-Replay und Z-Projektion sind integriert.

## Prüfkommandos und Ergebnis

```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure -j4
cmake --build build-ubsan -j4
ctest --test-dir build-ubsan --output-on-failure -j4
python3 scripts/demo.py
```

**46 CTest-Suiten bestanden**, Debug und UndefinedBehaviorSanitizer (`-fsanitize=undefined -fno-sanitize-recover=all`), ohne Sanitizer-Meldung. C++ mit `-Wall -Wextra -Werror`. Die erweiterte Gesamtdemo besteht. Alle **23 historischen Fixtures** wurden zusätzlich separat mit ihren gespeicherten JSON-Ergebnissen verglichen: identisch. Der nachträglich ergänzte vierte CLI-Prüffall wurde regulär gezielt und im vollständigen UBSan-Lauf geprüft.

## Prüffälle

Die neue Anforderung wurde vor Produktänderung mit `UnknownSearchPlanner` als fehlende Funktion nachgewiesen. Vier CLI-Testfälle und eine direkte C++-Suite prüfen:

- Im Referenzbeispiel x²/(x+1) bleiben Hypothesen, Kandidatenanzahl, Reihenfolge der fachlichen Traceereignisse und vollständiger Status in beiden Gleichheitsmodi identisch zum Indexplaner. Die Kostendifferenz betrifft ausschließlich die Planung.
- Bei exakt ausreichendem neuen Operationsbudget bleibt der Cachelauf vollständig, der alte Indexlauf wird INCOMPLETE. Beide vollständigen Modi sind replaybar.
- Operatorauswahl einschließlich `+^` und `+-*/^`: gleiche vollständige Hypothesen wie mit bisherigem Indexplaner und identischer V6-Replay.
- Operations-, Kandidaten-, Trace-, Tracebyte- und Ergebnisgrenzen: INCOMPLETE und identischer Replay. Manipulierte Planer-/Cacheversion, Operatorauswahl und Ergebnisfelder werden trotz neu berechnetem Digest abgewiesen.
- Z-Auswahl bei x=0 liefert 0; bei x=−1 bleibt die Polstelle gesperrt. Auswahl-Replay funktioniert nach Entfernen der Forschungsdatei und Regeln.
- Ohne echte Teilbäume bleiben alle vier Cachezähler null, Kosten entsprechen dem Indexplaner. Wiederholte neue Läufe sind identisch. Der neue Planer bleibt für das Portfolio gesperrt.
- Alle 31 nichtleeren Operatorteilmengen, Konstanten 2, 3 und 1/2, bis fünf Knoten: jeder Kandidat wird vorwärts und rückwärts strukturell mit dem bisherigen nichtindizierten Generator verglichen.
- Maximalraum mit 64 Konstanten und neun Knoten: 2000 deterministische Zufallsränge und letzter Rang; gleiche Bäume, wirksame Verdrängung und höchstens 64 Einträge.
- Vollständige Siebenknoten-Grammatik mit Konstante 1: korrekte Teilbaumkopien an unterschiedlichen Positionen sowie tatsächliche Treffer und Fehlzugriffe.
- Jeder kleinere Kostenabbruch beim Aufbau eines ausgewählten Baums: Grenze eingehalten, spätere Generierung auf derselben Grammatikinstanz korrekt.
- Kleine Forschungsaufgabe x²+2 mit beiden Gleichheitsmodi: jede kleinere positive Operationsgrenze, alle Kandidaten- und Checkpoint-Abbruchpositionen, Callback-Abbruch und exakt ausreichendes Budget, jeweils mit Replay.

Die vor der Änderung mit Core 0.23 erzeugte Portfolio-V19-Fixture enthält ihren V6-Erfahrungssnapshot und eine V17-Quelle. Sie und die historische Ausdrucks-V5-Fixture sind direkt im neuen CLI-Test enthalten.

## Unabhängiges Review

Das lesende Review verglich die Produktionsdateien gegen eine vor der Änderung gesicherte Kopie. Es fand keine handlungsrelevanten Fehler. Eine separate Probe prüfte **31000 zufällige Kandidaten** mit maximal neun Knoten über alle **31 Operatorteilmengen**. Aus dem ungecacheten Indexbaum und einer separat simulierten Slotbelegung berechnete Kosten, sämtliche Zähler und alle Bäume stimmten überein.

Zusätzlich bestanden **10587 Abbruchfälle bei gefülltem Cache** mit anschließender Prüfung weiterer Kandidaten. Geprüft wurden historische Ausdrucks-Fixtures, Ablehnung für Portfolio/affine Suche und Experience-Import, V6-Metadaten, Z-Provenienz sowie die Grenze von 64 Dreiknotenbäumen. Vollsuite, UBSan und Dokumentation wurden im Hauptlauf geprüft. Es steht kein Git-Commit-Diff zur Verfügung; die Produktionskopie diente als Vergleichsbasis.

## Messung aus der Demo

Rationale Zusammenführung, Konstante 1, sieben Knoten, vier Grundrechenarten, Lern- und Prüfpaare für x²/(x+1):

| Messgröße | Indexplaner | Teilbaumcache |
| --- | ---: | ---: |
| Kandidaten | 2802 | 2802 |
| Planungsarbeit | 56843 | 50406 |
| Gesamte logische Arbeit | 93297 | 86860 |

Cache: 3374 Treffer, 14 Fehlzugriffe, 14 belegte Einträge, keine Verdrängung. Beide vollständigen Befunde sind identisch. Mit 86860 Operationseinheiten besteht nur der Cachelauf vollständig. V6-Replay startet mit leerem Cache; Auswahl-Replay benötigt keine ursprüngliche Forschungsdatei.

Die Messung umfasst die logische Cacheverwaltung, ist aber keine CPU-Zeitmessung oder universelle Beschleunigungsgarantie. Andere Zugriffsmuster können Mehrarbeit verursachen. Portfolio-/Experience-Einbindung und weitergehende Optimierungen bleiben offen; kein vollständiger M2-/M4-Abschluss.
