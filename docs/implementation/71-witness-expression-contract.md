# Rationale Gegenbeispiele in der Ausdruckssuche – Core 0.34

## Umfang und Ziel

`research --strategy expression --equivalence rational-q-witness` integriert die [begrenzte rationale Zeugensuche](69-rational-witness-contract.md) in die eigenständige Formelforschung. Alle drei Planer `grammar`, `indexed-grammar` und `cached-grammar`, die bisherige Operatorauswahl und literale Potenzen werden unterstützt.

Der neue Modus ist optional. Die ersten neun Ausdrucksversionen, Standardwerte, bisherigen Ergebnis- und Kostenmodelle bleiben erhalten. Portfolio, Erfahrungsplanung und `--domain-cache equal-64` akzeptieren ihn noch nicht. Der Teilbaumcache des Planers `cached-grammar` bleibt davon unabhängig nutzbar.

Abnahmeziel: tatsächliche rationale Gegenstellen im Suchlauf, sichtbare Zuordnung der verglichenen Ausdrücke, exakte Gegenprüfung, erhaltene Originale, gemeinsame Budgets, V10-Replay und vollständige Forschung mit anschließender Z-Auswahl.

## Gruppierung

Kandidaten durchlaufen unverändert Training, getrennte Gegenrechnung, Hold-out und Normalisierung mit Erhalt ursprünglicher Nennerbedingungen. Nur Kandidaten mit gleichen gekürzten Bruchwerten gelangen zum Bereichsvergleich. Die Vertreter werden weiterhin in ihrer bestehenden Reihenfolge geprüft.

Der Bereichsvergleich nutzt `rational-domain-gcd-modular-witness16-v1`: gemeinsame Faktoren entfernen, Restgrade bis zwei analytisch entscheiden, höhere Grade modular prüfen und erforderlichenfalls reduzierte Brüche mit Zählerbetrag und positivem Nenner höchstens 16 exakt einsetzen. Gefundene Zeugen werden an beiden ursprünglichen Bereichsbedingungen gegengeprüft.

Nur `EQUAL` erlaubt eine Zusammenführung. `DIFFERENT` und `UNKNOWN` bleiben getrennt. Ein Gegenbeispiel gegenüber einem Vertreter schließt eine Zusammenführung mit einem anderen passenden Vertreter nicht aus. Ein erfolgloser endlicher Suchraum liefert keine zusätzliche Gleichheitsbehauptung. Repräsentanten und alle gespeicherten Originalherkünfte bleiben erhalten.

Ein Beispiel ist `((x-x)/(x-(3/x)))`: Die ursprüngliche Bereichsbedingung enthält das Polynom `x*(x²−3)`. Der Vergleich mit der überall definierten Nullfunktion findet die rationale Gegenstelle 0. Anschließend darf der Ausdruck zur bereits vorhandenen Nullfunktion mit Lücke bei 0 gehören, da ±√3 nicht rational sind.

## Budget und Ergebnisgrenze

Jeder Vergleich und jede Kandidatenauswertung verwendet das gemeinsame Operationsbudget. `domain_comparison_operations` enthält die gesamte verbrauchte Vergleichsarbeit einschließlich abgebrochener Vergleiche und ist bereits in `operations_used` enthalten. Die mathematischen Kosten bleiben exakt an den Vertrag von Core 0.33 gebunden.

Nur im neuen Modus darf `--max-results` Werte von 1 bis **256** annehmen. Der Standard bleibt **128**. Gezählt werden Originaltreffer, nicht zusammengeführte Gruppen. Frühere Modi behalten ihre Obergrenze 128. Der größere optionale Rahmen ist erforderlich, damit das dokumentierte Beispiel die betreffenden Ausdrücke erreicht. Er ist kein automatisches Weiterlaufen und kein unbegrenzter Speicher.

Die bisherigen Arbeits-, Kandidaten-, Zahlenbit-, Grammatik-, Trace-, Byte- und Exportgrenzen bleiben erhalten. Bei einer Unterbrechung bleibt der aktuelle Ursprung separat `INCOMPLETE` erhalten. Bereits abgeschlossene Vergleiche behalten ihre Zähler; ein unterbrochener Einzelvergleich veröffentlicht keinen halbfertigen Zeugen und keine Kandidatenzahl. Dessen verbrauchte Arbeit wird trotzdem abgerechnet.

## Sichtbare Nachweise

Nur der neue Modus ergänzt folgende Laufzähler:

- `domain_witness_candidates`: Summe der vollständig ausgewerteten Suchstellen aus **abgeschlossenen** Bereichsvergleichen beider Seiten.
- `domain_bounded_witnesses`: Anzahl abgeschlossener Vergleiche, deren ausgewählter Gegenzeuge aus der begrenzten Suche stammt. Analytische lineare oder quadratische Zeugen zählen nicht dazu.
- `domain_different`: Anzahl aller abgeschlossenen Bereichsvergleiche mit Ergebnis `DIFFERENT`, einschließlich analytischer Entscheidungen.

Die bisherigen Vergleichs-, Offenheits- und modularen Zertifikatzähler bleiben vorhanden. Die neuen Zähler buchen keine zusätzlichen Operationen.

Pro Ausdruckskandidat wird höchstens der **erste ausgewählte begrenzte Gegenzeuge** in seinem bestehenden Trace-Ereignis gezeigt:

```text
EXPRESSION id=8978 VERIFIED_ON_SAMPLES domain_witness=0 witness_against=2 excluded_side=left domain_checks=2 domain_unknown=0 merge_into=72
```

`left` bezeichnet den gerade untersuchten Kandidaten, `right` den angegebenen Vertreter. Die Trace-Zeile enthält nicht alle späteren Zeugen dieses Kandidaten; deren abgeschlossene Vergleiche sind in den Zählern enthalten. Die bestehenden 512 reservierten Bytes pro Verarbeitungsschritt reichen auch für die begrenzten Zeugen, IDs, Status- und Abschlussangaben. Es werden keine beliebig langen analytischen Zahlen in dieses Zusatzfeld geschrieben.

## Versionierung und Auswahl

Neue Läufe schreiben `BRUCE_EXPRESSION_BUNDLE_V10`, Engine `bruce-expression-v10`, Ergebnispolitik `defined-expression-samples-v10`, Gleichheitspolitik `rational-q-witness-domain-v1` und Vergleichspolitik `rational-domain-gcd-modular-witness16-v1`. Grammatik-, Operator-, Potenz- und gegebenenfalls Teilbaumcachepolitik bleiben explizit gebunden.

Replay berücksichtigt die zweistellige Versionsnummer und rekonstruiert den vollständigen Auftrag, Kandidaten, Gruppen, Originale, Zeugen-Trace, Zähler, Kosten und Teilbefunde. Änderungen daran werden auch mit neu berechnetem Dateihash abgelehnt. Frühere Dateien bleiben unverändert replaybar; vorhandene Ausgabedateien werden nicht überschrieben.

Z-Auswahl erkennt die eigene Herkunft `expression-q-witness-v10-…`. Polstellen, nichtganzzahlige Projektionen, Int64-Überlauf und unvollständige Forschung bleiben gesperrt. Die eingebettete Quelle erlaubt Auswahl-Replay nach Entfernen der ursprünglichen Forschungsdatei.

## Grenzen

Eine bewiesene Definitionslücke betrifft die vorgegebenen Kandidaten über Q. Der Forschungsstatus bleibt eine Aussage über geprüfte Samples, kein allgemeiner Beweis einer unbekannten Datenbeziehung. Das Referenzbeispiel mit 256 Originalen ist an der Ergebnisgrenze unvollständig.

Portfolio-/Experience-Einbindung, eine dazu passende Bereichscachepolitik, vollständigere Nullstellenentscheidungen, weitere Suchoptimierungen und Experience-Korrekturketten bleiben offen. Kein vollständiger M2-/M4-/Gesamtprojektabschluss.

[Benutzung](72-witness-expression-usage.md) · [Abnahme](acceptance/witness-expression-v1-2026-09-22.md)
