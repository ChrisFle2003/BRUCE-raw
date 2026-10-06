# Abnahme Operatorauswahl / Core 0.20 – 22.09.2026

## Lieferung

Explizite Teilmengen der vier Grundrechenarten für die eigenständige Ausdruckssuche. Beide Planer und beide Gleichheitsmodi, kanonische Auswahl, kleinere lazy Grammatik, V4-Replay und Z-Projektion sind integriert. Die vollständige Standardgrammatik behält ihre bisherigen Versionen und Ergebnisse. Portfolio und Erfahrungskontexte werden in diesem Paket nicht erweitert.

## Ausgeführte Prüfungen

```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure -j4
cmake --build build-ubsan -j4
ctest --test-dir build-ubsan --output-on-failure -j4
python3 scripts/demo.py
```

**38 CTest-Suiten bestanden**, Debug und UndefinedBehaviorSanitizer (`-fsanitize=undefined -fno-sanitize-recover=all`), ohne Sanitizer-Meldung. C++ mit `-Wall -Wextra -Werror`. Drei neue CLI-Testfälle und eine neue direkte C++-Suite. Die CLI-Anforderung wurde vor Produktänderung mit `UnknownOption: --operators` rot nachgewiesen. Gesamtdemo erfolgreich.

Geprüfte Fälle:

- Alle 15 nichtleeren Operatorauswahlen gegen die vollständige Grammatik, deren Bäume erst nach Erzeugung anhand ihrer tatsächlichen Operatoren gefiltert werden. Knotenweiser Vergleich mit beiden Planern erhält die vollständige gefilterte Folge: nur x bis neun Knoten, x und Konstante 1 bis sieben Knoten, x sowie −1, 0, 2/3 bis fünf Knoten.
- 64 Konstanten, neun Knoten, jede Operatorauswahl: gleicher Raum und letzter Rang für beide Planer, jeweils 200 deterministische große Zufallsränge knotenweise gleich. Die volle bisherige Obergrenze wird nicht überschritten.
- Kleine Räume mit unabhängigen festen Erwartungen: fünf Ausdrücke bei nur Addition und drei Knoten, sechs bei nur Subtraktion, zwei Terminale bei einem Knoten. Ungültige Ränge werden zurückgewiesen.
- Leere, doppelte, fremde und mit Leerzeichen versehene Operatoren werden abgewiesen. Eingabereihenfolge wird normalisiert; vollständige Auswahl liefert das bisherige Ergebnis. Andere CLI-Strategien und abweichende direkte API-Konfigurationen werden zurückgewiesen.
- Beide Planer liefern innerhalb derselben Operatorauswahl dieselben Hypothesen, einschließlich Herkunft und Normalformen. Syntaxmodus erhält unterschiedliche passende Bäume; rationale Zusammenführung erhält eine bestätigte Gruppe.
- Sämtliche kleinen positiven Operationsabbrüche, Checkpoint- und Kandidaten-Abbruchpositionen bei Addition und Division, unter beiden Planern und beiden Gleichheitsmodi, bleiben unvollständig und identisch replaybar. Callback-Abbruch und exakt ausreichende Budgets bestehen.
- CLI-Grenzen für Operationen, Kandidaten, Trace, Tracebytes und Ergebnisse bleiben wirksam. Unvollständige Quellen werden nicht zur Z-Auswahl zugelassen.
- Manipulierte Operatorauswahl, nichtkanonische Reihenfolge, Grammatikversion, Planerversion und Ergebniszählung werden auch bei neu berechnetem äußerem Digest zurückgewiesen.
- Beschränkte Divisionsgrammatik findet 1/x; Z-Projektion bei x=−1 liefert −1, x=0 bleibt gesperrt. Forschungs- und Auswahl-Replay benötigen die entfernten ursprünglichen Eingabe-/Regel-/Forschungsdateien nicht.
- Alle 19 historischen Fixtures separat gegen ihre gespeicherten JSON-Ergebnisse geprüft, einschließlich der vor Änderung mit Core 0.19 erzeugten V13-Fixture samt eingebettetem V4-Erfahrungssnapshot und V12-Quelle.

## Unabhängiges Review

Das lesende Review fand keine konkreten Fehler in Grammatikzählung, Rangfolge, Größenbegrenzung, Validierung, V4-Replay, historischen Standardpfaden, Portfolio-Abgrenzung oder Projektionskennungen.

Neben den neuen Tests führte der Reviewer unabhängige Prüfungen über alle 15 Operatorauswahlen, beide Planer und beide Gleichheitsmodi aus, einschließlich Projektion und Replay. Zusätzlich wurden 224 manipulierte V4-Köpfe geprüft. Keine Dateien geändert; kein verlässlicher Git-Vergleich verfügbar. Gesamtdokumentation und vollständige Suiten wurden vom Hauptlauf geprüft.

## Beobachtungen und korrigierte Fehler

Die erste Syntax-Testerwartung war zu eng: x*(x+1) und x+x*x sind zwei erlaubte Schreibweisen, daher ist AMBIGUOUS ohne Zusammenführung richtig. Die Erwartung wurde entsprechend präzisiert.

Der erste Projektionspfad verwendete rohe Operatorsymbole in der Z-Domänenkennung. Deren bestehender Vertrag erlaubt keine Zeichen wie `/` oder `*`; der neue Projektionstest meldete InvalidDomainId. Die Kennung kodiert die Auswahl jetzt eindeutig als vier Bit und erfüllt den bestehenden Kennungsvertrag. Die Auswahl bleibt im Forschungsnachweis lesbar gespeichert.

## Gemessener Referenzlauf

Lernpaare aus `selected-operators-training.csv`, Hold-out aus `selected-operators-holdout.csv`, Konstante 1, fünf Knoten, indizierter Planer und rationale Zusammenführung:

| Messgröße | Vollständige Grammatik | Nur Addition und Multiplikation |
| --- | ---: | ---: |
| Ausdrücke | 184 | 32 |
| Logische Operationseinheiten | 5206 | 1597 |
| Befund | VERIFIED_ON_SAMPLES | VERIFIED_ON_SAMPLES |

Die bestätigte Normalform ist x²+x. Bei ausschließlich Addition ergibt derselbe begrenzte Auftrag NO_CANDIDATE. Die Demo prüft diese Ergebnisse, V4-Replay und Z-Auswahl mit Wert 0.

## Grenzen

Die Ersparnis folgt aus einem ausdrücklich kleineren Suchauftrag. Andere Formeln werden dadurch nicht widerlegt. Es handelt sich nicht um eine CPU-Benchmark oder einen allgemeinen mathematischen Beweis. Neue Operatorarten, frei definierte Grammatikregeln, Portfolio-/Experience-Einbindung der Auswahl und der vollständige M2-Abschluss bleiben offen.

[Vertrag](../43-expression-operators-contract.md), [Benutzung](../44-expression-operators-usage.md).
