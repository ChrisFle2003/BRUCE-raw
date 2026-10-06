# Replay-Eingangsbudget – Vertrag V1

Core 0.51 ergänzt die geprüfte Ausführung um ein optionales Bytebudget für das native Bundle, das dem Replay übergeben werden soll. Dies ist eine Zulassungsgrenze für Prüfdaten, kein gemeinsames Rechen-, Zeit- oder Speicherbudget.

## Ablauf und Grenze

`execute_checked_application(task, control, VerificationBudget{...})` übernimmt das Budget beim Aufruf als Wert. `max_replay_bytes` ist eine optionale vorzeichenlose 64-Bit-Zahl. Spätere Änderungen am Budget des Aufrufers, etwa in einem Abbruchcallback, ändern diese Sitzung nicht.

Nach Berechnung und Serialisierung beginnt die Phase REPLAYING mit der Budgetprüfung:

- `bundle.size() <= max_replay_bytes`: vollständiger bisheriger Replay einschließlich Ergebnisvergleich.
- Größeres Bundle: `ReplayByteBudgetExceeded: bytes=N limit=L`, verpackt als `ExecutionSessionError` mit `failed_at: REPLAYING` und Verlauf bis FAILED. `replay_application` wird nicht aufgerufen; es entsteht kein Session-Ergebnis und kein Export.
- Fehlendes Limit: bisheriges Verhalten. Native Bundlegrenzen gelten weiterhin.
- Limit 0: kein erzeugtes Bundle wird zum Replay zugelassen.

Gemessen wird die Länge des vollständigen nativen Bundles in Bytes, einschließlich Header, Digest, Trace und Ergebnis; nicht Zeichenanzahl oder JSON-Ausgabelänge. Die Prüfung gilt auch ohne Exportwunsch und für unvollständige native Befunde. REPLAYING bezeichnet hier auch die Zulassungsprüfung; sein Auftreten im Fehlerverlauf behauptet keine bereits ausgeführte Replayberechnung.

## CLI und Berichte

`--max-replay-bytes N` ist für dieselben acht Laufarten wie `--verification replay` verfügbar und setzt diesen Modus voraus. Ohne ihn oder zusammen mit `--verification none` wird `ReplayBudgetRequiresVerification` gemeldet. Ungültige, negative oder über UInt64 hinausgehende Angaben werden zurückgewiesen.

Bei erfolgreicher Sitzung mit explizitem Limit lautet der Berichtsvertrag `application-execution-v2`. Zusätzlich zu den unveränderten V1-Feldern enthält `execution`:

```json
"verification_budget": {"max_replay_bytes": 10000, "replay_bytes": 1234}
```

Die Zahlen sind beispielhaft. Ohne Limit bleibt der bisherige V1-Bericht unverändert. Budgetfehler nutzen den vorhandenen Fehlervertrag `application-execution-failure-v1`; die Ursache nennt tatsächliche Bytes und Limit. Native Bundleformate und fachliche Ergebnisse ändern sich nicht. Ein passender Teilbefund bleibt INCOMPLETE.

Das Budget gehört zum Bericht des aktuellen Aufrufs und wird nicht im nativen Bundle gespeichert. Ein späterer Replay dieser Datei belegt daher kein früher eingestelltes Sitzungsbudget.

## Aussagegrenze

Die ursprüngliche Berechnung, Quellenprüfung und Serialisierung sind zum Prüfzeitpunkt bereits erfolgt. Das Budget begrenzt weder deren Aufwand noch ihre Speicherbelegung. Es misst keine Anzahl von Replayoperationen, berücksichtigt keine expandierte Speichernutzung und begrenzt nicht die Laufzeit des zugelassenen Replay. Ein kleineres Bundle kann weiterhin teure Berechnungen auslösen. Native Suchbudgets bleiben eigenständig; unterbrechbare Verifikation und echte phasenübergreifende Kostensteuerung bleiben offen.

[Benutzung](107-replay-byte-budget-usage.md) · [Abnahme](acceptance/replay-byte-budget-v1-2026-09-23.md)
