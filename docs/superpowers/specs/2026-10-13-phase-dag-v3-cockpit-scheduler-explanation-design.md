# V3 Cockpit Scheduler-Erklärung — Design

**VORSCHLAG, zur Implementierung freigegeben:** Der read-only V3-Cockpit ergänzt eine aus Replaydaten abgeleitete Scheduler-Erklärung. Bundle, CLI, Routen und Payloadschutz bleiben unverändert.

## Ziel

Ein hochgeladenes V3-Bundle soll erklären, warum jeder Knoten READY, FAILED, BLOCKED oder CANCELLED ist. Die Darstellung bleibt portabel, denn sie verwendet nur replay-verifizierte Specs, Receipts und den kanonischen Trace.

## Antwortmodell

Die erfolgreiche V3-JSON-Antwort erhält ein zusätzliches Feld `scheduler_explanation` mit:

- `trace_model: canonical-logical-v1`;
- `parallelism_recorded: false`;
- pro Knoten: node_id, outcome, failure_code, dependencies und `blocking_predecessors`.

`blocking_predecessors` enthält nur direkte Vorgänger, deren Receipt nicht READY ist. Für READY-, FAILED- und CANCELLED-Knoten ist die Liste leer. Für BLOCKED-Knoten enthält sie in deklarierter Abhängigkeitsreihenfolge node_id, outcome und failure_code des blockierenden Vorgängers.

Die Erklärung enthält keine Payloadbytes, Requestbytes, Evidencebytes, Thread-IDs, Zeitstempel, Wellenbreiten oder Performancewerte. Die UI zeigt deutlich, dass der Trace logisch kanonisch ist und die verwendete Parallelitätsgrenze nicht im Bundle gespeichert wird.

## UI

Die bestehende V3-Ansicht erhält eine sechste text-sichere Sektion Scheduler explanation. Sie rendert ausschließlich die Projektionsdaten mit textContent. V2 erhält keine neue Scheduler-Behauptung und behält seine Ansichten.

## Abnahme

1. Mehrstufige Fehlerketten zeigen nur direkte nicht-READY-Vorgänger je BLOCKED-Knoten.
2. READY-, FAILED- und CANCELLED-Knoten behaupten keine Blockierung.
3. Reihenfolge entspricht den deklarierten Dependencies und nicht Thread-Timing.
4. JSON und DOM enthalten keine Rohbytes oder Timing-/Parallelitätsbehauptungen.
5. V2, V3-Route, Bundleformat und Replay bleiben unverändert.
