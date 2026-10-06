# Gegenbeispiele im Portfolio und Erfahrungsplanung – Core 0.35

## Umfang

`portfolio-expression --equivalence rational-q-witness` unterstützt nun feste, adaptive und erfahrungsbasierte Reihenfolge sowie alle drei Ausdrucksplaner. Das Ausdruckskind verwendet V10; die Familienvereinigung verwendet dieselbe mathematische Vergleichspolitik `rational-domain-gcd-modular-witness16-v1`. Der Bereichscache `equal-64` bleibt für diesen Modus gesperrt; der Teilbaumcache des Ausdrucksplaners bleibt nutzbar.

Nur bewiesene Bereichsgleichheit erlaubt eine Zusammenführung. Ein fertig gegengeprüfter rationaler Zeuge belegt `DIFFERENT`; eine erfolglose endliche Suche bleibt `UNKNOWN`. Originale, Vertreterwahl und feste nachgelagerte Familienreihenfolge bleiben erhalten. Die Suche nach Zeugen ist keine allgemeine Nullstellenentscheidung und kein Beweis einer aus Samples vermuteten Datenbeziehung.

## Gemeinsame Ressourcen und Nachweise

Es gilt `operations_used = ordering_operations + Summe(Teilrun-Operationen) + union_operations`. Bereichsarbeit ist darin enthalten und darf nicht zusätzlich addiert werden. Die Zähler `domain_witness_candidates`, `domain_bounded_witnesses` und `domain_different` summieren abgeschlossene Vergleiche aus Ausdruckskind und Vereinigung. Angefangene Vergleiche belasten die Operationszähler, veröffentlichen aber keine halbfertigen Nachweise.

Die gemeinsame Originalgrenze ist im neuen Modus optional auf maximal 256 setzbar; Standard bleibt 128. Jeder Teilrun erhält höchstens das noch verfügbare gemeinsame Budget. Zusätzlich bleiben die ersten vier Familien auf jeweils höchstens 128 Ergebnisse begrenzt; das Ausdruckskind darf den verbleibenden Rahmen bis 256 nutzen. Erreicht ein Teilrun seine Grenze, bleibt das Portfolio unvollständig. Andere Modi behalten ihre bisherigen Grenzen.

Jede abgeschlossene Familienvereinigung darf ihren ersten ausgewählten begrenzten Zeugen in der bestehenden `UNION`-Zeile zeigen: `domain_witness`, Portfolio-Kandidaten-ID `candidate`, Vertreter-ID `witness_against` und `excluded_side`. Links ist der eingehende Kandidat, rechts der vorhandene Vertreter. Weitere Zeugen bleiben durch die aggregierten Zähler und vollständigen Replay nachvollziehbar. Bei unterbrochener Vereinigung bleiben vorher abgeschlossene Zeugen sichtbar; der aktuelle unvollständige Ursprung bleibt separat erhalten.

Die neue Budgetpolitik `shared-budget-v4` reserviert 3072 Wrapperbytes bei fester und 3328 bei adaptiver/erfahrungsbasierter Reihenfolge. Das deckt Start, Ende, Reihenfolge, Teilrunmeldungen und fünf Vereinigungsereignisse einschließlich begrenzter Zeugen ab. Bestehende Trace-/Byte-, Kandidaten-, Bit- und Exportgrenzen bleiben wirksam.

## Versionen und gemeinsame Auswahl

| Reihenfolge | Export/Engine | Planer |
| --- | --- | --- |
| Fest | Portfolio V32 | `five-family-q-witness-v1` |
| Adaptiv | Portfolio V33 | `sample-shape-v11` |
| Erfahrung | Portfolio V34 | `experience-order-v11` |

Gleichheit: `rational-q-witness-domain-v1`; Regel: `q-domain-union-v1`; Budget: `shared-budget-v4`.

Die gemeinsame Implementierung `portfolio_version` legt Header, Engine und Planer für alle neun Fünferportfolio-Generationen fest. Ausgabe, Export und Replay verwenden diese Auswahl. Frühere Generationsprioritäten, Header und Payloads bleiben unverändert. Die fachlichen Decoder und ihre strikten Versionsprüfungen bleiben bestehen; die Tabelle ersetzt nicht die historische Rechenlogik.

Z-Auswahl verwendet Herkunft `portfolio-q-witness-v32/33/34-…`. Unvollständige Forschung, Polstellen, nichtganzzahlige Projektion und Int64-Überlauf bleiben gesperrt. Replay enthält die nötigen Quellen und benötigt keine ursprünglichen Forschungs-/Erfahrungsdateien.

## Experience V11

Nur unabhängige vollständige V32-/V33-Läufe mit Hold-out dürfen Stimmenquellen werden. V34 darf keine weiteren Erfahrungsquellen erzeugen. Der eigene Kontext `routing-context-v11-five-q-witness` bindet Vergleichs-, Vereinigungs-, Budget-, Potenz-, Grammatik-/Teilbaumcachepolitik sowie bisherige Zahlen-, Such-, Operator- und Formmerkmale. Andere Politiken und Ausdrucksplaner beeinflussen diesen Kontext nicht.

Neue gemischte Speicher verwenden `BRUCE_EXPERIENCE_V11`, Engine `bruce-experience-v11`, Politik `routing-experience-v11`. Historische Quellen, Kennungen und Ereignisse bleiben erhalten. Die tatsächliche zweistellige Headerlänge wird berücksichtigt. Vollständige kanonische Gegenrechnung verhindert auch einen mit neuem Hash versehenen Versionsdowngrade.

Acht Quellen, 16 Ereignisse, 1 MiB, höchstens eine Million deklarierte Suchoperationen pro Quelle, globale Lerninput-Deduplizierung und Ausschluss eigener Lernpaare bleiben unverändert. Widerruf entfernt den Einfluss für spätere Läufe; alte Exporte behalten den eingebetteten damaligen Stand.

## Abnahme und weitere Arbeit

Prüfungen umfassen alle Planer/Reihenfolgen, Operatorauswahlen, reale Gegenzeugen mit 256 Originalen, direkte Vereinigung höhergradiger Bereichsbedingungen und ihre Arbeitsabbrüche, sämtliche Budget-/Abbruchpositionen kleiner Aufgaben, Kontexttrennung, gemischte Erfahrung, Widerruf, Auswahl und Manipulationen. Frühere Exporte und vollständige alte Versionen werden weiter geprüft.

Die [Architekturabnahme-Matrix](75-current-architecture-and-milestones.md) unterscheidet vorhandene Implementierung von offenen M0–M2-Verträgen. Bereichscaches für diese neue Politik, vollständige Grid-Anbindung, allgemeine Strategieerweiterbarkeit und weitergehende Experience-Korrekturketten bleiben offen. Kein vollständiger M2-/M4-/Gesamtprojektabschluss.

[Benutzung](74-witness-portfolio-usage.md) · [Abnahme](acceptance/witness-portfolio-v1-2026-09-22.md)
