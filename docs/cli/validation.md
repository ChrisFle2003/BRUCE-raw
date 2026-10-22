# CLI-Dokumentation: Validierung

[Handbuch](README.md)

- Geprüfte CLI-Version: 0.102.0.
- 202 dokumentierte CLI-Aufrufe ausgeführt; erwartete Exitcodes und gezielte fachliche Aussagen geprüft.
- Alle 97 im CLI-Quelltext gefundenen Optionsnamen sind im erklärenden Handbuch enthalten.
- 1115 Diagnose-Literale/Präfixe mit Quellverweis erfasst.
- Relative Markdown-Dateilinks im Handbuch geprüft.
- Eingaben, Argumente und vollständige JSON-Antworten stehen in `examples.json`.
- MCP-Beispiele prüfen ungültige Ports und nicht unterstützte Job-Argumente; HTTP, Loopback, getrennte V2/V3-Inspect-Routen, 1-MiB-V2-/5-MiB-V3-Grenzen, Replay und Metadatenschutz ohne Rohpayloads werden in den separaten MCP-CTest-Verträgen geprüft.
- NanoCell-Beispiele prüfen den äußeren Mod-Export, identisches Replay, `INCOMPLETE`, Kontextablehnung, ungültiges Hex und unbekannte Technik.
- Phase-DAG-V2-Beispiele prüfen einen seriellen Drei-Knoten-Spec mit zwei Kontexten, V2-Export, vollständigen Bundle-SHA-256, READY-Receipts, identische bestätigte inspect/replay-Ansichten ohne ursprüngliche Eingaben und die Ablehnung eines ungültigen Spec ohne Export.
- Phase-DAG-V3-Beispiele prüfen NanoCell → Payload-Gate → Payload-Gate, COMPLETE-Gateoutputs mit wiederverwendbaren Payloads, getrennte V3-Marker, vollständigen Bundle-SHA-256, READY-Receipts, identisches inspect/replay ohne Originaldateien und Reports ohne Payload-Rohbytes; ungültiger Spec, Versionsverwechslung in beide Richtungen, Korruption und neu gehashtes gefälschtes Receipt werden abgelehnt.
- Mod-Beispiele prüfen Data-Paketinspektion, explizites Testvertrauen, Built-in-Katalog, inaktive Installation, Status, Aktivierung, bytekompatibles V1-Routing, opt-in V2-Besitzdetails, identisches Replay, Deaktivierung und Ablehnung einer veralteten Registry-Version; Logic-Build, Update und Worker-Ausführung bleiben den dedizierten CTests vorbehalten.
- Trust-V2-Beispiele prüfen ausdrücklichen V1-Import, Generationenstatus, Prepare ohne Aktivierung, Verifikation, sicheren Wechsel und Rollback, blockierten Vertrauensverlust, explizite Notfalldeaktivierung, historische inkompatible Pakete, aktive Trust-Bindung trotz externer Datei und Parserfehler; ausschließlich öffentliche RFC-8032-Testschlüssel und isolierte temporäre Roots. Crash-Recovery, V1-Binary-Fence, Logic-Mod-Notfallrollback und laufende Worker werden durch dedizierte CTests geprüft, nicht durch diese Beispiele.
- TaskSpec-Beispiele prüfen explizite Rollen, Asset-Digests, native Evidenz ohne Lern-/Publikations-Gates, identisches Replay und Ablehnung eines Spec als Lauf.
- Negative Beispiele prüfen Parserfehler, doppelte Optionen, Holdout-Überlappung, BigInt-Division, Exportverweigerung, die drei Sitzungsbudgets sowie ungewählte und nicht unterstützte Spracheingaben.
- Signaloptionen wurden auf gültige Konfiguration und erfolgreiches Replay geprüft; diese Dokumentationsprüfung misst keine Signallatenz und injiziert keine Signale.
- Kein vollständiger Test jeder möglichen Optionskombination und kein Ersatz für die Core-Testsuite.

Reproduktion vom Projektstamm:

```bash
bash docs/cli/tutorial.sh ./build/bruce
python3 docs/cli/verify_examples.py ./build/bruce
```

Der Python-Aufruf regeneriert Options-/Diagnoseindex, Beispielantworten und diese Notiz. Die erklärenden Kapitel werden nicht automatisch neu geschrieben.
