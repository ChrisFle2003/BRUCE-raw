# Grid-Quellen für exakte Forschung – Core 0.38

## Umfang und Herkunft

Die bestehende exakte Forschung akzeptiert mit `--grid-train` und optional `--grid-holdout` geprüfte `BRUCE_GRID_BATCH_BUNDLE_V1`-Exporte als Datenquelle. Die Strategien, Suchbudgets, Gegenrechnung und Statusregeln bleiben unverändert. Die Anwendungs-API heißt `research_from_grid`.

Diese erste Verbindung untersucht eine feste Scheibe des bestehenden lokalen UInt64-Grids. Sie ist kein allgemeiner Grid-Generator und kein neuer Formelplaner. Die Grid-Wertregel ist bereits bekannt; eine aus ihren Werten wiedergefundene Formel ist keine Entdeckung einer bisher unbekannten mathematischen Regel.

## Projektion `fixed-slice-leaf-x-v1`

Jede Quelle wird vor Verwendung vollständig replayt. Akzeptiert werden ausschließlich exakte, erfolgreich ausgewertete Blattwerte. Modulare Werte, Metriken und `STOPPED`-Abfragen werden zurückgewiesen, nicht übersprungen.

- Forschungseingabe: X-Koordinate des letzten Pfadpaars, also eine ganze Zahl von 0 bis 7.
- Forschungsausgabe: exakt berechneter Grid-Wert, verlustfrei in die vorhandene exakte Zahlendomäne übernommen.
- Alle Abfragen haben denselben normalisierten Vorfahrenpfad und denselben letzten Exponenten Y.
- Training und Holdout haben dieselbe vollständige GridSpec einschließlich Bitgrenze und `0^0`-Regel. Cachebudgets dürfen sich unterscheiden; sie ändern nicht die mathematische Spec.
- Die ursprüngliche Reihenfolge bleibt erhalten. Doppelte X-Eingaben innerhalb einer Quelle oder über Training/Holdout hinweg werden abgewiesen, auch bei unterschiedlichen Schreibweisen derselben Adresse. Insgesamt sind dadurch höchstens acht verschiedene Eingaben möglich.
- Holdout ist optional. Wird in der CLI ausdrücklich eine Holdout-Datei angegeben, muss sie ein gültiger, nicht leerer Grid-Stapelexport sein.

Eine Eingabe `1,1/2,3` bedeutet daher `x=2`, Zielwert `8`, Präfix `1,1`, Exponent `3`. Weder Zeilennummer noch Wert werden als Adresse oder Forschungseingabe umgedeutet. Der Exponent dient nur zur Prüfung des gemeinsamen Ausschnitts; er wird der Suchstrategie nicht als Lösungsformel übergeben.

## Integration und Grenzen

CSV- und Grid-Eingaben werden innerhalb eines Auftrags nicht gemischt. `--grid-holdout` erfordert `--grid-train`. Grid-Quellen benötigen eine bestehende exakte Domäne (`bigint` oder `rational`); explizites `int64` wird abgewiesen. Bei affiner Forschung mit Grid-Quelle ist der Standard `bigint`, während der bisherige CSV-Standard unverändert bleibt.

Alle bisherigen Prüfungen der jeweiligen Strategie bleiben aktiv. Die API nimmt eine `ExactResearchTask` ohne bereits gesetzte Samples entgegen und setzt ausschließlich die geprüften Grid-Paare ein. Bereits gesetzte Samples sind ein Fehler. Sie führt die bestehende Forschung einschließlich Abbruchcallback aus.

Jede Quelldatei ist auf 2 MiB begrenzt. Der innere Grid-Replay besitzt seine bisherigen Abfrage-/Pfad-/Cachegrenzen; danach gelten die bestehenden Zahlen- und Forschungsbudgets. Diese sind getrennte Grenzen: Die Forschungsoperationszahl enthält nicht die vorgelagerte Grid-Quellenprüfung. Es gibt noch kein gemeinsames phasenübergreifendes Arbeitsbudget oder einen abbrechbaren Grid-Quellenreplay. Der äußere Export bleibt auf die vorhandenen 32 MiB begrenzt.

Fehler erzeugen keinen Export. Bestehende Dateien werden nicht überschrieben. Budget- und Benutzerabbrüche der Forschung behalten `INCOMPLETE` samt reproduzierbarer Abbruchposition; sie werden nicht zu vollständigen Ergebnissen aufgewertet.

## Forschungsexport und Replay

`BRUCE_GRID_RESEARCH_BUNDLE_V1` enthält:

1. Engine `bruce-grid-research-v1` und Projektionsversion.
2. Längenbegrenzte vollständige Trainings- und optional Holdout-Bundles.
3. Das unveränderte versionierte Bundle der ausgeführten exakten Forschung.
4. Eine äußere Ergebnisdarstellung mit Status, Quellen-Digests, kanonischem Präfix, Exponent, Eingabebereich und innerem Forschungsbefund.

Replay validiert die Grid-Quellen, leitet die geordneten Samples erneut ab und replayt die innere Forschung. Anschließend müssen deren Trainings- und Holdout-Paare **genau** mit den abgeleiteten Paaren übereinstimmen. Zwei jeweils gültige, aber nicht zusammengehörige Quell- und Forschungsbundles sind unzulässig. Danach wird die vollständige äußere Payload kanonisch verglichen.

Auch nach Löschen der ursprünglichen Dateien bleibt der Auftrag replaybar. Digests sind Integritätsmerkmale, keine Herkunftssignaturen. Eine konsistent neu erzeugte andere Berechnung bleibt ein anderer gültiger Auftrag; ein Replay ist kein allgemeiner mathematischer Beweis.

## Z-Auswahl und Aussagegrenzen

`compatibility --from-run` akzeptiert den neuen Forschungsexport, prüft ihn vollständig und behält den gesamten Wrapper als Herkunft im Auswahlbundle. `--at` ist für diese Quelle auf 0–7 beschränkt. Die Auswahl wertet eine gefundene Hypothese aus; sie führt an einer neuen Eingabe nicht automatisch eine zusätzliche Grid-Gegenprüfung durch. Der ursprüngliche Forschungsstatus bleibt erhalten. Unvollständige Forschung kann wie bisher nicht gerankt werden.

`VERIFIED_ON_SAMPLES` betrifft nur die tatsächlich verwendeten Paare. Holdout-Werte stammen aus separat angegebenen Grid-Abfragen, nicht aus externer Evidenz; bestehende Holdout-Auswertung kann konkurrierende Modelle widerlegen. Extrapolation, allgemeiner Beweis und Lernen werden daraus nicht abgeleitet.

Experience-Import des neuen Wrappers ist noch nicht implementiert. Ebenso offen bleiben andere Projektionen, mehrere variable Grid-Koordinaten, Expansion, BigInt im Grid/Z, umfassende Aststeuerung sowie ein gemeinsamer Aufgaben-/Strategie-/Gate-Vertrag. Kein vollständiger M2-Abschluss.

[Benutzung](81-grid-research-usage.md) · [Abnahme](acceptance/grid-research-v1-2026-09-22.md)
