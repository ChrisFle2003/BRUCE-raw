# Core 0.11: belegte Erfahrung für die Suchreihenfolge

Dieses Teilpaket implementiert einen begrenzten Routing-Erfahrungsspeicher. Es erfüllt nicht den gesamten allgemeinen ExperienceRecordV1-Vertrag aus `docs/contracts/03-evidence-and-experience.md`. Insbesondere werden keine mathematischen Regeln automatisch freigegeben und keine externen Nachweise authentifiziert.

## Speicher und Ereignisse

`BRUCE_EXPERIENCE_V1`, Policy `routing-experience-v1`: selbstständiger Snapshot mit maximal 1 MiB, 8 Quellläufen und 16 Ereignissen. Ein Add-Ereignis enthält den vollständigen Quellrun, laufende Sequenz und Digest des vorigen Ereignisses. Die Ereignis-ID ist SHA-256 des Ereignisinhalts. Ein Revoke-Ereignis referenziert einen vorhandenen aktiven Eintrag und enthält einen nichtleeren einzeiligen Grund mit höchstens 256 Bytes.

Add und Revoke erzeugen ausschließlich neue Ausgabedateien. Alte Ereignisinhalte bleiben im neuen Snapshot bytegleich erhalten; vorherige Dateien werden nicht geändert. Der alte Snapshot wird beim nächsten Schritt ausdrücklich angegeben. Es gibt keinen globalen Zeiger auf den neuesten Stand und keinen impliziten automatischen Schreibzugriff durch Forschung. Verzweigte Snapshot-Folgen sind möglich und bleiben getrennt.

Der Snapshot-Digest schützt die Integrität der vorliegenden Datei, nicht die Autorenschaft. Ein vollständig neu erzeugter, in sich gültiger Snapshot ist ein neuer Datenbestand. Widerruf löscht oder widerruft keine bereits exportierten historischen Forschungspakete.

## Zulässige Erfahrung

Es werden ausschließlich Portfolio-V2-/V3-Pakete akzeptiert, niemals V4-Pakete mit bereits eingebetteter Erfahrung. Vor Ausführung ihres Replays wird das tatsächlich geparste Operationsbudget auf höchstens 1000000 begrenzt. Dadurch entstehen keine rekursiv verschachtelten Erfahrungsläufe. Alle Versions-, Eingabe-, Status- und Traceprüfungen des Replays gelten unverändert.

Ein Quelllauf muss mit `Completed` und allen drei Teilstrategien enden, Hold-out enthalten und mindestens eine Hypothese mit `VERIFIED_ON_SAMPLES` besitzen. Kandidaten ohne Hold-out, unvollständige Läufe und vollständig widerlegte Läufe werden nicht als positive Routing-Erfahrung übernommen. Jede Strategie erhält pro Quelllauf höchstens eine Stimme, unabhängig von der Zahl ihrer Treffer.

Die Eingabeidentität ist SHA-256 der sortierten kanonischen Lernpaare. Derselbe Lernbestand darf nicht mehrfach gespeichert werden, auch nicht mit anderer Dateireihenfolge, anderen Grenzen oder nach Widerruf. Verschiedene Fingerprints beweisen keine statistische oder externe Unabhängigkeit; diese Daten sind ausschließlich lokale Sucherfahrung.

## Anwendung auf neue Suchen

`experience-order` ist nur für `portfolio-rational` mit explizitem Snapshot zulässig. Der Snapshot wird vor der neuen Forschung vollständig geprüft. Dann bestimmt die bisherige Formheuristik das Grundranking. Nur aktive Erfahrungen mit gleichem Grundmuster und identischen Domänen-/Koeffizienten-/Bit-/Grad-/Potenzgrenzen zählen. Identische aktuelle Lerndaten werden ignoriert. Aktuelle Hold-out-Daten beeinflussen dieses Ranking nicht.

Die drei Familien werden stabil nach absteigender Stimmenzahl sortiert; Gleichstand erhält die Reihenfolge der Formheuristik. Alle Familien bleiben Teil der Forschung. Die aktuelle Hypothese wird niemals aus einem gespeicherten Antwortwert übernommen. Alle bisherigen Prüfungen, Definitionslücken, Konflikte, Budgetgrenzen und Gesamtstatus gelten weiter. `learn_gate` und `publish_gate` bleiben geschlossen; nur die Ausführungsreihenfolge verändert sich.

## Ressourcen und Replay

Datensatz-Fingerprints und begrenzte Nachweisprüfung gehören zum Laden des Snapshots. Höchstens acht flache Quellruns mit jeweils höchstens 1000000 Operationsbudget werden je Ladevorgang replayt. Zusätzliche Formklassifikation pro Quelle ist auf vier Lernpaare begrenzt. `experience_audit_operations` weist die tatsächlich ausgeführten logischen Quell-/Formprüfoperationen separat aus; diese liegen außerhalb des aktuellen Forschungsbudgets. Dateilesen, Hashing und Textverarbeitung sind keine mathematischen Operationseinheiten. Ein Add-Befehl lädt den alten und prüft den neuen Snapshot; er kann daher mehrere solche Ladevorgänge enthalten.

Der anschließende Scan der maximal acht Einträge, die Stimmenaddition und Sortiervergleiche zählen in `ordering_operations` und `operations_used` der neuen Forschung. Bei Abbruch während der Planung können die ausgegebenen Stimmen-/Eintragszähler Teilstände enthalten; `strategy_order` bleibt bis zum Abschluss der Planung leer und der Gesamtstatus INCOMPLETE.

Der neue `BRUCE_PORTFOLIO_BUNDLE_V4` enthält den verwendeten Snapshot vollständig. Engine `bruce-portfolio-v4`, Regel `domain-union-v1`, Policy `shared-budget-v1`, Planer `experience-order-v1`. Replay benötigt weder aktuelle Storedatei noch Quellruns; er prüft den damaligen Stand erneut. Z-Projektion verwendet die tatsächliche neu geprüfte Funktion und Kontext `portfolio-v4-rational-v1`.
