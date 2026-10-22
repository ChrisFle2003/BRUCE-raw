# Common Runtime Compatibility V1 — Stufe-1-Management-Adapter

## Ziel

Die bestehende Management-Familie über beide read-only CLI-Routen explizit
bestätigbar machen, ohne Legacy-Ausgaben oder Management-Semantik zu ändern.

## Aufgaben

1. Tests zuerst für `experience inspect` und `rules-status` ergänzen.
2. Den gemeinsamen Management-Optionsparser um die Familienbestätigung
   erweitern; nur `management` akzeptieren.
3. Beide Aufrufstellen vor Datei-/Storezugriff validieren.
4. Hilfe, CLI-Dokumentation und generierte Artefakte über den bestehenden
   Verifier aktualisieren.
5. Fokusberichte, lokale Links, JSON-Validierung und Sicherheitsprüfung
   ausführen.

## Nichtziele

- keine gemeinsame Runtime;
- kein Descriptor-Output;
- keine Session-/Transfer-Adapter;
- keine Änderung an Managementbudget, Cancellation, Fehlerhülle oder
  Read-only-Verhalten.
