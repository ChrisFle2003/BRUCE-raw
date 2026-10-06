# BRUCE Core 0.1 – Benutzung und tatsächlicher Stand

**Fortschreibung:** Seit Core 0.2 sind [dynamische Z-Regelsnapshots und 64-Bit-Filtergruppen](08-z-compatibility-usage.md) mit Forschungsimport und Replay hinzugekommen. Die folgenden Abschnitte beschreiben weiterhin den ursprünglichen Forschungs-/Gridpfad.

## Was jetzt funktioniert

BRUCE verarbeitet neue Zahlenpaare und Suchgrenzen zur Laufzeit. Es besitzt keine fest gespeicherte Sammlung von Aufgaben oder Antworten. Die mitgelieferten Beispieldateien sind austauschbare Demonstrationsdaten, keine eingebaute Wissensbasis.

Die erste Forschungsstrategie untersucht ganzzahlige affine Regeln `a*x+b`. Kandidaten werden nacheinander erzeugt, an den Lernpaaren geprüft und bei Widerspruch ausgeschlossen. Passende Regeln werden anschließend an separat gelieferten Prüfwerten getestet. Die Rückprüfung benutzt `(y-b)/a` einschließlich Restprüfung; für `a=0` wird der konstante Wert geprüft. Sie ist ein anderer arithmetischer Prüfweg, keine zusätzliche unabhängige Datenquelle.

Weitere ausführbare Teile: exakte Cantor-Adressierung mit Inverse, lokales 8×8-Modell, hierarchische Gridpfade bis Tiefe 64, Potenzauswertung eines angeforderten Blattfelds und Replay eines Forschungslaufs.

## Bauen und starten

Voraussetzungen: Linux, GCC oder Clang mit 128-Bit-Zwischenarithmetik, C++20, CMake ≥ 3.20, Ninja und OpenSSL-Entwicklungsbibliothek. Python 3 wird für Demo und Tests verwendet. Die lokale Umgebung wurde mit GCC 14.2, CMake 3.31 und OpenSSL 3.5 gebaut.

Im Projektordner:

```bash
cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
python3 scripts/demo.py
```

Die Demo führt das echte Programm aus, zeigt das gefundene `2*x+3`, prüft einen exportierten Lauf in einem neuen Prozess und demonstriert die Widerlegung mit `(10,24)`. Ihr Export ist temporär und wird entfernt.

## Eigene Daten verwenden

Eine Datei muss exakt den Header `x,y` besitzen. Links steht die Eingabe, rechts die erwartete Ausgabe. Beide Werte sind vorzeichenbehaftete 64-Bit-Ganzzahlen. Dezimalzahlen, zusätzliche Spalten, Kommentare und alternative Spaltennamen werden abgewiesen. Gleiche x-Werte dürfen nicht mehrfach oder zugleich in Lern- und Hold-out-Datei vorkommen.

```text
x,y
-3,15
0,6
5,-9
```

Hier passt beispielsweise `-3*x+6`. BRUCE erhält diese Formel nicht aus dem Beispieltext; es sucht innerhalb des angegebenen Parameterraums.

```bash
./build/bruce research --train examples/affine-training.csv \
  --holdout examples/affine-holdout.csv --min -8 --max 8
```

`--min` und `--max` gelten jeweils für beide Parameter a und b. Der Bereich `[-8,8]` beschreibt 289 Kandidaten. Es wird kein Array dieses vollständigen Suchraums angelegt. Die Suche ist endlich und erschöpfend innerhalb ihrer Grenzen, solange kein Budget vorher greift.

Alle normalen Befehle liefern JSON. Der logische Trace enthält pro besuchtem Kandidaten Parameter und ersten Ausschlussgrund beziehungsweise Ergebnisstatus. Paarindizes im Trace sind nullbasiert. Eingaben und Budget stehen im Replay-Paket. Die Standardausgabe enthält keine unkontrollierten Float-Konvertierungen; große Gridwerte und Adressen werden als Dezimalstrings ausgegeben.

## Ergebnisse richtig lesen

| Status | Aussage |
| --- | --- |
| `CANDIDATE` | genau ein Kandidat passt zu den Lernpaaren; kein Hold-out vorhanden |
| `VERIFIED_ON_SAMPLES` | genau ein Kandidat passt zu Lern- und Hold-out-Paaren und besteht die arithmetische Rückprüfung |
| `AMBIGUOUS` | mehrere gleichrangige Kandidaten bleiben; eine kleinere ID entscheidet keine Wahrheit |
| `REFUTED` | alle zu den Lernpaaren passenden Kandidaten scheitern an den Hold-out-Paaren |
| `NO_CANDIDATE` | kein Treffer in der angegebenen ganzzahligen affinen Familie und ihren Grenzen |
| `INCOMPLETE` | ein Budget oder Abbruch verhindert die abschließende Auswahl |

`general_proof`, `learn_gate` und `publish_gate` bleiben `false`. Das Programm zeigt diagnostische Ergebnisse; eine automatische Wissenspromotion oder Veröffentlichung als allgemeiner Beweis ist nicht implementiert. Auch `VERIFIED_ON_SAMPLES` behauptet keine allgemeine Gültigkeit für neue Daten. Disjunkte x-Werte verhindern die direkte Wiederverwendung von Lernpunkten als Hold-out; die tatsächliche externe Unabhängigkeit deiner Daten kann die Software damit nicht beweisen.

Die zurückgehaltenen Prüfpaare können zwischen mehreren zu den Lernpaaren passenden Kandidaten unterscheiden. Beispiel: `(0,0)` lässt viele Steigungen offen; das Prüfpaar `(1,1)` widerlegt alle außer `a=1`. In diesem Fall sind die Prüfpaare auch Auswahldaten. Sie sind kein unberührter abschließender Test einer bereits eindeutig ausgewählten Formel. Für einen solchen Test wäre ein weiterer, vorher nicht verwendeter Datensatz nötig.

## Budgets und Abbruch

| Option | Standard | Zulässiger Bereich |
| --- | --- | --- |
| `--max-candidates` | 100000 | 1–1000000 |
| `--max-operations` | 1000000 | 1–10000000 |
| `--max-trace` | 100002 | 2–100002 Ereignisse |
| `--max-results` | 128 | 1–4096 Hypothesen |
| `--min`, `--max` | -8, 8 | -1000000–1000000, min ≤ max |

Je Sampleprüfung werden zwei Recheneinheiten reserviert: Vorwärtsrechnung und Rückprüfung. Bei Überlauf kann die zweite entfallen, die Reservierung bleibt konservativ gezählt. Maximal 4096 Lern- und 4096 Hold-out-Paare sind zulässig. Eingabedateien sind auf 1 MiB, Replay-Dateien auf 32 MiB begrenzt. Pfade haben höchstens 64 lokale Schritte.

`Ctrl+C` oder SIGTERM beendet die Suche an der nächsten Kandidatengrenze. Der bis dahin besuchte Teil und die Abbruchposition lassen sich mit `--output` exportieren und identisch replayen. Je Kandidat sind höchstens die begrenzten Samplelisten zu prüfen. Zeitlimit, beliebige Speicherbudgets und parallele Worker sind noch nicht implementiert.

Ein Budgetende wird als normales, unvollständiges Forschungsergebnis mit Exitcode 0 ausgegeben. Bedien-, Datei-, Versions- und Integritätsfehler liefern Exitcode 2 mit JSON-Fehler auf stderr. Automatisierte Nutzer müssen den Ergebnisstatus auswerten, nicht allein den Exitcode.

## Replay ausdrücklich exportieren

```bash
./build/bruce research --train examples/affine-training.csv \
  --holdout examples/affine-holdout.csv --output mein-lauf.bruce
./build/bruce replay mein-lauf.bruce
```

Ohne `--output` entstehen keine Datenbank und kein Runarchiv. Der Export enthält die tatsächlich verwendeten Zahlenpaare, Suchgrenzen, Budgets, Versionskennungen, Ergebnis und logischen Trace. Die ursprünglichen Paar-Dateien werden für Replay nicht benötigt.

Die Datei erhält einen SHA-256-Digest. Replay prüft zuerst den Digest und berechnet anschließend den gesamten Lauf neu. Ein nur neu berechneter Digest versteckt keine falschen Ergebnis-/Tracewerte. Unbekannte Engine-, Regel-, Policy- oder Paketversionen werden abgewiesen. Historische Replay-Unterstützung setzt voraus, dass das entsprechende Engineverhalten weiter verfügbar bleibt; es findet kein stiller Wechsel auf neue Regeln statt.

Der Digest ist **keine Signatur**: Wer das gesamte Aufgabenpaket und den dazu korrekten Lauf ersetzt, kann auch einen passenden Digest erzeugen. V1 bietet Integritätsprüfung und Reproduktion, keine externe Beglaubigung.

Der Export wird über eine private temporäre Datei und eine exklusive Veröffentlichung angelegt. Vorhandene Dateien werden nicht überschrieben. Dateidaten werden synchronisiert; eine vollständige transaktionale Archiv-/Stromausfallgarantie ist erst Bestandteil der späteren Persistenzarbeit.

## Grid und Adressen

```bash
./build/bruce coordinate --x 2 --y 6
./build/bruce coordinate --z 42
./build/bruce grid --path 2,6/4,3
./build/bruce grid --path 2,6 --mode modular --modulus 17
./build/bruce grid --path 2,6 --mode metric
./build/bruce grid --path 0,0 --zero-zero stop
```

Die Gridregel `LEAF_LOCAL_POWER_V1` berechnet ausschließlich `x^y` am letzten Pfadschritt. Elternschritte tragen die Adresse; sie potenzieren nicht rekursiv. In jedem Schritt gilt x,y ∈ [0,7]. Zwei Pfade können denselben Wert und unterschiedliche Identität besitzen.

`Exact` liefert den uint64-Wert innerhalb des Bitlimits. `Modular` liefert den modularen Wert mit breiter Zwischenarithmetik. `Metric` liefert für diese kleine lokale Domäne eine exakt aus dem kleinen Wert bestimmte Bitlänge; es ist noch keine allgemeine Großzahlmetrik ohne Expansion. `0^0` kann 1 bedeuten oder einen lokalen `GridSentinel` auslösen. Die Standardpolicy ist `one`.

Eine Feldanfrage berechnet genau ein Blatt beziehungsweise keinen Wert beim Sentinel. V1 hat noch keinen Cache, keinen Gridbereichsscanner und keine allgemeine benutzerdefinierte Formel. Die affine Forschung generiert ihre eigenen Parameter lazy; die generische Verbindung beider Strategien ist ein nächster Ausbauschritt.

## Was noch nicht implementiert ist

Allgemeine Formelentdeckung, adaptive Suchplanung, Experience Store/Lernen, NanoCell-Runtime, Token-/Sprachverständnis, Cockpit, Phase-DAG und GPU-Ausführung bleiben Roadmap-Arbeit. Regelkonfigurierbare numerische Z-Snapshots und getrennte 64-Bit-Filtergruppen sind inzwischen umgesetzt; externe Evidenzprüfung und sprachliche Rollen stehen weiterhin aus. Details in [Core 0.2](08-z-compatibility-usage.md). [Core 0.3](10-exact-domains-usage.md) ergänzt begrenzte BigInt-/rationale Zahlendomänen für Arithmetik und affine Forschung; Grid und Z-Prädikate behalten ihre bisherigen Zahlenbereiche.

Diese erste Lieferung ist ein ausführbarer Durchstich aus M0/M1 und Teilen von M2. Die vollständigen Roadmap-Meilensteine werden dadurch nicht pauschal als abgeschlossen markiert.
