# Begrenzte rationale Nullstellenzeugen – Core 0.33

## Umfang

Der eigenständige Polynombruchvergleich erhält die optionale Politik `compare --domain-policy modular-witness`. Sie ergänzt die bestehenden quadratischen Entscheidungen und modularen Nullstellenausschlüsse um exakt eingesetzte rationale Gegenbeispiele bei Restgrad 3–64. Bestehende Standard-, Forschungs-, Cache-, Portfolio- und Erfahrungspolitiken bleiben unverändert.

Ein vollständiger Ablauf umfasst vorgegebene Polynombrüche, eine konkrete Definitionslücke, Gegenprüfung beider ursprünglicher Bereichsbedingungen, sichtbaren Befund, Export und Replay. Es werden keine Beziehungen aus Samples bewiesen.

## Mathematische Entscheidung

Die bisherige Normalisierung, Erhaltung ursprünglicher Nennerausschlüsse und Entfernung gemeinsamer Faktoren gilt weiter. Beide Restpolynome sind teilerfremd. Für Restgrad 0–2 bleibt die exakte bisherige Entscheidung erhalten. Für höhere Grade werden zuerst die [modularen Zertifikate](61-modular-comparison-contract.md) mit unveränderter Primzahlfolge versucht.

Nur wenn kein modulares Ausschlusszertifikat entsteht, wird das jeweilige Restpolynom an folgenden rationalen Stellen ausgewertet:

- Nenner `b = 1, 2, …, 16` in dieser Reihenfolge.
- Je Nenner Beträge `m = 0, 1, …, 16`. Nur `gcd(m,b)=1` ist zugelassen.
- Bei positivem Betrag zuerst `m/b`, dann `−m/b`. Null wird ausschließlich als `0/1` geprüft.
- Jeder Wert wird exakt mit rationaler Horner-Auswertung berechnet. Die erste Null ergibt `PRESENT` und einen Zeugen.
- Ohne Treffer bleibt der Nullstellenstatus `UNKNOWN`. Die endliche erfolglose Suche ist **kein** Beweis für Nullstellenfreiheit.

Ein Zeuge wird anschließend durch unabhängiges Einsetzen in beide ursprünglichen, kanonisierten Ausschlusspolynome geprüft: Genau eine Seite muss ihn ausschließen. Erst danach ist der Bereichsbefund `DIFFERENT` fertig. Gemeinsame Nennernullstellen dürfen kein Gegenbeispiel werden. Die linke Seite hat bei zwei gefundenen Zeugen wie bisher Vorrang; beide Seiten werden in unveränderter Reihenfolge geprüft.

Beispiel: `(x³−8)/(x³−8)` und `1` besitzen dieselbe gekürzte Wertformel, aber verschiedene Bereiche über Q, belegt durch `x=2`. Dasselbe gilt für `(8x³−1)/(8x³−1)` bei `x=1/2`. Eine Lücke bei `17` oder `1/17` liegt außerhalb des Suchraums und bleibt bei erfolglosen modularen Prüfungen offen.

Die Gesamtstatusregeln bleiben unverändert: verschiedene Werte oder verschiedene Bereiche ergeben `PROVEN_DIFFERENT`; gleiche Werte und bewiesen gleiche Bereiche ergeben `PROVEN_EQUIVALENT`; sonst `UNDECIDED`.

## Budget und Sichtbarkeit

Es gelten die bisherigen maximal 65 Koeffizienten, Eingabe-, Zahlenbit- und Arbeitsgrenzen. Es gibt keine Faktorisierung großer ganzer Zahlen und keinen unbeschränkten Teilersuchlauf.

Zusätzlich zu den bisherigen Kosten wird jeweils vor Ausführung belastet:

1. Eine Einheit je ggT-Prüfung eines Betrags-/Nennerpaars.
2. Eine Einheit je Konstruktion und Bitprüfung eines zugelassenen rationalen Kandidaten.
3. Die bisherigen Horner-Kosten: eine Einheit für den Leitkoeffizienten und je eine für Multiplikation und Addition jedes weiteren Koeffizienten.
4. Eine Einheit für die abschließende Nullprüfung einschließlich Erhöhung des Zählers vollständig geprüfter Kandidaten.

Die kleinen festen Schleifengrenzen sind Kontrollarithmetik. Rationale Kandidaten und Auswertungen unterliegen `max_bits`. Eine Überschreitung liefert `INCOMPLETE / IntegerBitLimit`; Arbeitsgrenzen und Abbruch liefern ebenfalls `INCOMPLETE`. Der gesamte Bereichsbefund wird atomar übernommen: Keine halbfertigen Zeugen, Kandidatenzähler oder Teilzertifikate werden veröffentlicht. Nach abgeschlossenem Bereichsvergleich kann ein späterer Abbruch während des Wertevergleichs den fertigen Bereichsbefund erhalten.

`domain_certificate` ergänzt ausschließlich für die neue Politik `left_witness_candidates` und `right_witness_candidates`. Sie zählen vollständig ausgewertete Suchstellen im jeweiligen Restpolynom. Analytisch gefundene lineare/quadratische Zeugen zählen hier nicht; bei einem modularen Abwesenheitsbeweis bleibt der Zähler null. Die Arbeit ist bereits in `operations` enthalten.

## Versionierung und Replay

Neue Läufe verwenden `BRUCE_FRACTION_COMPARISON_V3`, Engine `bruce-fraction-comparison-v3` und Politik `rational-domain-gcd-modular-witness16-v1`. Die Politik bindet Suchgrenzen, Kandidatenreihenfolge, Primzahlfolge und Kostenmodell. Änderungen daran benötigen eine neue Version.

V1 und V2 behalten ihre bisherigen Politiken und byteidentischen Ergebnisse. Replay berechnet Eingaben, Normalformen, Bereichsentscheidung, Zeuge, Zähler, Kosten und Trace neu. Ein aktualisierter Dateihash legitimiert keine geänderten Beweiswerte oder Versionskombinationen. Bestehende Ausgabedateien werden weiterhin nicht überschrieben.

## Abgrenzung

Die Suche ist ein begrenztes Verfahren zum Finden von Gegenbeispielen, keine vollständige rationale Nullstellenentscheidung. Ausdruckssuche, Portfolio, Erfahrung und deren Bereichscaches verwenden sie noch nicht. Deren Einbindung benötigt eigene Versionen und bleibt ein Folgepaket. Weitere Nullstellenverfahren, Suchoptimierung, externe Evidenz und Experience-Korrekturketten bleiben offen. Kein vollständiger M2-/Gesamtprojektabschluss.

[Benutzung](70-rational-witness-usage.md) · [Abnahme](acceptance/rational-witness-v1-2026-09-22.md)
