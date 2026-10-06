# Wiederholte Grid-Abfragen – Core 0.37

Bruce kann mehrere Grid-Adressen in einem Auftrag abfragen und wiederholte Adressen aus einem begrenzten, nur während des Auftrags bestehenden Cache bedienen. Die Ergebnisse und Cacheentscheidungen lassen sich exportieren und wiederholen.

## Gleicher Wert, verschiedene Adressen

```bash
cmake --build build
./build/bruce grid-batch --paths '2,6;02,06;4,3;1,1/2,6;2,6' --output grid-stapel.bruce
./build/bruce replay grid-stapel.bruce
```

Alle fünf Abfragen liefern 64. Bruce berechnet drei verschiedene Adressen und erkennt zwei Wiederholungen. `02,06` ist dieselbe Adresse wie `2,6`; `4,3` und `1,1/2,6` bleiben eigenständig. Die ursprüngliche Schreibweise jeder Anfrage bleibt in ihrer Ausgabe erhalten. Die Anführungszeichen um die Pfadliste sind erforderlich, damit die Shell die Semikolons nicht als Befehlsseparatoren interpretiert.

## Cache begrenzen oder abschalten

```bash
./build/bruce grid-batch --paths '2,6;2,6' --cache-bytes 0
./build/bruce grid-batch --paths '2,6;2,6' --cache-bytes 65536
```

Ohne Cache werden zwei Blattwerte berechnet; mit ausreichendem Cache einmal. Das Budget liegt zwischen 0 und 1048576 Bytes; höchstens 64 Einträge werden gehalten. Es zählt kanonische Schlüssel- und Ergebnisbytes, nicht den gesamten Arbeitsspeicher des Programms. Bei Platzmangel verdrängt Bruce die ältesten Einträge. Ein zu großer einzelner Eintrag wird ohne Speicherung ausgewertet.

Der Cache startet bei jedem Stapel leer. Es wird keine Wissensdatenbank benötigt oder angelegt. Exportdateien entstehen nur mit `--output` und werden niemals überschrieben.

## Rechenmodi und lokale Stopps

```bash
./build/bruce grid-batch --paths '2,6;2,6;4,3' --mode modular --modulus 7
./build/bruce grid-batch --paths '2,6;2,6' --mode metric
./build/bruce grid-batch --paths '0,0;2,6;0,0;2,6' --zero-zero stop --output grid-stopps.bruce
./build/bruce replay grid-stopps.bruce
```

Modular ergibt jede der obigen Potenzen 1, im Metrikbeispiel beträgt die Bitlänge 7. Das letzte Beispiel meldet zwei lokale Stopps, berechnet 64 einmal und verwendet den Wert bei der Wiederholung erneut. Der Stopp einer Anfrage beendet nicht den Stapel.

Im äußeren Feld `evaluations` stehen die tatsächlich neu berechneten Werte. Die eingebetteten Einzelergebnisse behalten ihr bisheriges Format: Dort bedeutet `evaluations: 1`, dass ein Blattwert vorliegt, auch wenn der Stapel ihn aus seinem Cache bezogen hat. `cache_event` zeigt die Herkunft.

## Grenzen und nächste Roadmap

1–1024 Abfragen pro Stapel, höchstens 64 Schritte pro Pfad, lokale Koordinaten 0–7 und begrenzte Eingabebytes. Ungültige Aufträge erzeugen keinen Export. Die Prüfung eines Replay bestätigt den reproduzierten Auftrag, keine externe Herkunft und keinen allgemeinen mathematischen Beweis.

Die versionierte Identität und Cacheisolation sind für das bestehende lokale UInt64-Grid implementiert. Die Einbindung in Forschungsaufträge, allgemeinere Grid-Formen, Expansion, BigInt im Grid/Z und umfassende Aststeuerung bleiben offen. Dies ist kein vollständiger M2-Abschluss.

[Vertrag](78-grid-cache-contract.md) · [Abnahme](acceptance/grid-cache-v1-2026-09-22.md)
