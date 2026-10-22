# Common Runtime Compatibility V1 — Stufe 1 Gate-Adapter

**Status: IMPLEMENTIERT UND BELEGT.** Diese Stufe ergänzt eine explizite,
read-only Familienbestätigung für die bestehende `gate-run`-Route. Sie führt
keine gemeinsame Runtime ein und verändert den Legacy-Aufruf nicht.

## Ziel

Der Aufruf

```text
bruce gate-run RUN.bruce --compatibility-family gate
```

bestätigt explizit den bereits gewählten Gate-Pfad. Ohne die Option bleibt

```text
bruce gate-run RUN.bruce
```

unverändert. Die normale Fachausgabe, Fehlerhülle, Exit-Codes und
Veröffentlichungsgrenzen bleiben gleich.

## Begründung

`gate-run` ist die kleinste sichere Integrationsfläche: read-only, ohne Export,
mit einer klaren Kontrollfamilie und einer bestehenden einfachen JSON-
Fehlerhülle. Session, Transfer und Management bleiben in dieser Stufe
unverändert.

## Vertrag

- Erlaubter Wert: ausschließlich `gate`.
- Standard ohne Option: intern `gate`, keine Ausgabeänderung.
- Falsche bekannte Familien (`session`, `transfer`, `management`) werden mit
  `CommonRuntimeCompatibilityV1FamilyMismatch` abgewiesen.
- Unbekannte Werte werden mit dem bestehenden kontrollierten Fehlerpfad
  abgewiesen.
- Die Familienprüfung erfolgt vor dem Einlesen der Eingabedatei.
- Der Descriptor wird nur intern über die bestehende read-only API validiert;
  er wird nicht in die normale stdout-Antwort eingebettet. Die CLI exponiert
  in dieser Stufe ausschließlich `gate-run --compatibility-family gate`,
  nicht den Descriptor selbst.

## Abgrenzung

Diese Stufe führt nicht ein:

- keinen Descriptor-Ausgabemodus;
- keine neue Runtime, keinen Scheduler und keinen Adapter für andere Familien;
- keine Änderung an Gate-Budgets, Cancellation, Fehlerhüllen oder Exporten;
- keine freie Sprache, kein allgemeines Beweisen, keine GPU-/Netzwerk-,
  Plugin- oder externe-Store-Ausführung.

## Abnahmekriterien

1. Legacy- und expliziter Gate-Aufruf liefern bytegleiche stdout-Antworten.
2. Bestehende Gate-Optionen bleiben kombinierbar.
3. Nullbudget und Cancellation behalten ihre bisherigen Fehlerhüllen.
4. Falsche Familien, unbekannte Werte, doppelte Optionen und fehlende Werte
   werden kontrolliert abgewiesen.
5. Falsche Budgetfamilien bleiben unbekannte Optionen.
6. Kein falscher Familienwert liest die Eingabedatei oder erzeugt Dateien.
7. Bestehende Matrix-, Codec- und Workflowtests bleiben grün.

## Belege

- [`apps/bruce_cli.cpp`](../../../apps/bruce_cli.cpp) registriert die Option
  ausschließlich für `gate-run`, validiert den Descriptor vor dem Dateilesen
  und gibt weiterhin die bestehende Gate-Antwort aus.
- [`tests/integration/test_common_runtime_compatibility_v1_stage1_gate.py`](../../../tests/integration/test_common_runtime_compatibility_v1_stage1_gate.py)
  prüft Bytegleichheit, Hilfe, Optionskombinationen und die Dateilesegrenze.
- CTest `common_runtime_compatibility_v1_stage1_gate` und der CLI-Verifier
  prüfen den öffentlichen Aufruf.

## Offene nächste Stufe

Ein separater Descriptor-Ausgabemodus und Adapter für Transfer, Management
oder Session bleiben außerhalb dieser Stufe und benötigen eigene Verträge und
Abnahmetests. Die Familienauswahl in Stage 1 ist kein gemeinsamer Runtime-Host.
