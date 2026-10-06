# Abnahme Ausdruckspotenzen / Core 0.22 – 22.09.2026

## Lieferung

Optionaler Operator `^` für literale Exponenten 2 bis 8 aus dem Konstantenvorrat der eigenständigen Ausdruckssuche. Beide Planer und Gleichheitsmodi, unabhängige Gegenrechnung, erhaltene Definitionslücken, budgetierte Normalisierung, Ausdrucks-V5-Replay und Z-Projektion sind integriert. Historische Grammatiken ohne `^` bleiben unverändert. Portfolio und Experience sind noch nicht erweitert.

## Prüfkommandos und Ergebnis

```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure -j4
cmake --build build-ubsan -j4
ctest --test-dir build-ubsan --output-on-failure -j4
python3 scripts/demo.py
```

**42 CTest-Suiten bestanden**, jeweils im Debug-Build und mit UndefinedBehaviorSanitizer (`-fsanitize=undefined -fno-sanitize-recover=all`), ohne Sanitizer-Meldung. C++ mit `-Wall -Wextra -Werror`. Die erweiterte Gesamtdemo besteht. Alle **21 historischen Fixtures** wurden zusätzlich einzeln gegen ihr gespeichertes JSON-Ergebnis geprüft: identisch.

## Neue Prüffälle

Die neue Anforderung wurde vor der Produktänderung als fehlschlagender Test ausgeführt. Vier CLI-Testfälle und eine direkte C++-Suite prüfen:

- `x³+x` mit fünf Knoten, Konstanten 1–3 und `+^`: beide Planer und Gleichheitsmodi finden denselben bestätigten Befund. `+*` benötigt sieben Knoten; fünf reichen im deklarierten Raum nicht aus.
- Separate, vollständig materialisierte Referenzgrammatiken für alle 16 Operatorauswahlen mit `^`, drei Konstantenvorräte und Knotengrenzen 1, 3, 5: jeder Baum, Rang und Gesamtumfang stimmt mit beiden Produktionsplanern überein.
- Maximalraum mit 64 Konstanten 0–63, neun Knoten und allen Operatoren: 1450321801175 Kandidaten, 36 Indexblöcke; letzter Rang und 2000 deterministische Stichproben stimmen zwischen den Planern überein. Der Raum wurde nicht vollständig durchsucht.
- Exponenten 2–8 und Basen −2, −3/2, 0 und 2: Hauptrechnung, Gegenrechnung und Normalform stimmen mit einem separaten GMP-Potenzorakel überein. Exponenten −1, 0, 1, 9, 1/2 sowie variable oder berechnete Exponenten werden zurückgewiesen.
- Potenzierte Ausdrücke `1/x`, `x/x` und `0*(1/x)` behalten ihre Ausschlussbedingung x≠0 in allen drei Auswertungswegen. Ein überall undefinierter Basisausdruck bleibt undefiniert.
- Verschachtelte Potenzen erreichen Normalformgrad 64; die nächste Potenzierung überschreitet die Grenze kontrolliert. Bitgrenzen greifen sowohl in Haupt- als auch Gegenrechnung. CLI-Abbrüche durch Bit- oder Gradgrenzen liefern `INCOMPLETE` und identischen Replay.
- Kleine Potenzgrammatik mit beiden Planern und Gleichheitsmodi: alle kleineren positiven Operationsbudgets, Kandidaten- und Checkpointpositionen, Callback-Abbruch und exakt ausreichende Grenzen werden geprüft.
- Operations-, Kandidaten-, Trace-, Tracebyte- und Ergebnisgrenzen sind replaybar. Fehlende zulässige Exponenten ergeben einen gültigen verbleibenden Suchraum. Ungültige Auswahl und Potenzoperator im Portfolio werden abgewiesen.
- V5-Exporte mit manipulierten Regeln, Exponentenpolicy, Operatorauswahl oder Ergebniszählung werden trotz neu berechnetem Digest zurückgewiesen. Historische V4-Exporte dürfen nicht nachträglich `^` enthalten.
- `x^8` wird mit drei Knoten gefunden; bei −2 liefert die Z-Projektion 256. Polstellen und außerhalb Int64 liegende Projektionswerte werden gesperrt.
- Forschungs-Replay nach Entfernen aller Ursprungsdaten und Regeln, danach Auswahl-Replay nach zusätzlichem Entfernen der Forschungsdatei: identische Ergebnisse.

Die vor der Änderung mit Core 0.21 gesicherte Portfolio-V16-Fixture enthält einen historischen V5-Erfahrungssnapshot mit V14-Quelle. Dieser Replay sowie Ausdrucks-V4 werden zusätzlich direkt im neuen CLI-Test geschützt.

## Unabhängiges Review

Ein lesendes Review fand keine handlungsrelevanten Fehler in Grammatikzählung und Rangabbildung, Literalschranken, getrennten Rechenwegen, Ausschlussbedingungen, Grad-/Bitgrenzen, V5-Replay, Portfolio-Abgrenzung oder Z-Projektion.

Der Reviewer prüfte zusätzlich zwölf bestehende Ausdrucks-/Kompatibilitätssuiten und die damals vorhandenen drei neuen CLI-Testfälle. Eine separate Maximalraumprobe deckte alle 16 Auswahlen mit `^` und jeweils 1000 Ränge plus letzten Rang ab. Der vierte CLI-Test und die gesamten 42 Suiten wurden anschließend im Hauptlauf geprüft. Grenze: kein Git-Diff verfügbar; Review der aktuellen Dateien und Tests.

## Gemessene Demo

Indizierte Ausdruckssuche, rationale Zusammenführung, Konstanten 1–3, Lern- und Hold-out-Paare für x³+x:

| Messgröße | Produkte `+*`, sieben Knoten | Potenzen `+^`, fünf Knoten |
| --- | ---: | ---: |
| Kandidaten | 1884 | 130 |
| Logische Arbeit | 62045 | 3484 |

Beide vollständigen Läufe bestätigen dieselbe Formelgruppe. `+*` mit fünf Knoten liefert NO_CANDIDATE. Die Demo prüft V5-Replay, Z-Auswahl mit Wert 0 und Auswahl-Replay ohne Forschungsdatei. Die Zahlen vergleichen unterschiedliche deklarierte Suchräume, keine allgemeine CPU-Beschleunigung.

## Aussagegrenze

Implementiert ist eine begrenzte Erweiterung der Ausdrucksgrammatik. Sampleprüfungen beweisen keine allgemeine Datenbeziehung. Portfolio-/Experience-Einbindung, allgemeinere Exponenten und Funktionen sowie der vollständige M2-/M4-Abschluss bleiben offen.
