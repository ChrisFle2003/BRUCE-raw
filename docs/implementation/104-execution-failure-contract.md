# Strukturierte Sitzungsfehler – Vertrag V1

Core 0.50 erweitert die optionale geprüfte Ausführung um einen typisierten Fehlerbericht. Ziel ist, Fehlerphase und Ursache ohne Auswertung eines Meldungstexts zu untersuchen. Die vorhandenen acht Auftragstypen, Erfolgsberichte und nativen Bundles bleiben unverändert.

## Bibliotheksvertrag

`execute_checked_application` fängt weiterhin von `std::exception` abgeleitete Fehler aus EXECUTING, SERIALIZING und REPLAYING ab. Es wirft nun `ExecutionSessionError`, abgeleitet von `Error` und `std::nested_exception`:

- `failed_at()` enthält die Arbeitsphase, in der die Exception auftrat.
- `phases()` enthält den durchlaufenen Präfix bis zum terminalen FAILED.
- `cause()` enthält die ursprüngliche `what()`-Meldung.
- `what()` behält die bisherige Form `ExecutionSessionFailed at PHASE: Ursache`.
- `std::rethrow_if_nested` erlaubt den Zugriff auf die ursprüngliche Exception einschließlich ihres Typs. Auch beim Speichern der äußeren Exception mit `std::exception_ptr` bleibt sie erhalten.

Der Fehlerkonstruktor ist intern. Der Controller stellt den Verlauf zusammen; Aufrufer erhalten nur lesenden Zugriff. Nicht von `std::exception` abgeleitete Throws, Prozessabbruch und fehlender Speicher für die Fehlerbehandlung haben keine neue Berichtsgarantie.

## CLI-Vertrag

Nur bei einer solchen Sitzungs-Exception ergänzt die CLI auf stderr das folgende Objekt; Exitcode 2 und das bestehende `error`-Feld bleiben erhalten:

```json
{
  "error": "ExecutionSessionFailed at EXECUTING: InvalidGridPath: expected x,y/x,y",
  "execution": {
    "contract": "application-execution-failure-v1",
    "state": "FAILED",
    "failed_at": "EXECUTING",
    "phases": ["CREATED", "EXECUTING", "FAILED"],
    "cause": "InvalidGridPath: expected x,y/x,y"
  }
}
```

Es gibt dabei kein `result`, keine Behauptung bestandener Replayprüfung und keinen Exportversuch. Bereits bestehende Zieldateien bleiben erhalten. Ein nativer INCOMPLETE-Befund ist weiterhin ein reguläres Ergebnis und kein Sitzungsfehler.

Optionenprüfung, Dateieinlesen und Auftragsaufbau vor `execute_checked_application` bleiben außerhalb der Sitzung. Ebenso bleiben Ausgabeformatierung und Dateischreiben nach der Rückgabe außerhalb. Ihre Fehler behalten die allgemeine Fehlerhülle ohne erfundene Phasenfolge. Ein vorhandenes Exportziel kann deshalb trotz zuvor erfolgreicher Prüfung einen gewöhnlichen Exportfehler erzeugen.

## Grenzen

Der Bericht ist eine Diagnose des aktuellen Aufrufs, kein persistierter oder kryptografisch geprüfter Beleg. Er erlaubt weder Wiederaufnahme noch automatischen Retry. Replay bleibt atomar, die Sitzung hat weiterhin kein gemeinsames Kostenbudget und keinen allgemeinen Phasengraphen. M0–M2 werden nicht pauschal abgeschlossen.

[Benutzung](105-execution-failure-usage.md) · [Phasenvertrag](102-execution-phases-contract.md) · [Abnahme](acceptance/execution-failure-v1-2026-09-22.md)
