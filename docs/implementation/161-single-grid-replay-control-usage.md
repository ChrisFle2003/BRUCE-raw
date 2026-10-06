# Core 0.81: Replaysteuerung für einzelne Grid-Läufe

```bash
./build/bruce gate-run tests/fixtures/core-036-grid-v1.bruce --max-session-checkpoints 1000 --cancellation checkpoints
```

`replay_grid(bundle, checkpoint)` akzeptiert nun denselben optionalen Replaycallback wie die übrigen unterstützten Adapter. Der allgemeine Replayadapter reicht ihn für `BRUCE_GRID_BUNDLE_V1` durch. Die frühere Ablehnung `ReplayCheckpointsUnsupported` entfällt für diesen Bundletyp.

Prüfpunkte liegen im bestehenden blockweisen SHA-256-Pfad (64 KiB und Abschluss) sowie unmittelbar vor und nach der Grid-Auswertung. Callbackexceptions werden weitergereicht; es gibt keinen Teilbefund. Ohne Callback bleiben Ergebnis, Bundlebytes und Replayregeln erhalten.

Parsing, Inhaltsvergleich und die eigentliche Grid-Auswertung sind weiterhin zusammenhängende Arbeitsschritte. Es gibt keine maximale Abbruchlatenz, kein neues mathematisches Operationsbudget und keine Unterbrechung innerhalb der Grid-Berechnung.

In `gate-run` sind damit Prüfpunktbudget und Signalsteuerung auch für einzelne Grid-Läufe verwendbar. `applicable` bleibt false, da ein Grid-Ergebnis keine Forschungsauswahl oder Erfahrungsquelle darstellt. Die unterstützte Steuerung erteilt keine neuen fachlichen Befugnisse.

Die Tests prüfen bytegleichen Replay, Abbruch an jedem erreichten Prüfpunkt, unveränderte Gateausgabe, exakt ausreichendes Budget und Überschreitung. Historische Aussagen zu fehlender Grid-Replaysteuerung in Core 0.77–0.80 werden durch dieses Paket fortgeschrieben.

Weitere feinere Abbruchpunkte, allgemeine Speicherbudgets und automatische Ergebnisweiterleitung bleiben offen. Optimierung bleibt nachrangig.

## Abnahme am 25.09.2026

Lokal und remote bestanden 108/108 Testgruppen. Der Nutzerpfad `./build/bruce` auf Bruce-AI meldet 0.81.0; der oben angegebene Grid-Aufruf lief erfolgreich mit 4 Prüfpunkten. Installation und TGZ-Paketprüfung bestanden. Die CLI-Prüfung bestand lokal und remote mit 92 Beispielaufrufen und gültigen internen Links. Quellen und Dokumentation wurden per SCP übertragen.
