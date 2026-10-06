# Modulare Nullstellenausschlüsse – Core 0.29

## Umfang und Abnahmeziel

Die freigegebene Fortsetzung erweitert ausschließlich den eigenständigen Polynombruchvergleich. `compare --domain-policy modular` ergänzt die bisherige Entscheidung bis Restgrad 2 um begrenzte Zertifikate für höhere Grade. Ein vollständiger Beispielpfad umfasst Eingabe, exakten Befund, sichtbares Zertifikat, Export und Replay. Ausdruckssuche, Portfolio und Erfahrung verwenden weiterhin ihre bisherige Vergleichspolitik.

`--domain-policy quadratic` bleibt Standard. Bestehende Ergebnisse, Kosten und V1-Dateien bleiben unverändert. Die neue Politik ist optional und behauptet keine vollständige Entscheidung aller rationalen Nullstellen.

## Mathematischer Vertrag

Wie bisher bleiben die ursprünglichen Nennerausschlüsse erhalten. Beide Ausschlusspolynome werden quadratfrei und normiert; ihr gemeinsamer Teiler wird entfernt. Die verbleibenden Polynome sind teilerfremd und normiert. Leere Definitionsbereiche und Restgrade 0–2 werden wie bisher entschieden.

Für jeden Restgrad 3–64 werden in fester Reihenfolge die Primzahlen **2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31** versucht:

1. Teilt eine Primzahl einen Nenner eines rationalen Koeffizienten des Restpolynoms, wird diese Primzahl übersprungen.
2. Andernfalls werden alle Koeffizienten durch Multiplikation mit dem inversen Nenner in den Restklassenkörper übertragen.
3. Das Polynom wird für jede Restklasse ausgewertet. Eine Null beendet diesen Versuch ohne Zertifikat; sie beweist keine rationale Nullstelle.
4. Sind sämtliche Werte ungleich null, gilt `NONE`: Das Restpolynom hat keine rationale Nullstelle. Die erste geeignete Primzahl und sämtliche Werte bilden das Zertifikat.
5. Ohne solches Zertifikat bleibt `UNKNOWN`.

**Begründung:** Für ein normiertes rationales Polynom sei L das kleinste gemeinsame Vielfache der Koeffizientennenner. Nach Multiplikation mit L ist das Polynom ganzzahlig mit Leitkoeffizient L. Der vollständig gekürzte Nenner einer rationalen Nullstelle teilt L: Einsetzen von a/b und Multiplikation mit bⁿ zeigt, dass b den Leitkoeffizienten teilt. Bei einer zulässigen Primzahl ist L invertierbar; deshalb wäre auch die Nullstelle modulo dieser Primzahl definiert und eine Nullstelle des reduzierten Polynoms. Eine vollständig nullstellenfreie Restwerttabelle schließt dies aus.

Das ist eine Aussage über alle rationalen Eingaben der **vorgegebenen** Formeln. Es ist kein Beweis über reelle Definitionsbereiche, keine Formelentdeckung aus Beispielen und keine irreduzible Faktorisierung.

## Zertifikat und Gesamtbefund

Zusätzlich zu den bisherigen Restpolynomen, Nullstellenstatus und gegebenenfalls einem unabhängig eingesetzten Gegenbeispiel enthält `domain_certificate`:

- `left_modular_prime`, `right_modular_prime`: beweisende Primzahl oder 0, falls kein modulares Zertifikat vorliegt;
- `left_modular_values`, `right_modular_values`: Werte für x=0 bis p−1, bei fehlendem Zertifikat leere Listen.

Eine Primzahl 0 bedeutet nicht `UNKNOWN`: Auch die bisherige lineare/quadratische Entscheidung kann bereits abgeschlossen sein. Maßgeblich sind `left_roots`, `right_roots` und `domain_relation`.

Die Gesamtentscheidung bleibt unverändert: verschiedene Bruchwerte oder verschiedene Definitionsbereiche ergeben `PROVEN_DIFFERENT`; gleiche Werte und nachweislich gleiche Bereiche ergeben `PROVEN_EQUIVALENT`; sonst `UNDECIDED`. Eine gefundene rationale Ausschlussstelle wird weiterhin an beiden ursprünglichen Bereichsbedingungen gegengeprüft.

## Ressourcen und Unterbrechung

Es gelten dieselben Eingabe-, Grad-, Bit- und Arbeitsgrenzen wie im [Basisvertrag](55-fraction-comparison-contract.md). Jeder Primzahlversuch, jeder Zähler-/Nennerrest, jeder Inversenversuch, jede Koeffizientenübertragung, jeder Horner-Schritt und jede Nullprüfung kosten je eine logische Einheit vor Ausführung. Inverse werden in der endlichen Folge 1 bis p−1 gesucht. Maschinenarithmetik für diese festen kleinen Restklassen ist auf Werte unter 1000 beschränkt; `max_bits` begrenzt weiterhin die exakten rationalen Ein- und Zwischenwerte, nicht diese Kontrollarithmetik.

Budgetgrenze, Bitgrenze und Abbruch liefern `INCOMPLETE`. Der Bereichsvergleich wird erst nach vollständigem Abschluss in den Run übernommen. Ein dortiger Abbruch veröffentlicht deshalb keine halbfertige Bereichsentscheidung; zuvor abgeschlossene Normalformen bleiben erhalten. Ein Abbruch während der nachfolgenden Wertprüfung darf das fertige Bereichszertifikat erhalten, aber keinen fertigen Gesamtbeweis ausgeben.

## Versionierung und Replay

Modulare Läufe schreiben `BRUCE_FRACTION_COMPARISON_V2`, Engine `bruce-fraction-comparison-v2`, Politik `rational-domain-gcd-modular-v1`. Header, Engine und Politik müssen zusammenpassen. V1 bleibt an `rational-domain-gcd-quadratic-v1` gebunden. Die Politik bestimmt auch Primzahlliste, Reihenfolge und Kostenmodell; Änderungen daran erfordern eine neue Version.

Export enthält alle Eingaben, Grenzen, gegebenenfalls den Abbruchpunkt und das vollständige Ergebnis. Replay berechnet einschließlich Zertifikat und Kosten neu und vergleicht den vollständigen kanonischen Inhalt. Ein neu berechneter Dateihash legitimiert keine geänderten Beweiswerte. Bestehende Dateien werden nicht überschrieben.

## Prüfung und offene Arbeit

Tests prüfen bekannte kubische und höhergradige Beweise, rationale Koeffizienten und negative Skalierung, echte rationale Löcher, gemeinsame Faktoren, Grad 64, übersprungene Primzahlen, begründet unentschiedene Fälle, unabhängige Restwerttabellen, jede Budget-/Abbruchposition in Referenzläufen, manipulierter Replay und historische Ergebnisse. Der Demo-Pfad zeigt die neue Entscheidung gegenüber der bisherigen offenen Antwort.

Die Einbindung in Ausdrucksgruppen, Portfolio und Erfahrung bleibt ein eigenes Paket. Vollständige Nullstellenentscheidung, allgemeinere Grammatiken, externe Evidenz, Sprache, NanoCell, Cockpit, Phase-DAG, GPU und BigInt im Grid/Z bleiben offen.
