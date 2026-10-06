# Abnahme Z-Kompatibilität / Core 0.2 – 22.09.2026

## Lieferung

Numerische, regelkonfigurierbare Z-Kompatibilität im vorhandenen BRUCE-Kern: immutable eingelesener Snapshot, exakte Prädikate, getrennte 64-Bit-Gruppen, bitweise Zulässigkeitsfilterung, Ranking ohne verdeckte Auflösung von Gleichständen, begrenzte Regelspur und kanonischer Replay. Bestehende Forschungsexporte können als tatsächlich konsumierte Kandidatenquelle dienen. Persistenz bleibt optional.

## Ausgeführte Prüfungen

```bash
cmake --build build
ctest --test-dir build --output-on-failure
python3 tests/integration/test_compatibility.py build/bruce -v
python3 scripts/demo.py
cmake --build build-ubsan
ctest --test-dir build-ubsan --output-on-failure
```

Ergebnis: vier CTest-Suiten bestanden, sowohl im Debug-Build als auch mit UndefinedBehaviorSanitizer. Build weiterhin mit `-Wall -Wextra -Werror`. Keine Sanitizer-Meldung. Zwölf neue CLI-Tests zusätzlich zu den acht vorhandenen CLI-Tests und 4200 bisherigen C++-Checks. Direkte API-Prüfungen sichern kontrollierten Abbruch samt Replay, geschlossene unvollständige Filterbits und den Dispatcher für Forschungspakete.

Abgedeckte neue Fälle:

- positive Treffer, harte Ausschlüsse, manuelle Negativreferenzen und Kontext bleiben getrennt;
- zwei beste Kandidaten bleiben mehrdeutig; ein harter Ausschluss ist durch Gewichtung nicht umkehrbar;
- 130 Kandidaten ergeben Gruppen von 64/64/2, mit korrektem Bit 63 und ohne unbenutzte zulässige Bits;
- anders sortierte Eingabezeilen erzeugen dieselbe kanonische Ausgabe;
- negative modulare Werte, int64-Minimum, uint64-Layermaximum und uint64-z-Maximum;
- zwölf deterministisch erzeugte Snapshots mit jeweils 70 Kandidaten und zwölf Regeln gegen einen unabhängigen Python-Ganzzahlvergleich;
- Kandidaten-, Prüf- und Tracebudgets exakt ausreichend versus einen Schritt zu knapp;
- kein Gewinner bei unvollständigem Lauf;
- unbekannte Sprache/Engine, doppelte IDs, ungültige Operatoren, fehlende Negativreferenzen und gemischte Koordinatenkontexte;
- neue Regeldatei verändert den alten Replay nicht; Originaldateien können beim Replay fehlen;
- neu berechneter äußerer Digest versteckt weder ein geändertes Ergebnis noch einen widersprüchlichen inneren Snapshot;
- tatsächlicher Forschungsimport inklusive reproduzierbarer Projektion und erhaltenem Ursprungsstatus;
- ein mit dem ursprünglichen Core-0.1-Binary erzeugtes, dauerhaft gespeichertes Forschungspaket bleibt kompatibel.

## Sichtbarer Demoablauf

Die bisherige affine Demo findet weiterhin `2*x+(3)` aus 289 Kandidaten und replayt identisch. Der neue Schritt importiert ihren Forschungslauf, wertet an `x=10` aus und erhält `SELECTED`, während `origin_status=VERIFIED_ON_SAMPLES` unverändert bleibt. Ein separater Filterfall liefert `AMBIGUOUS` mit den IDs 3 und 9. Das bisherige Gegenbeispiel erzeugt weiterhin `REFUTED`.

## Review

Ein separater Reviewer fand keinen reproduzierbaren Codefehler. Seine unabhängigen Proben verglichen 160 Regelkombinationen auf drei Grenzwertkandidaten mit Python-Ganzzahlen und prüften kanonische Darstellung sowie exakte Budgetgrenzen. Die Empfehlung für dauerhafte unsigned-z-/Budgettests wurde umgesetzt. Der widersprüchliche frühere ID-Tie-Break im Vertragsdokument wurde korrigiert.

## Verbleibende Grenzen

Die negative Referenz ist eine vom Eingabevertrag benannte manuelle Quelle, keine extern beglaubigte Evidenz. Auswahl ist kein Beweis und öffnet weder LearnGate noch PublishGate. Token-/Sprachintegration, Experience-Korrekturen, BigInt, adaptive Suche, NanoCell und weitere Roadmap-Module sind nicht Teil dieser Abnahme. Details in [Core-0.2-Benutzung](../08-z-compatibility-usage.md).
