# Abnahme Ausdruckszusammenführung / Core 0.15 – 22.09.2026

## Lieferung

Optionaler Modus `--equivalence rational` der bestehenden Ausdruckssynthese. Gekürzte rationale Polynomnormalform, monisches quadratfreies Ausschlusspolynom, erhaltene Originalbäume und Samplebefunde, begrenzte Herkunftslisten, gemeinsame Arbeitsbudgets, V2-Replay und Z-Import. Der bisherige Syntaxmodus bleibt Standard.

## Ausgeführte Prüfungen

```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure -j4
cmake --build build-ubsan -j4
ctest --test-dir build-ubsan --output-on-failure -j4
python3 scripts/demo.py
```

**28 CTest-Suiten bestanden**, sowohl Debug als auch UndefinedBehaviorSanitizer (`-fsanitize=undefined -fno-sanitize-recover=all`), keine Sanitizer-Meldung. C++ mit `-Wall -Wextra -Werror`. Vier neue CLI-Tests und eine neue direkte C++-Suite. Die neue CLI-Anforderung wurde vor Implementierung mit `UnknownOption: --equivalence` rot nachgewiesen. Gesamtdemo erfolgreich.

Geprüfte Fälle:

- Die drei positiv geprüften Schreibweisen von x²/(x+1) bilden eine Gruppe; zwei weitere Schreibweisen mit zusätzlichem Ausschluss x=0 bleiben als widerlegte zweite Gruppe erhalten.
- IDs, Formeln, Originalbäume und Samplebefunde aller fünf Herkünfte stimmen mit dem bisherigen Syntaxmodus überein.
- x/x bleibt von 1 getrennt; x²/x² wird mit x/x zusammengeführt; Kürzung von (x²−1)/(x−1) erhält die Lücke bei x=1.
- Verschachtelte Division, negative rationale Skalierung und Nullmultiplikation bewahren alle ursprünglichen Definitionslücken; ein identisch verschwindender Divisor ergibt einen leeren Definitionsbereich.
- 0/(x²+1) bleibt konservativ von 0 getrennt: Es wird keine noch nicht implementierte Nullstellenanalyse behauptet.
- Alle 2802 Bäume bis sieben Knoten mit Konstante 1: Original und Normalform stimmen an sechs ganzzahligen/rationalen Eingaben einschließlich Polstellen überein; gleiche Gruppenschlüssel enthalten gleiche Normalformen.
- Jede kleinere positive Operationsgrenze, alle Kandidaten-/Checkpoint-Abbrüche, Callback-Abbruch und exakt ausreichende Budgets zweier kleiner Referenzläufe sind korrekt und replaybar.
- Kandidaten-, Trace-, Tracebyte- und Ergebnisgrenzen halten den Befund unvollständig. Die Ergebnisgrenze zählt Herkünfte, nicht Gruppen.
- Symbolisches Bitlimit: x+x bei x=0 passt numerisch, seine Normalisierung scheitert bei einem Bit am Koeffizienten 2; Teilbefund bleibt INCOMPLETE und identisch replaybar.
- Veränderte Gleichheitsversion, Ausschlusskoeffizienten und Ursprungs-ID werden trotz neu berechneter äußerer Prüfsumme zurückgewiesen.
- Z-Projektion bei x=0 liefert eine Gruppe mit Wert 0 und SELECTED; x=-1 wird als Polstelle zurückgewiesen. Auswahl-Replay funktioniert nach Entfernen von Ursprungsrun und Regeldatei.
- Vor Änderungen gesichertes tatsächliches Core-0.14-V1-Paket und sämtliche älteren Replay-Fixtures bleiben unverändert reproduzierbar.

## Unabhängiges Review

Ein lesendes Review fand keine blockierenden Probleme in Normalisierung, Ausschlussbedingungen, Herkunft, Budgets oder Replay. Ein separat kompilierter Prüfhelfer verglich alle 48330 Ausdrücke bis neun Knoten mit Konstante 1 sowie 30000 reproduzierbar gewählte Ausdrücke mit Konstanten −1, 0 und 2/3 an jeweils acht rationalen Eingaben. Insgesamt **626640 Vergleiche** zwischen Originalauswertung, unabhängiger Ganzzahl-Gegenrechnung und Normalform stimmten einschließlich Polstellen überein. Nenner und nichtleere Ausschlusspolynome waren monisch. Diese zusätzliche Stichproben-/Raumprüfung ersetzt keinen Beweis der gefundenen Datenbeziehung.

Eine anfängliche Testerwartung übersah die ebenfalls erzeugte Schreibweise 1*1. Die korrekte Gruppe ohne Ausschlüsse enthält 1, 1*1 und 1/1; der Test wurde entsprechend dem bestehenden Grammatikvertrag korrigiert.

## Sichtbares Ergebnis

Unveränderte Ausdrucks-Beispielpaare, einzige Konstante 1, maximal sieben Knoten: **2802 untersuchte Bäume, fünf ursprüngliche Lernkandidaten, zwei Gruppen, davon eine VERIFIED_ON_SAMPLES und eine REFUTED**. Gesamtstatus wechselt gegenüber dem Syntaxmodus von AMBIGUOUS zu VERIFIED_ON_SAMPLES.

**105518 logische Operationseinheiten**, davon **69064 Planung/Erzeugung** und **2392 Normalisierung einschließlich zusätzlicher Samplevergleiche**. Die Enumeration wird nicht verkürzt. Die Demo zeigt beide Modi, die erhaltenen Lücken, Forschungs-Replay, eindeutige Auswahl an x=0 und Auswahl-Replay ohne Quellrun.

## Grenzen

Hinreichende Gleichheitsprüfung für die bestehende kleine rationale Grammatik; keine vollständige Gleichheitserkennung rationaler Nullstellenmengen. Symbolische Zwischenwerte können konservative Ressourcenabbrüche auslösen. Keine zusätzlichen unabhängigen Datenbelege, keine allgemeine Formel aus Samples bewiesen, keine Portfolio-/Experience-Einbindung dieser Strategie und kein vollständiger M2-Abschluss.

[Vertrag](../33-expression-equivalence-contract.md), [Benutzung](../34-expression-equivalence-usage.md).
