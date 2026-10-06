# Begrenzter Cache für bewiesene Bereichsgleichheit – Core 0.31

## Umfang

Die freigegebene Fortsetzung ergänzt die eigenständige Ausdruckssuche um `--domain-cache equal-64`. Er ist nur mit `--equivalence rational-q-modular` zulässig und funktioniert mit allen drei Grammatikplanern. Standard ist `none`; bestehende Ausgaben, Kostenmodelle und Exportversionen bleiben erhalten. Portfolio und Experience sind noch nicht angeschlossen.

Der Cache ist ausschließlich temporärer Zustand eines Forschungslaufs. Er ersetzt keine Gegenrechnung, speichert keine Aufgaben-/Antwortdatenbank und übernimmt keine externen Behauptungen.

## Identität und Wiederverwendung

Eine Cacheinstanz besitzt ihren unveränderlichen rationalen Zahlenbereich einschließlich Bitgrenze und verwendet ausschließlich `rational-domain-gcd-modular-v1`. Schlüssel bestehen aus den beiden **geordneten, vollständigen** Koeffizientenlisten der Ausschlusspolynome, mit eindeutigen Klammern und Koeffiziententrennern. Die übergebenen Listen werden für den Schlüssel nicht algebraisch umgeformt; äquivalente anders geschriebene oder vertauschte Paare dürfen einen Cachefehler verursachen, aber keinen falschen Treffer.

64 feste Plätze; Platzzuordnung durch FNV-1a-64 modulo 64. Vor einem Treffer werden Länge und sämtliche Schlüsselbytes verglichen. Ein Hash- oder Platzgleichstand beweist keine mathematische Gleichheit.

Nur vollständig berechnete Ergebnisse `EQUAL` werden gespeichert. `UNKNOWN`, `DIFFERENT`, Zahlen-/Gradgrenzen und abgebrochene Vergleiche werden nicht eingetragen. Ein Cachetreffer liefert eine bereits belegte Bereichsgleichheit; Wertegleichheit der Brüche wird davor weiterhin gesondert geprüft. Es gibt keine zusätzliche transitive Schlussfolgerung und keine Änderung der Vertreterreihenfolge.

Bei einem belegten Platz ersetzt ein neu vollständig bewiesenes anderes Paar den bisherigen Eintrag. Ein anderer oder offener Befund verdrängt keine gespeicherte Gleichheit.

## Speicher und Kosten

Ein Schlüssel darf höchstens 4096 Bytes enthalten; größere Schlüssel umgehen den Cache und führen den vollständigen Vergleich aus. Es gibt höchstens 64 Einträge und 262144 gespeicherte **Schlüsselbytes**. Das ist keine Angabe des gesamten RAM-Verbrauchs: Stringkapazitäten, Container, temporäre Zahlentexte, GMP und übrige Forschungsdaten kommen hinzu. Die vorhandenen Koeffizienten-, Grad- und Bitgrenzen gelten weiter.

Cachekosten werden vor Ausführung belastet: je eine Einheit pro Listenprüfung, Koeffizientenprüfung und Textkonvertierung, pro Hashbyte, pro Längenprüfung, pro tatsächlich verglichenem Schlüsselbyte und vor dem Eintragen. Ein Abbruch vor der letzten Eintragsbelastung darf keinen neuen Speicherinhalt erzeugen. Schlüsselaufbau, Prüfung und Hashing sind budgetiert; die mathematische Gegenprüfung benutzt ihre bisherigen Kosten.

`domain_cache_operations` ist ein **Teil** von `domain_comparison_operations`, das wiederum in `operations_used` enthalten ist. Diese Zähler nicht addieren. Die übrige Ausdruckssuche und ihre Grenzen bleiben unverändert. Wie bisher liegen Abbruch-Checkpoints zwischen Kandidaten; innerhalb eines Vergleichs greift die Arbeitsgrenze vor jedem kostenpflichtigen Schritt.

## Sichtbare Statistik

Nur aktivierte Cacheläufe ergänzen:

- `domain_cache_policy = equal-64-key4096-v1`;
- `domain_cache_hits`: vollständig bestätigte Schlüsselübereinstimmungen;
- `domain_cache_misses`: vollständig geprüfte, nicht passende Cacheplätze;
- `domain_cache_bypasses`: Schlüssel, die wegen ihrer Form oder Größe nicht gespeichert werden können;
- `domain_cache_inserts`, `domain_cache_evictions`: abgeschlossene Einträge bzw. Ersetzungen;
- `domain_cache_entries`, `domain_cache_key_bytes`: aktueller Speicherinhalt;
- `domain_cache_operations`: tatsächlich belastete Cacheverwaltung.

Die Statistik bleibt auch bei Teilbefunden erhalten. Ein begonnener und beim Schlüsselaufbau unterbrochener Vergleich muss noch keinen Treffer-/Fehltrefferzähler erhöhen. `domain_comparisons` zählt weiterhin alle begonnenen Bereichsprüfungen einschließlich Cacheanfragen. `domain_modular_certificates` zählt nur neu berechnete modulare Seitenzertifikate, keine wiederverwendeten Beweise.

## Versionierung und Replay

Aktivierte Läufe schreiben `BRUCE_EXPRESSION_BUNDLE_V9`, Engine `bruce-expression-v9`, Ergebnispolicy `defined-expression-samples-v9`. Gleichheits-, Mathematik-, Grammatik-, Potenz- und Teilbaumcachepolitik bleiben explizit enthalten. Die zusätzliche Payloadzeile `domain_cache_policy equal-64-key4096-v1` muss zur Version passen.

Der Export enthält keine externe Cachedatei. Replay baut einen leeren Cache auf und reproduziert alle Einträge, Treffer, Kosten, Teilbefunde und Ursprünge aus den eingebetteten Eingaben. Änderungen an Cacheinhaltspolitik, Schlüsselbildung, Platzzuordnung oder Kostenmodell benötigen eine neue Version. Manipulierte Zähler oder Metadaten werden auch mit neuem Dateihash abgelehnt.

Z-Auswahl erkennt V9 mit eigener Herkunft `expression-q-cached-v9-…`. Polstellen, nichtganzzahlige Ergebnisse, Int64-Überlauf und unvollständige Forschungsbefunde bleiben gesperrt; Auswahl-Replay benötigt die Originalforschungsdatei nicht.

## Abnahme und nächste Arbeit

Prüfungen umfassen echte Cachetreffer bei identischen vollständigen Gruppen/Originalen, Kollision gegen eine echte rationale Definitionslücke, Ersetzung, unbekannte Vergleiche, übergroße Schlüssel, feste Speichergrenze, jede Unterbrechung beim ersten Eintrag, Forschungsbudgets/Checkpoints und vollständigen Replay. Ein vollständiger Forschungs-/Auswahlpfad und ein größerer Teilbefund zeigen Aussagegrenzen und gemessene Arbeit.

Portfolio-/Experience-Einbindung ist das nächste Integrationspaket. Weitere Suchoptimierungen, vollständigere Nullstellenverfahren, externe Evidenz und Experience-Korrekturketten bleiben offen, ebenso Sprache, NanoCell, Cockpit, Phase-DAG, GPU und BigInt im Grid/Z. Kein vollständiger M2-/Gesamtprojektabschluss.
