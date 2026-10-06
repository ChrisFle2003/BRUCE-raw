# Höhere Nennergrade exakt vergleichen – Core 0.29

Bruce kann jetzt bei manchen Nennern höheren Grades nachweisen, dass sie keine rationalen Nullstellen haben. Dafür gibt es eine optionale Erweiterung des eigenständigen Formelvergleichs.

## Beispiel: ein bisher offener Vergleich wird entschieden

```bash
cmake --build build
./build/bruce compare \
  --left-numerator '-2,0,0,1' --left-denominator '-2,0,0,1' \
  --right-numerator 1 --right-denominator 1 \
  --domain-policy modular --output modular-vergleich.bruce
./build/bruce replay modular-vergleich.bruce
```

Koeffizienten stehen aufsteigend: `-2,0,0,1` bedeutet x³−2. Der Vergleich prüft `(x³−2)/(x³−2)` gegen `1` und liefert `PROVEN_EQUIVALENT` über den rationalen Zahlen. Ohne die neue Option liefert derselbe Vergleich weiterhin `UNDECIDED`.

Im Zertifikat steht `left_modular_prime: 7` mit den Werten `[5,6,6,4,6,4,4]`. Das sind alle Werte von x³−2 für die sieben Restklassen modulo 7. Keine davon ist null. Weil eine rationale Nullstelle hier auch eine Nullstelle modulo 7 ergeben müsste, beweist diese vollständige Tabelle das Fehlen rationaler Nullstellen. Der Nenner schließt also keine rationale Eingabe aus.

Über den reellen Zahlen hätte der ursprüngliche Bruch dagegen ein Loch bei der Kubikwurzel von 2. Der ausgegebene Beweis gilt ausdrücklich **über Q**.

## Offene Fälle und echte Löcher

Die neue Prüfung ist begrenzt. Sie versucht elf festgelegte Primzahlen bis 31. Nicht jede Formel ohne rationale Nullstellen erhält dabei ein Zertifikat. Beispielsweise bleibt der Vergleich von P/P mit 1 für `P=(x²−2)(x²−3)(x²−6)` offen. Eine Nullstelle modulo einer Primzahl ist kein Nachweis einer rationalen Nullstelle.

Echte Löcher bleiben erhalten. `x/x` und `1` sind weiterhin verschieden. Auch bei rationalen Koeffizienten werden unzulässige Primzahlen übersprungen, damit etwa eine Ausschlussstelle bei 1/2 nicht verloren geht.

`INCOMPLETE` bedeutet, dass eine Ressourcen- oder Abbruchgrenze erreicht wurde. `UNDECIDED` bedeutet dagegen: Die konfigurierte Prüfung ist abgeschlossen, kann diesen Vergleich aber nicht entscheiden.

## Grenzen und weitere Roadmap

`--max-operations` und `--max-bits` begrenzen die Prüfung. Die zusätzliche Beweisarbeit wird in `operations` mitgezählt. `--domain-policy quadratic` ist der unveränderte Standard; `modular` schreibt versionierte V2-Dateien mit reproduzierbarem Zertifikat. Ein Export überschreibt keine vorhandene Datei.

Diese Erweiterung gilt vorerst für `compare`. Ausdruckssuche, Portfolio und Erfahrungsplanung verwenden weiterhin ihren bisherigen Q-Vergleich. Ihre Einbindung sowie vollständigere Nullstellenverfahren bleiben weitere Pakete; dies ist kein vollständiger M2-/Gesamtprojektabschluss.

[Vertrag](61-modular-comparison-contract.md) · [Abnahme](acceptance/modular-comparison-v1-2026-09-22.md)
