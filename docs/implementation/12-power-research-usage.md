# BRUCE Core 0.4 – nichtlineare Formeln untersuchen

BRUCE kann jetzt in einer begrenzten Familie selbst nach dem Exponenten suchen: `a*x^p+b`. Die Forschung untersucht lineare, quadratische, kubische und höhere Potenzformeln, prüft passende Kandidaten über einen zweiten Rechenweg und wertet zurückgehaltene Prüfpaare aus. BigInt und rationale Zahlen bleiben exakt. Eine Aufgaben-/Antwortdatenbank wird nicht benötigt.

## Direkt ausprobieren

```bash
cmake --build build
python3 scripts/demo.py
./build/bruce research --strategy power-affine \
  --train examples/power-training.csv \
  --holdout examples/power-holdout.csv
```

Aus `(0,3)`, `(1,5)`, `(2,11)` findet BRUCE **`2*x^2+3`**. Die zurückgehaltenen Paare `(-3,21)` und `(5,53)` stimmen ebenfalls; der Befund lautet `VERIFIED_ON_SAMPLES`. Der Exponent 2 ist keine verdeckte Vorgabe: Standardmäßig untersucht BRUCE 1 bis 4. Die Demo prüft auch den Replay, die Projektion nach Z und ein widersprechendes Prüfpaar.

```bash
./build/bruce research --strategy power-affine \
  --train examples/power-training.csv \
  --holdout examples/power-counterexample.csv
```

Das zusätzliche Paar `(3,99)` ergibt `REFUTED`, da `2*3^2+3=21`.

## Den Suchraum bestimmen

| Option | Verhalten |
| --- | --- |
| `--strategy power-affine` | untersucht `a*x^p+b` |
| `--max-power` | p von 1 bis zur Grenze; Standard 4, maximal 8 |
| `--domain bigint` | Standard für diese Strategie; ganzzahlige Daten und Koeffizienten |
| `--domain rational` | exakte Brüche für Daten und Koeffizienten |
| `--min`, `--max` | Zählerintervall der Koeffizienten; Standard -8 bis 8 |
| `--max-denominator` | bei Brüchen Standard 8, maximal 16; bei BigInt 1 |
| `--max-bits` | Standard 4096, maximal 16384 pro Zahlenkomponente |

Der bisherige Aufruf ohne Strategie bleibt affine Forschung. `--max-power` ist ausschließlich bei `power-affine` erlaubt. `--domain int64` wird dort ausdrücklich abgewiesen; die neue Strategie verwendet das GMP-Zahlenbackend. Alle übrigen Eingabe-, Ergebnis-, Kandidaten-, Operations- und Tracegrenzen entsprechen [Core 0.3](10-exact-domains-usage.md).

Beispiel für eine Bruchformel: Die Lernpaare `(-2,-13/3)`, `(0,-1/3)`, `(3/2,65/48)` passen zu `(1/2)*x^3-(1/3)`. Mit eigener CSV-Datei:

```bash
./build/bruce research --strategy power-affine --domain rational \
  --train meine-bruchpaare.csv --holdout meine-pruefpaare.csv \
  --min -2 --max 2 --max-denominator 3 --max-power 4
```

Der Suchraum umfasst keine beliebigen Polynome: `x^2+x+1`, mehrere unabhängige Potenzterme, variable Exponenten, negative Exponenten und frei zusammengesetzte Ausdrücke liegen außerhalb dieser Strategie.

## Kandidaten und Mehrdeutigkeit

Die Reihenfolge ist fest: aufsteigender Exponent, dann a-Zähler/Nenner und b-Zähler/Nenner. Brüche werden nur gekürzt erzeugt. Konstante Funktionen mit `a=0` entstehen ausschließlich bei `p=1`; eine Konstante wird dadurch nicht mehrfach als scheinbar konkurrierende Formel gezählt.

Für die Standard-Ganzzahlgrenzen sind es 1105 tatsächlich erzeugte Kandidaten: 289 bei p=1 und jeweils 272 bei p=2,3,4. Die Rohslotzahl `raw_parameter_combinations=1156` enthält die übersprungenen doppelten Konstanten. Rationale Rohslots können zusätzlich ungekürzte Brüche enthalten, die ebenfalls übersprungen werden. Der Raum wird nicht vorab als Kandidatenliste materialisiert.

`a_index` kodiert hier Exponent und a-Slot: `(p-1)*slots+a_slot`. `b_index` ist der b-Slot, `id=a_index*slots+b_index`. Im JSON enthält jede Hypothese ihren `power` und die vollständige Formel. Diese Koordinaten werden auch beim Z-Import verwendet.

Passen beispielsweise nur die Paare `(0,0)` und `(1,1)` zu den Eingaben, bleiben `x`, `x^2`, `x^3` und `x^4` gleichrangig. Der Status ist **`AMBIGUOUS`**. BRUCE bevorzugt weder eine kleinere ID noch heimlich den kleinsten Exponenten. Weitere passende Paare können die Auswahl einschränken; Daten, die zur Auswahl beitragen, sind danach keine unabhängige abschließende Bewertung mehr.

## Exakte Prüfung und Stopgründe

Vorwärts wird `x^p` durch wiederholtes Quadrieren berechnet, anschließend mit a multipliziert und b addiert. Die Gegenprüfung beginnt bei a und multipliziert p-mal nacheinander mit x; sie übernimmt keinen Potenzwert aus der Vorwärtsrechnung. Danach addiert sie b und vergleicht mit der erwarteten Ausgabe. Beide Wege verwenden exakte Arithmetik mit den konfigurierten Grenzen.

Konstante Formeln brauchen keine Auswertung einer unnötigen großen Potenz. Für alle anderen Kandidaten gelten Grenzen auch dann, wenn ein späterer Rechenschritt das Ergebnis wieder verkleinern würde. Eine Begrenzung in Vorwärts- oder Gegenprüfung stoppt den gesamten Lauf mit `INCOMPLETE / IntegerBitLimit`. Bereits passende Kandidaten bleiben als Teilbefunde sichtbar, berechtigen aber nicht zu einem abschließenden Gesamtstatus.

Pro Sampleprüfung werden konservativ `3*p+4` logische Operationseinheiten reserviert. Das begrenzt die Arbeit beider Prüfwege, misst aber keine internen GMP-Instruktionen. Genau ausreichende Budgets erlauben den Abschluss; unzureichende Budgets erzeugen `INCOMPLETE`. Abbruch zwischen Kandidaten und seine logische Position sind replaybar.

Die bisherigen Status gelten: `CANDIDATE`, `VERIFIED_ON_SAMPLES`, `AMBIGUOUS`, `REFUTED`, `NO_CANDIDATE`, `INCOMPLETE`. `NO_CANDIDATE` bezieht sich nur auf die vollständig untersuchte Familie samt Koeffizienten- und Exponentengrenzen. Die Gates für allgemeinen Beweis, Lernen und Veröffentlichung bleiben geschlossen.

## Export, Replay und Regelauswahl

```bash
./build/bruce research --strategy power-affine \
  --train examples/power-training.csv --holdout examples/power-holdout.csv \
  --output potenz-forschung.bruce
./build/bruce replay potenz-forschung.bruce
./build/bruce compatibility --rules examples/power-selection.rules \
  --from-run potenz-forschung.bruce --at -3 --output potenz-auswahl.bruce
./build/bruce replay potenz-auswahl.bruce
```

Die Projektion setzt tatsächlich x=-3 in die gefundene Potenzformel ein und erzeugt den Wert 21. Die Z-Kontexte heißen `power-bigint-v1` beziehungsweise `power-rational-v1`. Z-Prädikate bleiben int64-basiert: gebrochene oder zu große Projektionswerte werden abgewiesen, niemals gerundet oder still weggelassen. `SELECTED` bezeichnet weiterhin die Regelentscheidung, keinen mathematischen Beweis.

Das neue Paket `BRUCE_POWER_BUNDLE_V1` verwendet Engine `bruce-power-v1`, Regel `power-affine-v1` und Policy `power-samples-v1`. Es hält Exponentengrenze, Zahlenbereich, Samples, Budgets, Abbruchposition, Befund und Trace fest. Replay berechnet den gesamten Lauf neu. Ein Auswahlpaket enthält den vollständigen Ursprung. Originaldateien werden für Replay nicht benötigt. Historische Pakete aus Core 0.1, 0.2 und 0.3 bleiben unterstützt.

Nur `--output` speichert Daten; ein bestehender Export wird nicht überschrieben. SHA-256 und Neuberechnung prüfen innere Konsistenz, keine externe Herkunft oder Allgemeingültigkeit.

## Weitere Roadmap

Die begrenzte Potenzformelsuche ist implementiert. [Core 0.5](14-adaptive-search-usage.md) ergänzt inzwischen datenabhängige Planung innerhalb dieser Familie: exakte Koeffizientenableitung oder vollständige Suche. Allgemeine Formelsynthese, Planung über mehrere Strategien, Experience Store/Lernen, Token-/Sprachverständnis, NanoCell, Cockpit, Phase-DAG und GPU-Ausführung bleiben offen. BigInt-Werte im Grid und in Z-Prädikaten stehen weiterhin aus. Der vollständige M2-/Gesamtprojektabschluss wird daraus nicht abgeleitet.
