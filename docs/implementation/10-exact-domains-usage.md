# BRUCE Core 0.3 – große Ganzzahlen und exakte Brüche

BRUCE rechnet jetzt mit großen Ganzzahlen (`bigint`) und gekürzten Brüchen (`rational`). Beide Domänen sind in die bestehende affine Forschung eingebunden: eigene Zahlenpaare einlesen, Koeffizienten erzeugen, vorwärts und rückwärts prüfen, zurückgehaltene Paare prüfen und den gesamten Lauf reproduzieren. Eingaben werden zur Laufzeit verarbeitet. Ein Antwortspeicher ist weiterhin nicht erforderlich.

## Ausprobieren

Voraussetzungen zusätzlich zum bisherigen C++20-/OpenSSL-Build: GMP mit C++-Headern und pkg-config (unter Debian: `libgmp-dev`, `pkg-config`).

```bash
cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
python3 scripts/demo.py
```

Die Demo enthält Bruchrechnung, die Formel `(1/2)*x+(1/3)` und eine Forschung mit Eingaben ab `10^100`. Sie führt Replay in neuen Prozessen aus und entfernt ihre temporären Dateien anschließend.

```bash
./build/bruce calculate --domain rational --op add --left 1/3 --right 1/6
./build/bruce calculate --op pow --left 2 --right 256
./build/bruce calculate --op divmod --left -7 --right 3
./build/bruce calculate --op modpow --left 2 --right 256 --modulus 97
```

Ergebnisse: `1/2`, exakt `2^256`, Quotient `-3` und Rest `2`, sowie modularer Wert `61`. `calculate` verwendet ohne Domänenangabe `bigint`. Zahlen erscheinen im JSON als Strings; keine Umwandlung in Gleitkommazahlen.

## Zahlen und Rechenregeln

Erlaubt sind dezimale Ganzzahlen wie `-123` und in der rationalen Domäne Brüche wie `-2/3`. `2/4` wird zu `1/2`, `-2/-4` ebenfalls zu `1/2`, `0/-7` zu `0`. Dezimalpunkte, Exponentialschreibweise, Pluszeichen und Leerzeichen innerhalb der Zahl sind nicht erlaubt. Nenner null und Division durch null ergeben einen Fehler.

| Operation | Verhalten |
| --- | --- |
| `add`, `sub`, `mul` | exakte Addition, Subtraktion und Multiplikation |
| `div` | in `rational` exakter Bruch; in `bigint` muss der Rest null sein |
| `divmod` | nur `bigint`; `a=q*b+r`, immer `0 <= r < abs(b)` |
| `pow` | nichtnegativer ganzzahliger Exponent bis uint64-Maximum; `0^0=1` |
| `modpow` | nur `bigint`, positiver `--modulus`, nichtnegativer uint64-Exponent |

`--max-bits` begrenzt jede rohe Eingabekomponente und jedes gekürzte Rechenergebnis: standardmäßig 4096, zulässig 1–16384 Bits. Vorzeichen zählt nicht als Bit. Überlange Textdarstellungen werden schon vor der Zahlenkonvertierung abgewiesen, auch bei vielen führenden Nullen. Ein rationaler Wert hat getrennte Grenzen für Zähler und Nenner. Vorübergehende Produkte dürfen größer sein, bleiben durch die begrenzten Operanden beschränkt. Die modulare Potenz reduziert nach jedem Schritt und benötigt keine ausgeschriebene Riesenpotenz.

Die Grenzen gelten auch für Zwischenergebnisse der affinen Rechnung und Gegenprüfung. Beispielsweise darf ein zu großes `a*x` nicht erst durch späteres Addieren von `b` wieder kleiner werden. `calculate` liefert bei einem überschrittenen Limit einen JSON-Fehler mit Exitcode 2.

## Forschung mit Brüchen

```bash
./build/bruce research --domain rational \
  --train examples/rational-training.csv \
  --holdout examples/rational-holdout.csv \
  --min -2 --max 2 --max-denominator 3 \
  --output bruch-forschung.bruce
./build/bruce replay bruch-forschung.bruce
```

Lernpaare: `(0,1/3)`, `(2,4/3)`, `(7/3,3/2)`. Zurückgehaltenes Prüfpaar: `(1/7,17/42)`. BRUCE erzeugt die passende Formel `(1/2)*x+(1/3)` und meldet `VERIFIED_ON_SAMPLES`.

Die Familie bleibt `a*x+b`. Für jeden Koeffizienten werden reduzierte Brüche `n/d` mit ganzzahligem `n` in `[min,max]` und `d` von 1 bis `max-denominator` untersucht. Das Zählerintervall darf höchstens 4096 Werte enthalten; seine Position kann eine große Ganzzahl sein. Die Nennergrenze ist standardmäßig 8, maximal 16. Beispiel: `--min -2 --max 2 --max-denominator 3` erzeugt elf verschiedene Koeffizienten, somit 121 affine Kandidaten. `2/2` wird nicht nochmals neben `1` erzeugt; `0` entsteht nur als `0/1`.

Die Erzeugung erfolgt lazy, geordnet nach Zähler und anschließend Nenner. Die Felder `a_index` und `b_index` bezeichnen Rohslots; die Kandidaten-ID ist `a_index * slots + b_index`. Nicht gekürzte Slots werden übersprungen. Deshalb können IDs Lücken haben und `raw_parameter_combinations` größer als die Zahl ausgeführter Kandidaten sein.

## Forschung mit großen Ganzzahlen

```bash
./build/bruce research --domain bigint --train meine-grossen-paare.csv \
  --holdout meine-pruefpaare.csv --min -8 --max 8 --max-bits 4096
```

Dateiformat weiterhin `x,y`, danach pro Zeile Eingabe und erwartete Ausgabe. In dieser Domäne sind auch `a` und `b` ganzzahlig; die Nennergrenze ist 1. Der bisherige Aufruf ohne `--domain` verwendet weiterhin den ursprünglichen int64-Pfad. Das alte `grid`-Kommando und Z-Prädikate behalten ihre bisherigen Zahlentypen.

Je Lern-/Hold-out-Datei sind höchstens 4096 Paare und 1 MiB Text erlaubt; die kanonischen Paare zusammen sind zusätzlich auf 1 MiB begrenzt. Kanonisch gleiche x-Werte gelten als doppelt: `1/2` und `2/4` dürfen weder innerhalb einer Datei noch verteilt auf Lernen und Hold-out vorkommen.

## Aussagekraft und Grenzen

Jedes passende Lernpaar muss sowohl `a*x+b=y` als auch die getrennte Rückrechnung erfüllen; für `a=0` wird die konstante Ausgabe geprüft. Hold-out-Paare werden erst danach geprüft. Ein Widerspruch auf Hold-out widerlegt den betreffenden Kandidaten.

| Status | Aussage |
| --- | --- |
| `CANDIDATE` | ein verbleibender Kandidat passt zu den Lernpaaren; kein Hold-out angegeben |
| `VERIFIED_ON_SAMPLES` | ein verbleibender Kandidat besteht auch die angegebenen Hold-out-Prüfungen |
| `AMBIGUOUS` | mehrere Kandidaten bleiben gleichrangig |
| `REFUTED` | alle gespeicherten Lernkandidaten scheitern an Hold-out |
| `NO_CANDIDATE` | kein Kandidat dieser vollständig untersuchten Familie passt zu den Lernpaaren |
| `INCOMPLETE` | Abbruch oder Budget-/Zahlenlimit verhindert einen abschließenden Befund |

`VERIFIED_ON_SAMPLES` ist kein allgemeiner Beweis. Werden mehrere Lernkandidaten durch Hold-out ausgewählt, sind diese Daten Teil der Auswahl; für eine weitere unabhängige Bewertung braucht es neue zurückgehaltene Daten. `general_proof`, `learn_gate` und `publish_gate` bleiben `false`.

Wird während der Suche ein Zahlenlimit erreicht, lautet der Gesamtbefund **`INCOMPLETE / IntegerBitLimit`**. Der Kandidat wird nicht als mathematisch widerlegt behandelt. Eine zu große oder ungültige Eingabe wird dagegen schon vor Beginn als Fehler zurückgewiesen.

| Forschungsbudget | Standard | Maximum |
| --- | ---: | ---: |
| `--max-candidates` | 100000 | 1000000 |
| `--max-operations` | 1000000 | 10000000 |
| `--max-trace` | 100002 | 100002 |
| `--max-results` | 128 | 128 |
| `--max-trace-bytes` | 1048576 | 4194304 |

Mindestwerte: 1 für Kandidaten/Operationen/Ergebnisse, 2 für Traceereignisse, 256 für Tracebytes. Eine Sampleprüfung reserviert vier Operationseinheiten; das ist ein logisches Budget, keine Messung interner GMP-Instruktionen. `trace_bytes` zählt die Textbytes der Traceereignisse, ohne JSON-Struktur. Vor einer Kandidatenprüfung wird Platz für das Ereignis und 256 Bytes für Fehler-/Abschlussmeldung reserviert. Die Trace nennt den ersten Widerspruch, die erreichte Prüfphase oder den Stopgrund. `Ctrl+C` wird zwischen Kandidaten umgesetzt und die logische Abbruchposition exportiert.

## Replay und Z-Kompatibilität

Nur `--output` speichert einen Lauf. Vorhandene Dateien werden niemals überschrieben. Das neue Format `BRUCE_EXACT_BUNDLE_V1` enthält Domäne, Zahlen-/Suchgrenzen, kanonische Samples, Budgets, Versionen, gegebenenfalls Abbruchposition und vollständiges Ergebnis. Replay prüft SHA-256, führt die Suche erneut aus und vergleicht den gesamten kanonischen Inhalt. Ursprüngliche CSV-Dateien sind dafür nicht nötig. Historische Core-0.1- und Core-0.2-Pakete bleiben lesbar.

Die bestehende Z-Kompatibilität kann vollständige neue Forschungsexporte mit `--from-run` konsumieren. `--at` bleibt int64; der berechnete Wert muss exakt ganzzahlig und als int64 darstellbar sein. Nicht darstellbare oder gebrochene Projektionen werden ausdrücklich zurückgewiesen. Kein Kandidat wird dafür gerundet oder still entfernt. Bei neuen Domänen heißen die Kontexte `affine-bigint-v1` beziehungsweise `affine-rational-v1`; die Koordinaten sind die Rohslot-Indizes. Exportierte Auswahlläufe enthalten ihren kompletten Ursprung und prüfen die Projektion beim Replay erneut.

## Weitere Roadmap

Dieses Paket erweitert den exakten Zahlenbereich und die affine Forschung. [Core 0.4](12-power-research-usage.md) ergänzt inzwischen die begrenzte nichtlineare Suche `a*x^p+b` mit selbst untersuchten Exponenten. Allgemeine Formelsynthese, adaptive Suchplanung, Experience Store/Lernen, Token-/Sprachverständnis, NanoCell, Cockpit, Phase-DAG und GPU-Ausführung bleiben offen. BigInt-Werte im Grid und in Z-Prädikaten sind ebenfalls noch kein Bestandteil dieser Lieferung. Der vollständige M2-/Gesamtprojektabschluss wird daraus nicht abgeleitet.
