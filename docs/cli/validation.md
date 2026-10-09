# CLI-Dokumentation: Validierung

[Handbuch](README.md)

- Geprüfte CLI-Version: 0.102.0.
- 150 dokumentierte CLI-Aufrufe ausgeführt; erwartete Exitcodes und gezielte fachliche Aussagen geprüft.
- Alle 84 im CLI-Quelltext gefundenen Optionsnamen sind im erklärenden Handbuch enthalten.
- 635 Diagnose-Literale/Präfixe mit Quellverweis erfasst.
- Relative Markdown-Dateilinks im Handbuch geprüft.
- Eingaben, Argumente und vollständige JSON-Antworten stehen in `examples.json`.
- NanoCell-Beispiele prüfen den äußeren Mod-Export, identisches Replay, `INCOMPLETE`, Kontextablehnung, ungültiges Hex und unbekannte Technik.
- Runtime-Daten-Mod-Beispiele prüfen explizites Testvertrauen, inaktive Installation, Aktivierung, Route-Export, identisches Replay, Deaktivierung und Ablehnung einer veralteten Registry-Version.
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
