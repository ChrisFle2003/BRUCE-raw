# Nichtlineare Forschung – Core 0.4

Fortsetzung der freigegebenen Roadmap im bestehenden Forschungsablauf. Native Umsetzung im aktuellen Verzeichnis; keine zusätzliche Datenbank oder Abhängigkeit. Der Plan nutzt die Writing-Plans-/Executing-Plans-Schritte mit testweiser Umsetzung.

## Ziel und Vertrag

`research --strategy power-affine` erzeugt lazy `a*x^p+b` mit `p=1..max-power`, Standard 4, Maximum 8. Koeffizienten folgen dem bestehenden BigInt-/Bruchvertrag; ohne Domäne verwendet diese Strategie BigInt. Die bisherige affine Suche bleibt unverändert. `--max-power` ist ausschließlich für die neue Strategie erlaubt.

Reihenfolge: Exponent, Zähler-/Nennerslot von a, Slot von b. Konstante Funktionen (`a=0`) werden nur bei p=1 erzeugt. Rohslot x = `(p-1)*slots+a_slot`, y = b_slot, ID = x*slots+y. Alte ID-/Trace-/Replayverträge bleiben bytegleich. Kein Score bevorzugt heimlich einen Exponenten; verschiedene passende Formeln bleiben mehrdeutig.

Vorwärts: Potenz durch wiederholtes Quadrieren, dann a*x^p+b. Gegenprüfung: unabhängig über p aufeinanderfolgende Multiplikationen von a mit x, anschließend b addieren. Keine Wiederverwendung der Vorwärtspotenz. Jede Sampleprüfung reserviert `3*p+4` logische Operationseinheiten (konservative Obergrenze). Konstante Kandidaten werten die unnötige Potenz nicht aus. Jedes relevante Zwischenergebnis bleibt unter dem Zahlenlimit. Überschreitung stoppt den Gesamtlauf als INCOMPLETE, auch bei einem schon gefundenen Kandidaten.

Neues Format `BRUCE_POWER_BUNDLE_V1`, Engine `bruce-power-v1`, Regel `power-affine-v1`, Policy `power-samples-v1`; enthält Exponentengrenze und vollständigen Eingabe-/Budget-/Ergebniskontext. Der bestehende Parser dispatcht explizit nach Paketversion. Neue Z-Importe berechnen die Potenz tatsächlich; unverändert nur ganzzahlige int64-Projektion, `power-bigint-v1`/`power-rational-v1` als Kontext.

## Schritte

- [x] Historischen Core-0.3-Export vor Codeänderung sichern; neue CLI-Abnahmen schreiben und fehlende Strategie rot beobachten.
- [x] `math/power_affine` implementieren; bestehenden `ExactResearchTask` um Strategie/Exponentengrenze und Hypothese um Exponent erweitern. Vorhandene affine Ausgabe nicht verändern.
- [x] Generator, Gegenprüfung und Budgets in `exact_research.cpp` integrieren; CLI, Replay und Z-Projektion vervollständigen.
- [x] Quadratik/Kubik, rationale Werte, negative x, Konstanten, Mehrdeutigkeit, Hold-out-Widerlegung, fehlende Kandidaten, Ressourcenlimits und Manipulation prüfen. Unabhängige Python-Fraction-Referenz für vollständige Kandidatenmengen.
- [x] Unabhängiges Review und Debug-/UBSan-Gesamttests durchführen; ausführbare Demo und Benutzung dokumentieren.

## Review-Schwerpunkte

Alte Exporte dürfen sich nicht ändern. Ein rehashter veränderter Exponent darf keinen unverändert bestätigten Replay ergeben. x=0/1 darf konkurrierende Potenzen nicht künstlich eindeutig machen. Eine konstante Formel soll nicht wegen doppelter Syntax als mehrdeutig gelten. Sehr große rationale Zwischenergebnisse dürfen nicht durch Rundung oder stillen Kandidatenausschluss verschwinden.

## Abnahmebefehle

```bash
python3 tests/integration/test_power_research.py build/bruce -v
cmake --build build
ctest --test-dir build --output-on-failure
cmake --build build-ubsan
ctest --test-dir build-ubsan --output-on-failure
python3 scripts/demo.py
```

Keine allgemeine Formelsynthese, adaptive Planung oder M2-Gesamtabnahme wird aus dieser Lieferung abgeleitet.
