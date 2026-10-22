# Scaled Exponential Grid Rule V1 — Design

**Status: IMPLEMENTIERT UND BELEGT** — Der begrenzte V1-Vertrag ist durch die Task-1-/Task-2-Implementierung und deren unabhängige Reviews sowie die Abschlussprüfung vom 16.10.2026 belegt: 12/12 relevante CTest-Suiten, CLI-Verifier mit 167 Aufrufen in vollständiger temporärer Quellkopie und lokale Dokumentationslinks ohne Fehler. Der detaillierte lokale Nachweis liegt unter `runs/scaled-exponential-grid-rule-v1/reports/final-review.md`; die unabhängige Gesamtprüfung folgt separat. Die direkte Grid-Stufeneingabe bleibt 0..7; die beschriebene UInt64-Grenze 57 erweitert diesen Eingabebereich nicht.

## Auftrag

BRUCE soll die vom Nutzer bestätigte diskrete Exponentialleiter exakt, begrenzt und replaybar ausführen:

\[
h(s)=100(2^s-1)
\]

`s` ist eine nichtnegative Stufe, nicht freie Sprache und nicht ein physikalischer Messwert. Die Zahl `100` ist in V1 eine absichtlich abstrakte BRUCE-Skala. Die Regel modelliert daher keine reale Blasenphysik; eine spätere physikalische Variante müsste Druck, Dichte, Gravitation, Temperatur und Einheiten ausdrücklich vertraglich führen.

## Ausgangslage

**IMPLEMENTIERT UND BELEGT:** Der Grid-Kern berechnet ausschließlich die Blattauswertung `x^y` für lokale 8×8-Koordinaten und `uint64`-Werte. Pfadtiefe adressiert nur den Raum; sie beeinflusst den berechneten Blattwert nicht. `GridSpec` fixiert `LEAF_LOCAL_POWER_V1`; Exact und Modular nutzen begrenzte ganzzahlige Potenzen, während `0^0` lokal als Wert oder Stop-Sentinel behandelt wird.

**IMPLEMENTIERT UND BELEGT:** Die allgemeine Ausdrucksgrammatik akzeptiert Potenzen nur mit einem literalen Exponenten `2..8`. Sie kann `2^s` mit variablem Exponenten daher nicht als Formel ausführen. Die Power-Affine-Familie hat die Form `a*x^p+b`, ebenfalls nicht `a*(b^x-1)`.

## Geltungsbereich

Die Erweiterung führt eine zweite feste Grid-Wertregel ein:

```text
SCALED_EXPONENTIAL_V1
result = 100 * (2^stage - 1)
```

- Der bestehende CLI-Befehl `bruce grid` erhält eine explizite Auswahl der Regel; Standard bleibt unverändert `LEAF_LOCAL_POWER_V1`.
- Für die neue Regel bestimmt der Blattwert `y` die Stufe `stage`; `x` bleibt Teil der Adresse und der Trace zeigt beide Werte.
- Gültige Stufen sind `0..57`. Damit bleibt das Ergebnis in `uint64`:
  `100 * (2^57 - 1) = 14_411_518_807_585_587_100`.
- Die lokale 8×8-Grenze bleibt bestehen. V1 kann damit unmittelbar die Stufen `0..7` darstellen; eine spätere allgemeine Stufenquelle ist nicht Teil dieses Vertrags.
- Exact, Metric und Modular erhalten klar definierte Semantik. Der öffentliche Grid-Bundle-Replay bleibt kanonisch.

Nicht Teil von V1 sind freie Formeln, veränderliche Basen oder Skalen, negative oder gebrochene Stufen, BigInt im Grid, physikalische Blasenmodelle, freie Code-Plugins, GPU-/Netzwerkausführung oder eine allgemeine Transition-Engine.

## Schnittstelle und Semantik

Die Auswahl soll eine klare, geschlossene CLI-Option verwenden:

```text
bruce grid --path X,S --rule leaf-local-power|scaled-exponential-v1
```

`--rule` ist optional. Ohne Option bleibt die bestehende Regel aktiv und ihre Ausgabe bleibt bytekompatibel.

| Modus | `leaf-local-power` | `scaled-exponential-v1` |
|---|---|---|
| `exact` | `x^y` innerhalb `max_bits` | `100*(2^s-1)` innerhalb `max_bits` |
| `metric` | exakte lokale Potenz, Bitlänge | exakter Wert nur zur sicheren Größenbestimmung, Bitlänge |
| `modular` | `x^y mod M` | `(100 mod M) * ((2^s mod M - 1) mod M) mod M` |

Der skalierte Ausdruck darf nicht über einen ungeschützten Zwischenwert berechnet werden. Exact und Metric verwenden die vorhandene begrenzte Potenzrechnung und eine explizite begrenzte Multiplikation. Modular benutzt die vorhandene modulare Potenzrechnung und eine `__uint128_t`-basierte modulare Multiplikation.

Bei `--zero-zero stop` wirkt der bestehende Sentinel weiterhin ausschließlich für die Adresse `(0,0)`, unabhängig von der gewählten Regel. Die skalierte Regel selbst besitzt keinen zusätzlichen Stop-Sonderwert.

## Datenmodell, Trace und Replay

`GridTask`, `GridRun`, `GridSpec`, Grid-Cache-Key, Bundle-Payload und Replay erhalten die kanonische Regelkennung. Eine nicht bekannte Regel wird kontrolliert abgelehnt.

Für die neue Regel enthält die sichtbare Ausgabe mindestens:

```text
rule = SCALED_EXPONENTIAL_V1
stage = y
base = 2
scale = 100
formula = 100*(2^stage-1)
```

Der Trace behält Pfadvalidierung, Blattkoordinate, Konfiguration und Evaluation/Stop bei und ergänzt die deterministische Regel- und Stufeninformation. Replay vergleicht den kompletten Bundle-Payload wie bisher; ein manipuliertes Regelkennzeichen, eine manipulierte Stufe oder ein anderes Ergebnis muss den bestehenden Evidenzvergleich brechen.

Der Cache-Key muss die Regelkennung enthalten. Gleiche Adresse unter unterschiedlichen Regeln darf keinen gemeinsamen Cacheeintrag ergeben.

## Fehler, Grenzen und Kompatibilität

- Nicht erlaubte Regelkennungen: kontrollierter Fehler `InvalidGridRule`.
- Nicht erlaubte Regel-/Modus-Kombinationen: kein stiller Fallback.
- Exact-Ergebnis über `max_bits`: derselbe kontrollierte Bitgrenzenfehler wie für die bestehende Potenz.
- Lokale Adressen außerhalb `0..7`, ungültige Moduli und Bundle-Manipulationen behalten ihre bestehenden Fehlerverträge.
- V1 erweitert weder die Addressierung noch das existierende `LEAF_LOCAL_POWER_V1`; alle bisherigen Grid-, Batch-, Resume- und Research-Ausgaben bleiben für die Standardregel unverändert.

## Abnahme

1. `scaled-exponential-v1` liefert für Stufe `0..7` exakt `0, 100, 300, 700, 1500, 3100, 6300, 12700`.
2. Exact, Metric und Modular stimmen für repräsentative Stufen und Moduli mit unabhängigen Referenzrechnungen überein.
3. Ein zu niedriges `max_bits` lehnt den Wert kontrolliert ab.
4. Standard-Grid-Ausgaben und Bundles bleiben unverändert.
5. Neue Bundles lassen sich wiedergeben; jeder relevante Manipulationsfall wird verworfen.
6. Batch und Resume übernehmen Regel, Cache-Trennung und Replay korrekt.
7. CLI-Hilfe, Handbuch und Beispiele beschreiben nur den tatsächlich implementierten Vertrag.

## Architekturgrenze

Die Regel ist eine kleine, deterministische Mathematik- und Grid-Erweiterung. Sie ist kein Beweisverfahren, kein physikalischer Simulator und kein NanoCell-Programm. NanoCell V1 behält seinen vorhandenen Byte-/Bitopcode-Satz; die skalierte Exponentialrechnung bleibt im Grid-/Math-Layer.
