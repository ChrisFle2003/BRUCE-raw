# Z-Kompatibilität V1 – nächster ausführbarer Ausbau

Die Fortsetzungsfreigabe des Nutzers umfasst den nächsten Roadmap-Schritt. Der vorhandene C++-Kern und seine CLI werden erweitert; kein zusätzliches Produkt und keine verpflichtende Datenbank entstehen.

## Lieferumfang

Versionierte, zur Laufzeit geladene Regelsnapshots mit exakten Vergleichen und modularen Bedingungen; positive Regeln, harte Ausschlüsse, deklarierte negative Evidenz und Kontextbedingungen. Bis zu 4096 Kandidaten werden stabil nach ID in Gruppen von 64 Bits geordnet. Regelspur und getrennte Masken erklären jede Entscheidung. Fachliche Gleichstände bleiben mehrdeutig. Bei unvollständiger Prüfung wird kein Gewinner ausgegeben.

Regeldatei: `BRUCE_RULES_V1`, dann `snapshot <id>`, anschließend Zeilen `rule <id> <positive|reject|negative|context> <x|y|z|value|layer> <eq|ne|lt|le|gt|ge|mod_eq> <arg1> <arg2> <weight> manual <evidence-id|->`. Vergleiche verwenden arg1, arg2 muss dabei 0 sein. `mod_eq` verwendet positiven Modulus arg1 und normierten Rest arg2. Nur positive Regeln besitzen Gewichte, begrenzt auf ±1000000. Negative Regeln verlangen eine benannte Referenz; diese ist deklarierte Eingabe, keine durch einen Experience Store beglaubigte Evidenz.

Kandidaten: `id,domain,layer,x,y,value`, Koordinaten und IDs uint64, Wert int64; z wird exakt abgeleitet. IDs sind innerhalb eines Laufs eindeutig. Domain und Layer gehören zur Adresse, erzeugen aber keine neue Rechenbedeutung. Kandidaten verschiedener Domains/Layer werden nicht gegeneinander gerankt; ein Lauf verlangt gemeinsamen Domain-/Layerkontext.

Grenzen: 128 Regeln, 4096 Eingabekandidaten, höchstens 1048576 Regelprüfungen, 65536 Regeltrace-Ereignisse. Kandidaten-, Prüf- und Tracebudgets erzeugen kontrolliertes `INCOMPLETE`. Abbruchposition wird für Replay gespeichert. Ein Prüflauf ist Auswahl, kein mathematischer Beweis; Learn-/Publish-Gates bleiben geschlossen.

Die CLI erhält `compatibility --rules DATEI --candidates DATEI` sowie `--from-run RUN --at X`. Letzteres replayt ein vorhandenes vollständiges Forschungspaket, übernimmt nur nicht widerlegte Hypothesen und wertet sie an X aus. Ihre Koordinaten sind `(a-min,b-min)`, ihr Wert ist `a*X+b`; das Ursprungspaket bleibt im neuen Export. Die mathematischen Status des Ursprungslaufs werden durch Ranking nicht hochgestuft. `replay` erkennt beide Paketversionen; bestehende Forschungsexporte bleiben bytegleich reproduzierbar.

## Schritte

- [x] Bestehende Tests als grüne Basis prüfen; neue End-to-End-Abnahmen zunächst rot nachweisen.
- [x] `compatibility` um immutable Snapshot, Prädikate, Maskengruppen und Trace erweitern.
- [x] Anwendungsablauf mit Dateieingabe, optionaler Forschungsprojektion, Export und Replay integrieren.
- [x] Grenzfälle, Manipulation, Abbruch und alte Replay-Pakete prüfen; unabhängigen Review durchführen.
- [x] Demo, Vertrag, Benutzung und tatsächlichen Status aktualisieren.

## Bewusste Grenze

Das ist die numerische Referenz der Z-Kompatibilität. Token-/Sprachrollen, gelernte Regelpromotion und extern beglaubigte Evidenzzahlen benötigen die späteren Experience-/Sprachmodule. Arbiträre Programmausführung in Regeln, BigInt, NanoCell und GPU sind nicht Teil dieses Pakets.

## Arbeitsnachweis

Ausgangssuite 2/2 grün. Der erste neue CLI-Test scheiterte vor der Implementierung an `UnknownCommand: compatibility`. Danach Engine, CLI und Export/Replay implementiert. Eine zusätzliche Ausgabeabnahme verlangte sichtbare Regeldefinitionen und scheiterte zunächst am fehlenden `rules`-Feld; nach Ergänzung sind Bedingungen und Evidenzreferenzen auch ohne Originaldatei in der Replay-Ausgabe sichtbar.

Abschluss: 4/4 CTest-Suiten in Debug und UBSan bestanden. Enthalten sind die bisherigen 4200 C++-Checks, acht bisherige und zwölf neue CLI-Tests sowie direkte Z-Abbruch-/Masken-/Replay-Prüfungen. Demo ausgeführt. Ein unabhängiger Code-Review fand keine konkreten Codefehler; seine Empfehlungen zu unsigned-z- und exakten Budgetgrenzen wurden als dauerhafte Tests aufgenommen. Der alte Tie-Break-Vertrag wurde mit der implementierten Mehrdeutigkeit abgestimmt.

Es existiert weiterhin kein gültiges Git-Repository in diesem Arbeitsordner; daher keine Branch-/Commit-/PR-Aktionen. [Abnahmebericht](acceptance/z-compatibility-v1-2026-09-22.md).
