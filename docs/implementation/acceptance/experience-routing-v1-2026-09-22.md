# Abnahme Routing Experience Store / Core 0.11 – 22.09.2026

## Lieferung

Optionaler Erfahrungspfad `experience add/revoke/inspect` mit unveränderlichen, explizit benannten Snapshots. Replay-geprüfte V2-/V3-Portfolioläufe liefern höchstens eine positive Stimme pro Strategie. Gleiche kanonische Lerndaten werden nicht mehrfach aufgenommen. Widerruf ergänzt ein Ereignis und entfernt zukünftigen Planungseinfluss beim Verwenden des neuen Snapshots. `experience-order` nutzt passende Erfahrung als Reihenfolgehilfe, berechnet die neue Aufgabe vollständig erneut und exportiert den verwendeten Stand im V4-Paket.

Dies ist ein begrenztes Teilpaket des allgemeinen Experience-Vertrags. Learn-/PublishGate bleiben geschlossen; keine automatische Regelpromotion oder externe Evidenzauthentifizierung.

## Ausgeführte Prüfungen

```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure -j4
cmake --build build-ubsan -j4
ctest --test-dir build-ubsan --output-on-failure -j4
python3 scripts/demo.py
```

**20 CTest-Suiten bestanden**, im Debug-Build und mit UndefinedBehaviorSanitizer (`-fsanitize=undefined -fno-sanitize-recover=all`); keine Sanitizer-Meldung. C++ mit `-Wall -Wextra -Werror`. Sieben neue CLI-Tests und eine C++-Suite. Der erste CLI-Test wurde vor Implementierung mit `UnknownCommand: experience` rot nachgewiesen.

Geprüfte Fälle:

- Quelllauf übernehmen, Kontextstimmen tatsächlich für eine neue Aufgabe verwenden, vollständige Hypothesen bleiben gleich;
- Replay des V4-Pakets ohne ursprüngliche CSV-, Store- oder Quelllaufdateien;
- gleiche Lerndaten und umsortierte gleiche Lernpaare werden als Duplikat abgewiesen;
- keine positive Aufnahme ohne Hold-out oder aus unvollständiger Suche;
- gleicher aktueller Lernbestand erhält keine Stimme aus sich selbst;
- Append erhält vorherige Dateien und bestehende Ereignis-IDs;
- Widerruf erhält historische Daten, doppelte Widerrufe scheitern, vollständig widerrufene Erfahrung fällt auf die Formheuristik zurück;
- vorhandene Exportdateien werden nicht überschrieben;
- andere Konfiguration erhält keine passende Stimme; aktuelle Hold-out-Werte ändern Reihenfolge/Stimmen nicht;
- frühe positive Teilhypothese bei Kandidatenlimit hält Gesamtstatus INCOMPLETE;
- sämtliche globalen Checkpoints und alle kleineren positiven Operationsgrenzen des Referenzlaufs mit identischem Replay;
- gespeicherte Nachweisarbeit wird separat ausgewiesen; eigentliche Stimmenplanung bleibt im neuen Forschungsbudget;
- acht Quellen und 16 Ereignisse funktionieren; Überschreitungen und zu hohes Quell-Operationsbudget werden abgewiesen;
- manipulierte eingebettete Evidenz wird trotz neuem äußerem Digest erkannt;
- V4-Läufe können nicht rekursiv als neue Quellen aufgenommen werden;
- Z-Projektion des neu geprüften Ergebnisses, Polstellenfehler und Auswahl-Replay ohne Ursprungsdateien;
- gesicherter tatsächlicher Core-0.10-V3-Export und alle bisherigen Fixtures bleiben identisch replaybar.

## Review und Korrektur

Das unabhängige Review fand einen Fehler in der ersten Quellbudget-Vorprüfung: Eine führende Leerstelle am echten Operationsfeld zusammen mit einem zusätzlichen Scheinfeld konnte die Textsuche umgehen. Die Quelle wurde zwar später als nichtkanonisch abgewiesen, aber erst nach erneuter Ausführung.

Korrektur: Der Replaydecoder prüft jetzt das tatsächlich geparste Budget gegen die vom Experience-Loader vorgegebene Grenze unmittelbar vor `exact_research`. Die separate Textsuche entfällt. Ein Regressionstest reproduzierte zuerst das falsche `ReplayMismatch`; nach Korrektur folgt bereits `ExperienceSourceOperationLimit`. Der Reviewer bestätigte den Fix.

Keine verbleibenden Reviewbefunde. Zusätzlich bestanden 24 unabhängige affine, quadratische und rationale Vergleichsfälle: gleiche vollständige Status/Hypothesen wie ohne Erfahrung und identischer V4-Replay. Keine Produktdateien durch das Review verändert.

## Sichtbarer Ablauf

Erfahrung aus `(x+2)/(x+1)` mit vier Lernpaaren und einem Hold-out-Paar. Neue Aufgabe `(2*x+3)/(x+1)` mit eigenen Werten. Der Formplaner beginnt mit Potenzsuche; der Erfahrungsplaner beginnt mit Bruchfunktionen, mit einer passenden Erfahrung und Stimmen `[0,0,1]`.

Die neue vollständige Suche verbraucht 495 logische Operationseinheiten. Die Quellprüfung einschließlich Formkontext kostet separat 488 Einheiten. Der Bericht behauptet keine allgemeine Beschleunigung. Ein Widerruf führt beim neuen Snapshot wieder zur Grundreihenfolge. Der frühere exportierte Lauf bleibt nach Entfernen der Store-/Quelllaufdateien mit seinem eingebetteten damaligen Stand identisch replaybar.

## Grenzen

Ein explizit gewählter Snapshot, maximal 1 MiB, acht flache Quellen und 16 Ereignisse. Keine automatische Speicherfortschreibung, kein globaler aktueller Stand, keine Kompaktierung oder Reaktivierung widerrufener Einträge. Der Widerrufsgrund ist eine dokumentierte Entscheidung, kein automatisch geprüfter externer Beweis. Unterschiedliche Lerndaten-Fingerprints garantieren keine statistische Unabhängigkeit. Keine allgemeine Formelentdeckung und kein vollständiger M2-/M4-Abschluss. [Benutzung](../26-experience-routing-usage.md), [Teilpaket-Vertrag](../25-experience-routing-contract.md).
