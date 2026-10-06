# Begrenzte und unterbrechbare Grid-Stapel – Core 0.41

## Ausführungsgrenze

`grid-batch --max-query-steps N` begrenzt die Zahl abgearbeiteter Abfragen auf 0–1024. Ein Schritt ist eine abgefragte Adresse: Cachetreffer und lokale Sentinel-Stopps zählen ebenfalls. Die Zahl tatsächlich berechneter Blattwerte bleibt separat in `evaluations` sichtbar.

Die Anwendungs-API `evaluate_grid_batch` akzeptiert zusätzlich einen Abbruchcallback. Vor jeder Abfrage erhält dieser die Zahl bereits bearbeiteter Abfragen. Die CLI verbindet SIGINT/SIGTERM mit diesem Callback. Der Abbruch erfolgt zwischen Abfragen; eine laufende einzelne Abfrage wird nicht teilweise gespeichert.

Vor dem ersten Schritt gelten weiterhin Spec-, Anzahl-, Eingabebyte- und Cachegrenzen. Bei einem begrenzten oder callbackfähigen Lauf wird die **Syntax aller ursprünglichen Pfade** vorab geprüft. Ein fehlerhafter Restpfad wird auch bei Schrittbudget null nicht als replaybare Aufgabe akzeptiert. Die mathematische Auswertbarkeit noch nicht bearbeiteter Werte wird dadurch nicht bestätigt; beispielsweise kann eine zu kleine exakte Bitgrenze erst bei ihrer Auswertung scheitern.

## Reihenfolge und Status

An jeder Abfragegrenze gilt:

1. Eine gespeicherte Abbruchposition oder ein aktiver Callback beendet den Lauf mit `INCOMPLETE / Cancelled`.
2. Andernfalls beendet eine erreichte Schrittgrenze den Lauf mit `INCOMPLETE / QueryBudgetExceeded`.
3. Andernfalls wird die Abfrage vollständig ausgewertet, aus dem Cache bedient oder lokal gestoppt.

Sind alle Abfragen abgearbeitet, ist der Stapel `COMPLETED / Completed`. Es gibt nach der letzten Abfrage keinen weiteren Callback. Eine Grenze genau bei der Listenlänge macht den fertigen Lauf nicht unvollständig. Bei gleichzeitigem Abbruch und Budgetende vor einer noch ausstehenden Abfrage hat der Abbruch Vorrang.

Ein lokaler `(0,0)`-Stopp bleibt auf seine einzelne Abfrage begrenzt und beendet den Stapel nicht. Schrittbudget null liefert einen leeren Teilbefund mit null Berechnungen und leerem Cache. Strukturfehler und Rechenfehler bleiben Fehler ohne Export, keine verschleierten Teilbefunde.

## V2-Export und Replay

Ein normaler Lauf ohne Schrittgrenze und ohne tatsächlich ausgelösten Abbruch behält das bisherige `BRUCE_GRID_BATCH_BUNDLE_V1`-Format. Eine gesetzte Grenze oder gespeicherte Abbruchposition verwendet V2 mit `query-boundaries-v1`:

- ursprüngliche vollständige geordnete Pfadliste und Spec;
- optionales Schrittbudget und optionaler Abbruchindex;
- ausschließlich die vollständig abgearbeiteten Einzelergebnisse;
- Cacheentscheidungen und Statistiken dieses Präfixes;
- abschließendes `END`-Trace-Ereignis mit Status, Grund und bearbeiteter Anzahl.

Die V2-Ausgabe ergänzt `stop_reason`, `processed_queries` und `requested_queries`. Ein Teilbefund behauptet keine Bearbeitung der restlichen Liste. Ein V2-Header ohne Ausführungsgrenze oder gespeicherte Abbruchposition ist unzulässig.

Replay startet mit leerem Cache, arbeitet dasselbe Präfix ab und prüft danach die gesamte kanonische Payload. Es benötigt kein neues Betriebssystemsignal. Veränderte Grenzen, Gründe, Zähler, Ergebnisse oder Versionen werden bei Widerspruch auch nach neuer Digestberechnung verworfen. Die vorhandenen Gesamtdateigrenzen gelten weiter; vorhandene Exporte werden nicht überschrieben.

V2 ist kein gespeicherter Executorzustand zur Fortsetzung ab dem nächsten Schritt. Ein erneuter Auftrag mit größerem Budget startet weiterhin von vorn. Digests bestätigen keine externe Urheberschaft.

## Grenze zur Forschung

`research --grid-train/--grid-holdout` akzeptiert vollständig abgearbeitete V2-Stapel nach ihrem Replay. Jede unvollständige Trainings- oder Holdout-Quelle wird mit `IncompleteGridSource` abgewiesen. Der sichtbare Anfang einer abgebrochenen Liste wird nicht stillschweigend zur vollständigen Forschungsgrundlage erklärt.

Die V1-/V2-/V3-Forschungsexporte, Samplebindung und Z-Auswahl behalten ihre bisherigen Regeln. Beim optionalen Gesamtbudget zählt weiterhin die tatsächliche Größe der Quellexporte, einschließlich der V2-Metadaten.

Die **Quellenprüfung innerhalb eines Forschungsauftrags bleibt atomar**. Dieser Ausbau macht den eigenständigen Grid-Stapelexecutor unterbrechbar, nicht sämtliche Phasen der Forschung. Inkrementelle Quellenprüfung, Wiederaufnahme, allgemeine Phasen-/Gate-Verträge und Expansion bleiben offen. Kein vollständiger M2-Abschluss.

[Benutzung](87-grid-partial-usage.md) · [Abnahme](acceptance/grid-partial-v1-2026-09-22.md)
