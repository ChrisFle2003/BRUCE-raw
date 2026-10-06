# Polynombrüche mit Definitionsbereich vergleichen – Core 0.26

Bruce kann jetzt zwei eingegebene Polynombrüche exakt als partielle Funktionen über **rationalen Zahlen** vergleichen. Die neue Funktion heißt `compare`. Sie berücksichtigt auch Lücken, die beim Kürzen eines Bruchs unsichtbar werden.

## Beispiel: Ein scheinbarer Ausschluss ohne rationale Nullstelle

```bash
cmake --build build
./build/bruce compare \
  --left-numerator 1,0,1 --left-denominator 1,0,1 \
  --right-numerator 1 --right-denominator 1 \
  --output vergleich.bruce
./build/bruce replay vergleich.bruce
```

Die Zahlen stehen für die Koeffizienten von der konstanten bis zur höchsten Potenz: `1,0,1` bedeutet `1+x²`. Das Beispiel vergleicht `(1+x²)/(1+x²)` mit `1` und liefert `PROVEN_EQUIVALENT`: gleiche Werte und gleicher Definitionsbereich, weil `1+x²` keine rationale Nullstelle hat.

Der Export enthält Eingaben, Grenzen, mathematische Teilbefunde, Rechenspur und Prüfsumme. Replay berechnet alles erneut. Ohne `--output` wird keine Datei angelegt; vorhandene Ziele werden nicht überschrieben.

## Eine echte Lücke bleibt erhalten

```bash
./build/bruce compare \
  --left-numerator 0,1 --left-denominator 0,1 \
  --right-numerator 1 --right-denominator 1
```

`0,1` bedeutet `x`. Das Ergebnis für `x/x` gegenüber `1` lautet `PROVEN_DIFFERENT`. Die gekürzten Wertefunktionen sind gleich (`value_relation=EQUAL`), die Definitionsbereiche verschieden (`domain_relation=DIFFERENT`). Im Zertifikat steht `witness="0"`, `excluded_side="left"`. Bruce hat diese Stelle in beide Ausschlusspolynome eingesetzt: Links ist sie verboten, rechts erlaubt.

Auch gemeinsame Lücken werden korrekt berücksichtigt. `(x³+x)/(x³+x)` und `x/x` sind auf Q gleich: Beide schließen genau `0` aus. Der gemeinsame Faktor `x` wird beim Bereichsvergleich abgetrennt; der verbleibende Faktor `x²+1` hat keine rationale Nullstelle.

## Was das Ergebnis aussagt

| Beispiel gegenüber `1` | Ergebnis auf Q |
| --- | --- |
| `(x²+1)/(x²+1)` | Gleich |
| `(x²−2)/(x²−2)` | Gleich: ±√2 sind keine rationalen Eingaben |
| `x/x` | Verschieden: linke Lücke bei 0 |
| `(x²−1)/(x²−1)` | Verschieden: linke Lücken bei ±1 |
| `(x³−2)/(x³−2)` | Noch unentschieden durch diese begrenzte Prüfung |

Gemeinsame Faktoren und Wiederholungen werden zuerst entfernt. Verbleibende konstante, lineare und quadratische Polynome entscheidet Bruce exakt. Höhere Restgrade können `UNDECIDED` liefern. Das ist keine Behauptung, dass das mathematische Problem unlösbar wäre.

Der Vergleich findet ausschließlich über Q statt. Beispielsweise wären `(x²−2)/(x²−2)` und `1` über den reellen Zahlen wegen der irrationalen Lücken verschieden. Ein Nenner, der identisch null ist, beschreibt einen leeren Definitionsbereich. Zwei solche partiellen Funktionen gelten als gleich.

Hier geht es um einen algebraischen Nachweis für **die beiden eingegebenen Formeln**. Die bisherige Forschung aus Zahlenpaaren bleibt `VERIFIED_ON_SAMPLES`; dieser neue Befehl beweist keine vermutete Gesetzmäßigkeit hinter Daten.

## Eigene Eingaben und Grenzen

Alle vier Koeffizientenlisten müssen angegeben werden. Brüche wie `1/2,-3/4,1` sind erlaubt. Pro Liste sind höchstens 65 Einträge zugelassen, also höchstens Grad 64. Der Grad der Eingabe ist von der Grenze für die verbleibende Nullstellenprüfung zu unterscheiden.

```bash
./build/bruce compare \
  --left-numerator 1/2,1/2 --left-denominator 1/2 \
  --right-numerator 1,1 --right-denominator 1 \
  --max-bits 256 --max-operations 10000
```

Dieses Beispiel bestätigt `(1/2+x/2)/(1/2) = 1+x` mit gleichem Definitionsbereich.

- `--max-bits`: 1 bis 16384, standardmäßig 4096; begrenzt Koeffizienten und Zwischenrechnungen.
- `--max-operations`: 1 bis 1000000, standardmäßig 1000000; begrenzt die logische mathematische Arbeit.
- Eine erreichte Grenze in der Rechnung liefert `INCOMPLETE` und einen Stopgrund. Ungültige oder schon zu große Eingaben werden als Eingabefehler zurückgewiesen.
- Ctrl+C beendet den Vergleich an einer logischen Rechengrenze. Der Export enthält die Abbruchposition für Replay.

Die Bitgrenze schränkt Ressourcen ein; sie macht aus Q keine endliche Stichprobenmenge. Logische Rechenschritte sind keine CPU-Zeitangabe. Die Ausgabe zeigt höchstens vier abgeschlossene Phasen. Bei `INCOMPLETE` können einzelne Teilprüfungen bereits fertig sein; der Gesamtvergleich bleibt unvollständig.

```bash
ctest --test-dir build --output-on-failure
python3 scripts/demo.py
```

## Weitere Roadmap

Der eigenständige Vergleich ist implementiert. Seine Einbindung in Ausdruckssuche, Portfolio und Erfahrungsplanung sowie allgemeinere Nullstellenentscheidungen bleiben offen. Bestehende Suchläufe verwenden ihre bisherigen Gleichheitsregeln und Formate. Weitere Suchoptimierung, allgemeinere Operatoren, externe Evidenz, Experience-Korrekturketten, Sprache, NanoCell, Cockpit, Phase-DAG, GPU-Ausführung und BigInt im Grid/Z bleiben weitere Pakete. Kein vollständiger M2-/Gesamtprojektabschluss.

[Vertrag](55-fraction-comparison-contract.md) · [Abnahme](acceptance/fraction-comparison-v1-2026-09-22.md)

Nächster Ausbau: [Core 0.27 – genauerer Bereichsvergleich direkt in der Ausdruckssuche](58-q-expression-usage.md).
