# Phase-DAG V3 — Begrenzter paralleler Scheduler

## Status und Ziel

**VORSCHLAG, zur Implementierung freigegeben:** Phase-DAG V3 erhält eine optionale begrenzte Parallelität für unabhängige Knoten. Der V3-Spec- und Bundlevertrag bleibt unverändert. Ein serieller Lauf bleibt Standard und produziert die bisherigen kanonischen Bytes.

Das Ziel ist, bis zu vier unabhängige Adapteraufrufe je Welle gleichzeitig auszuführen, ohne dass Betriebssystemplanung oder Abschlusszeit die Receiptreihenfolge, Tracefolge, Bundlebytes oder Replay beeinflussen.

## Öffentlicher Vertrag

`PhaseDagControlV3` erhält `max_parallel_nodes` mit Standardwert 1. Zulässig sind ausschließlich die Werte 1 bis 4. Werte außerhalb dieses Bereichs werden vor der Ausführung mit einem kontrollierten Fehler abgelehnt.

`execute_phase_dag_v3` behält seine bestehende Signatur. `pack_phase_dag_run_v3` und `replay_phase_dag_run_v3` behalten ihr Format und ihre Signaturen. Die Parallelitätszahl gehört nicht zum Bundle: Sie ändert nur die lokale Ablaufplanung, nicht das fachliche Ergebnis.

Die CLI ergänzt ausschließlich für `bruce phase-dag-v3 execute` den optionalen Schalter `--max-parallel-nodes N`, mit N von 1 bis 4 und Standard 1. `inspect` und `replay` akzeptieren ihn nicht. Ein CLI-Lauf mit N größer 1 muss dasselbe Bundle wie ein serieller Lauf derselben Spec erzeugen.

## Deterministische Wellen

1. Der Scheduler ermittelt alle aktuell ausführbaren Knoten in lexikographischer Knoten-ID-Reihenfolge.
2. Er bildet daraus eine Welle der ersten höchstens `max_parallel_nodes` Knoten.
3. Adapter einer Welle können gleichzeitig laufen. Sie erhalten nur direkte, bereits abgeschlossene Vorgängerinputs.
4. Der Scheduler wartet auf alle Adapter der Welle und verarbeitet deren Resultate anschließend wieder in der festen Wellenreihenfolge.
5. Erst nach dieser serialisierten Übernahme werden Nachfolger für die folgende Welle ausführbar.

Receipts bleiben in der kanonischen Reihenfolge der Spec-Knoten. Traceeinträge folgen der logischen, nicht der zeitlichen Reihenfolge: VALIDATED, danach pro Welle STARTED und Abschlussereignis in Wellenreihenfolge, gefolgt von COMPLETED nach den bestehenden Grenzregeln.

## Fehler, Budgets und Cancellation

Adapterfehler, ungültige Outputs, Receiptbudgetüberschreitungen und nicht bereite Vorgänger folgen exakt den bestehenden FAILED-/BLOCKED-Regeln. Ein Fehler eines Knotens beeinflusst andere unabhängige Knoten derselben bereits gestarteten Welle nicht; abhängige Nachfolger werden erst in einer späteren Welle BLOCKED.

Cancellation wird vor einer Welle geprüft. Bei angeforderter Cancellation wird keine neue Welle gestartet; noch nicht gestartete Knoten erhalten die bestehenden CANCELLED-Receipts. Bereits gestartete Adapter werden nicht abgebrochen und werden vollständig, kanonisch übernommen.

Der Scheduler erzeugt höchstens vier gleichzeitige Adapteraufrufe und keine Worker, Warteschlangen, Prozesse oder externen Dienste. Die festen NanoCell- und Payload-Gate-Adapter müssen dabei ohne geteilten veränderlichen Schedulerzustand aufgerufen werden.

## Replay und Grenzen

Replay führt weiterhin mit serieller Standardparallelität aus. Weil Resultatübernahme, Receiptordnung und Traceordnung deterministisch sind, muss der rekonstruierte Lauf bytegleich zum parallel erzeugten Bundle sein. Ein Bundle enthält keine Laufzeit-, Thread- oder Timingdaten.

V2, Cockpit-Inspection, Payloadlimits, Bundlegrenzen, Adapterkatalog und V3-Cockpit-Routen werden durch diesen Ausbau nicht verändert.

## Testbare Abnahme

1. Eine Welle unabhängiger Testadapter erreicht die konfigurierte maximale Gleichzeitigkeit, aber nie mehr als 4.
2. Maximalwerte 0 und 5 werden kontrolliert abgelehnt; der ausgelassene Wert bleibt 1.
3. Seriell und parallel erzeugen für dieselbe mehrwellige Spec exakt gleiche Bundles, Receipts und Trace.
4. Ein Fehler in einer parallelen Welle lässt unabhängige Knoten fertig werden und blockiert erst abhängige Knoten in einer späteren Welle.
5. Cancellation vor einer Welle startet keine weiteren Adapter und bleibt replaybar nach den bestehenden Regeln.
6. CLI akzeptiert den Schalter nur für V3 execute; ungültige Werte und sein Gebrauch bei inspect/replay sind kontrollierte Fehler.
7. V2- und bestehende V3-Verträge bleiben grün.

## Außerhalb des Umfangs

Nicht Bestandteil sind verteilte Ausführung, unbegrenzte Threads, Prioritäten, Work-Stealing, dynamische Adapter, persistente Queues, Cockpit-Live-Streaming, GPU-Ausführung und Änderungen an V2.
