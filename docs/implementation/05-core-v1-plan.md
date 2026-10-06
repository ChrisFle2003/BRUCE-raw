# BRUCE Core V1 – Umsetzung

Freigabe: Nutzerauftrag vom 22.09.2026, nach Vorlage der Roadmap. Ausführung direkt im Arbeitsordner, der kein gültiges Git-Repository enthält. Keine zusätzliche Architekturfreigabe nötig. Die allgemeine Roadmap wird mit diesem ersten begrenzten Durchstich begonnen, nicht vollständig abgeschlossen.

## Ausführbarer Umfang

BRUCE benötigt keinen vorgegebenen Aufgaben-/Antwortspeicher. Neue Eingaben und Parameter erzeugen zur Laufzeit Kandidaten. Persistenz ist ausschließlich ein ausdrücklich angeforderter, selbst enthaltener Replay-Export. Dieses Verhalten ist eine begrenzte mathematische Fähigkeit, keine bereits implementierte allgemeine Begriffsbildung oder beliebige Formelentdeckung.

C++20, CMake/Ninja, OpenSSL für SHA-256; Python nur für CLI-Integrationstests. Rechnen mit geprüften int64/uint64-Werten und breiteren Zwischenwerten auf GCC/Clang. Linux-Dateiexport mittels exklusiver atomarer Veröffentlichung. Kein Netzwerk, LLM oder Datenbankserver.

## Gemeinsame Verträge

- `core`: Fehler, Budgets, Samples und Aufgabenwerte; keine UI-/Store-Abhängigkeit.
- `coordinate`: Cantor-Adresse und Inverse; separates lokales 8×8-Modell und längenbehafteter Gridpfad bis Tiefe 64.
- `compatibility`: harte Ausschlüsse vor Ranking; gleichrangige Kandidaten bleiben mehrdeutig, IDs sortieren nur.
- `math`: geprüfte affine Rechnung, arithmetisch unabhängiger Rückcheck, Potenzen exakt/modular/metrisch; kein stiller Überlauf.
- `application`: begrenzte affine Suche, fachliche Ergebniszustände, logischer Trace und Replay-Orchestrierung.
- `evidence`: SHA-256, größenbegrenztes Lesen und exklusiver Export; keine Strategiewahl.
- `cli`: Eingabe, Anzeige und Aufruf der Anwendung.

Versionen sind im Export festgehalten. Unbekannte Versionen werden abgewiesen. Identisches Aufgabenpaket erzeugt identischen logischen Ablauf; Zeitmessungen gehören nicht zum Trace. Abbruch wird als Kandidatenposition eingefroren und replayt. Das Paket schützt gegen erkennbare Veränderung, bietet aber keine Signatur/Authentizität gegen einen Angreifer, der alles neu erzeugt.

Suchfamilie: `a*x+b`, ganzzahlige a,b in einem vom Benutzer vorgegebenen Intervall. Lernpaare und Hold-out haben getrennte x-Werte. Ein geprüfter Hold-out trägt nur den Status `VERIFIED_ON_SAMPLES`, niemals einen allgemeinen Formel-Beweis. Ohne Hold-out bleibt ein passender Kandidat `CANDIDATE`. Erschöpfte Budgets oder abgebrochene Runs sind `INCOMPLETE`. Bei mehreren gleichrangigen Kandidaten bleibt `AMBIGUOUS` sichtbar.

CLI-Pfade: `coordinate`, `grid`, `research`, `replay`. Forschung liest Dateien mit Header `x,y`; die linke Spalte ist Eingabe, die rechte erwartete Ausgabe. Andere Spaltennamen werden nicht still umgedeutet. Vorhandene Exportdateien werden niemals überschrieben.

## Umsetzungsschritte

- [x] 1. Unit- und CLI-Abnahmen schreiben; fehlende Implementierung als roten Ausgangspunkt nachweisen.
- [x] 2. Koordinaten, Mathematik und Kompatibilität implementieren; Grenzwerte und Mehrdeutigkeit prüfen.
- [x] 3. Dynamische affine Suche mit unabhängiger Gegenrechnung, Hold-out, Budgets und Trace implementieren.
- [x] 4. Kanonischen Replay-Export samt Integritätsprüfung und CLI ergänzen.
- [x] 5. Gesamtabnahme, zusätzlicher Code-Review und Startanleitung liefern; tatsächlichen Stand in den bestehenden Dokumenten korrigieren.

## Abnahme und Review-Fokus

1. Exakte Adressen bis uint64-Grenze; keine Wert-/Adressvermischung, Pfadtiefe 12 ohne Vollgitter.
2. Unterschiedliche zur Laufzeit erzeugte Datensätze; keine hart codierte Referenzformel, Hold-out darf nicht als Lernpaar eingehen.
3. Harte Ausschlüsse, gleichrangige Alternativen, fehlender Kandidat und ganzzahliger Überlauf.
4. Kandidaten-, Rechen-, Ergebnis- und Tracegrenzen; Abbruch öffnet keine Bestätigung.
5. Replay in neuem Prozess, unbekannte Version, geänderte Evidenz mit und ohne neu berechneten Digest, fehlende Dateien und exklusiver Export.

## Arbeitsnachweis

Erste Tests vor Produktcode angelegt; der Compileversuch scheiterte erwartungsgemäß am fehlenden `bruce/application/research.hpp`. Danach Implementierung und grüner Gesamtlauf.

Abnahme vom 22.09.2026: C++-Vertragsprüfung mit 4200 Checks und acht Python-CLI-Tests erfolgreich; CTest 2/2 in Debug und mit UndefinedBehaviorSanitizer erfolgreich. Demo und ein echter SIGINT-Abbruch samt exportiertem Replay geprüft. Details im [Abnahmebericht](acceptance/core-v1-2026-09-22.md).

Unabhängiger Code-Review meldete eine P2-Abweichung: ungültige Eingabebytes erzeugten ungültiges JSON. Reproduktion als neuer CLI-Test schlug fehl; UTF-8-validierendes Escaping behob den Fehler, Gesamtsuite anschließend grün. Review-Hinweis zur Hold-out-Auswahl wurde in Benutzungsdokumentation und Regression festgehalten. Keine weiteren offenen Review-Befunde.

Entscheidungen während der Umsetzung: geprüfte 64-Bit-Wertedomäne statt ungeprüfter BigInt-Versprechen; nur optionaler Runexport statt verpflichtender Datenbank; noch kein Gridcache oder vollständiger Z-Regelsnapshot. Diese Grenzen stehen in der Benutzungsdokumentation und lassen die zugehörigen Roadmap-Pakete offen. Die getrennten Hold-out-Paare dürfen Trainingsalternativen widerlegen; die Auswahl liefert ausschließlich eine Aussage über die geprüften Samples.

Git-basierte Worktree-/Commit-Hilfsskripte sind hier mangels gültigem Repository nicht anwendbar. Es wurden keine Commits, Branches, Pushes oder Pull Requests erstellt; Tests und Abnahmebericht bleiben im Arbeitsordner nachvollziehbar.
