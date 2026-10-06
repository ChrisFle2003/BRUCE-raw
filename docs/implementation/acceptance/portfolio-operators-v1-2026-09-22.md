# Abnahme Portfolio-Operatorauswahl / Core 0.21 – 22.09.2026

## Lieferung

Operatorauswahl für das Ausdruckskind im Fünferportfolio, mit beiden Ausdrucksplanern und allen drei Reihenfolgen. Andere Familien behalten ihren Suchraum. Neue V14–V16-Replays, getrennte Erfahrungskontexte, gemischte V5-Speicher, Widerruf und Z-Projektion sind integriert. Die vollständige Standardgrammatik behält ihre historischen Pfade.

## Prüfkommandos

```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure -j4
cmake --build build-ubsan -j4
ctest --test-dir build-ubsan --output-on-failure -j4
python3 scripts/demo.py
```

**40 CTest-Suiten bestanden**, Debug und UndefinedBehaviorSanitizer (`-fsanitize=undefined -fno-sanitize-recover=all`), ohne Sanitizer-Meldung. C++ mit `-Wall -Wextra -Werror`. Die erweiterte Gesamtdemo besteht.

## Ausgeführte Prüffälle

Vier neue CLI-Testfälle und eine neue direkte C++-Suite. Die Portfolio-Anforderung wurde vor Produktänderung mit `OperatorsRequireExpressionStrategy` rot nachgewiesen. Der bisherige Ablehnungstest für die Operatoroption wurde entsprechend dem absichtlich erweiterten Schnittstellenvertrag auf weiterhin unzulässige Strategien angepasst.

- Beide Ausdrucksplaner, feste und adaptive Reihenfolge: gleiche vollständige Hypothesen innerhalb derselben Operatorauswahl, kleinere Grammatik und geringere Beispielarbeit als mit vollständiger Grammatik. Bei exakt ausreichendem neuen Operationsbudget ist die eingeschränkte Beispielsuche vollständig, die volle unvollständig.
- Die anderen vier Teilruns bleiben im vollständigen Referenzvergleich identisch. Zusätzliche Probe für x+1: Das reine Multiplikations-Ausdruckskind findet keinen Kandidaten; Potenz- und Polynomfamilie liefern trotzdem die bestätigte Gruppe.
- Feste V14- und adaptive V15-Quellen werden mit beiden Ausdrucksplanern geprüft und gespeichert; V16 als Quelle wird abgewiesen.
- Gemischter V5-Speicher mit alter vollständiger Indexgrammatik, neuer Index-Teilgrammatik, neuer nichtindizierter Teilgrammatik und anderer Operatorauswahl. Alte Record-IDs bleiben erhalten; jeder Ziellauf verwendet genau seinen passenden Kontext. Kanonisch äquivalente Zeichenreihenfolge funktioniert.
- Selbstausschluss, fehlender Einfluss alter Kontexte, Widerruf und Rückfall auf den Formplaner. Passende Stimmen sind `[0,0,0,0,1]`; Ausdruckssuche wird zuerst ausgeführt, alle fünf Familien bleiben Teil des vollständigen Laufs.
- Kleine Aufgaben mit familienübergreifender und reiner Ausdrucksherkunft unter allen sechs Planerkombinationen: jede kleinere positive Operationsgrenze, alle Checkpoint- und Kandidaten-Abbruchpositionen, Callback-Abbruch, exakte globale Abrechnung und Replay funktionieren.
- Operations-, Kandidaten-, Trace-, Tracebyte- und Ergebnisgrenzen werden in allen sechs CLI-Kombinationen eingehalten. Ungültige Operatorangaben und andere Strategien werden abgewiesen.
- Nichtkanonische oder vollständige Operatorauswahl in V14, geänderte Operatoren, fremde Ausdrucksplanerversion und manipulierte Kinderzählung werden trotz neu berechnetem Digest zurückgewiesen.
- Polstellenprobe x²/(x+1) mit `+*/`, beiden Ausdrucksplanern und fester/adaptiver Reihenfolge: x=0 liefert SELECTED, x=−1 bleibt gesperrt.
- V16-Forschungs-Replay nach Entfernen sämtlicher Originaleingaben, Quellen, Regeln und Speicher; anschließend Auswahl-Replay nach zusätzlichem Entfernen der Forschungsdatei: identische Ergebnisse.
- Alle 20 historischen Fixtures separat mit ihren gespeicherten JSON-Ergebnissen verglichen, einschließlich der vor Änderung mit Core 0.20 erzeugten Ausdrucks-V4-Fixture sowie V13/V10 mit eingebetteten historischen Erfahrungsspeichern.

## Unabhängiges Review

Das lesende Review fand keine handlungsrelevanten Fehler in Versionszuordnung, Standardpfaden, Kind-Isolation, V5-Kontexten, Quellenprüfung, Ausschluss gelernter Quellen, Widerruf oder Z-Kennungen.

Zusätzlich bestanden **152 unabhängige Proben**: alle 14 echten Operator-Teilmengen, beide Ausdrucksplaner, feste/adaptive Reihenfolge und ausgewählte Trace-, Byte- sowie Ergebnisgrenzen, jeweils mit identischem Bundle-Replay und eingehaltenen Tracegrenzen. Grenzen: kein Git-Diff verfügbar; Review der aktuellen Dateien und Tests. Gesamtsuiten und Dokumentation wurden im Hauptlauf geprüft.

## Messungen aus der Demo

Konstante 1, fünf Knoten, Potenz-/Polynomgrenze 2, indizierter Ausdrucksplaner, Paare für x²+x:

| Messgröße | Vollständige Ausdrucksgrammatik | Ausdrucksgrammatik +* |
| --- | ---: | ---: |
| Ausdruckskandidaten | 184 | 32 |
| Gesamte feste Portfolioarbeit | 5431 | 1822 |

Die übrigen vier Teilruns bleiben identisch. Die erfahrungsbasierte Zielsuche benötigt 1644 logische Einheiten; die separat ausgewiesene Quellenprüfung 1869. Die Demo prüft gleiche vollständige Hypothesen gegenüber adaptiver Reihenfolge derselben Grammatik, Widerruf sowie Forschungs- und Auswahl-Replay ohne ursprüngliche Dateien.

## Grenzen

Die Operatorauswahl schränkt einen Teil des Suchauftrags ausdrücklich ein. Sie ist keine automatische Widerlegung ausgeschlossener Formeln und keine Beschränkung der anderen vier Familien. Logische Arbeitseinheiten sind keine CPU-Benchmark; Samplebestätigung ist kein allgemeiner Beweis. Neue Operatorarten, freiere Grammatiken, weitergehende Experience-Verträge und M2-/M4-Abschluss bleiben offen.

[Vertrag](../45-portfolio-operators-contract.md), [Benutzung](../46-portfolio-operators-usage.md).
