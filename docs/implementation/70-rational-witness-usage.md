# Rationale Definitionslücken finden – Core 0.33

Bruce kann beim Vergleich vorgegebener Polynombrüche jetzt kleine rationale Nullstellen höhergradiger Nenner suchen. Damit wird beispielsweise die Lücke bei `x=2` in `(x³−8)/(x³−8)` sichtbar, obwohl sich der Bruch außerhalb dieser Stelle zu `1` kürzt.

## Ausführen und nachprüfen

Die Koeffizienten stehen weiterhin aufsteigend: `-8,0,0,1` bedeutet `x³−8`.

```bash
cmake --build build
./build/bruce compare \
  --left-numerator '-8,0,0,1' --left-denominator '-8,0,0,1' \
  --right-numerator 1 --right-denominator 1 \
  --domain-policy modular-witness --output lueckenbeweis.bruce
./build/bruce replay lueckenbeweis.bruce
```

Ergebnis: `PROVEN_DIFFERENT`, `value_relation = EQUAL`, `domain_relation = DIFFERENT`. Der Bereichsnachweis enthält `witness = "2"` und `excluded_side = "left"`. Der Zeuge wurde an beiden ursprünglichen Nennerbedingungen gegengeprüft. Mit der bisherigen Politik `modular` bleibt dieser Fall `UNDECIDED`.

Auch gebrochene Zeugen sind möglich:

```bash
./build/bruce compare \
  --left-numerator '-1,0,0,8' --left-denominator '-1,0,0,8' \
  --right-numerator 1 --right-denominator 1 \
  --domain-policy modular-witness
```

Hier lautet der Zeuge `1/2`. Gemeinsame Lücken beider Brüche werden vor der Suche entfernt und können keinen Unterschied begründen.

## Grenzen richtig lesen

Gesucht werden vollständig gekürzte Brüche mit Zählerbetrag höchstens 16 und positivem Nenner höchstens 16. Zuerst versucht Bruce weiterhin die bisherigen modularen Beweise. Ohne solchen Beweis und ohne gefundenen Zeugen bleibt das Ergebnis offen.

```bash
./build/bruce compare \
  --left-numerator '-4913,0,0,1' --left-denominator '-4913,0,0,1' \
  --right-numerator 1 --right-denominator 1 \
  --domain-policy modular-witness
```

Dieser Fall bleibt `UNDECIDED`: Die echte Lücke bei `17` liegt außerhalb der Suche. Ein ausgeschöpfter Arbeitsrahmen (`--max-operations`) oder eine überschrittene Zahlengröße (`--max-bits`) liefert stattdessen `INCOMPLETE`. Eine größere Bitgrenze vergrößert nicht den festgelegten Suchraum.

Die beiden Zähler `left_witness_candidates` und `right_witness_candidates` stehen im Bereichszertifikat. Sie zählen vollständig ausgewertete Suchstellen. Ihre Arbeit ist bereits in `operations` enthalten. Bei einem abgebrochenen Bereichsvergleich wird kein halbfertiges Zertifikat ausgegeben.

Neue Exporte sind V3; alte V1-/V2-Dateien bleiben replaybar. Die Option gilt zunächst nur für `compare`. Forschung, Portfolio, Erfahrung und Bereichscaches verwenden ihre bisherigen Verfahren.

## Weitere Roadmap

Die begrenzte Zeugensuche ist implementiert. Ihre Einbindung in Forschung und Portfolio, vollständigere Nullstellenentscheidungen, weitere Suchoptimierungen und Experience-Korrekturketten bleiben offen. Der Beweis betrifft die angegebenen Formeln über Q; er ist kein allgemeiner Beweis einer aus Samples vermuteten Beziehung.

[Vertrag](69-rational-witness-contract.md) · [Abnahme](acceptance/rational-witness-v1-2026-09-22.md)
