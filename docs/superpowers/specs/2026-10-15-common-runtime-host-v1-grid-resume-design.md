# Common Runtime Host V1 — `grid-resume` Session Probe

**Status: IMPLEMENTIERT UND BELEGT.**

Implementierungsbelege: [Host-Header](../../../include/bruce/application/common_runtime_host_v1.hpp)
und [Host-Source](../../../src/application/common_runtime_host_v1.cpp),
[CLI-Routing](../../../apps/bruce_cli.cpp),
[Host-Unit-Tests](../../../tests/unit/common_runtime_host_v1_tests.cpp),
[Resume-Integrationstest](../../../tests/integration/test_common_runtime_compatibility_v1_stage2_grid_resume.py),
[CTest-Registrierung](../../../CMakeLists.txt) und der Implementierungs- und
Verifikationsnachweis (archivaler lokaler, nicht versionierter Pfad:
`runs/common-runtime-host-v1-grid-resume.md`).
Der unabhängige Review ergab SPEC PASS / QUALITY PASS ohne Befunde. Die
fokussierten Prüfungen bestanden; die Gesamtsuite war bei Übergabe noch
52/211 laufend und fehlerfrei, daher ist sie kein abgeschlossener
Gesamtsuite-Nachweis.

Diese Spezifikation beschreibt den nächsten begrenzten Stage-2-Schnitt nach den
implementierten `compare`, `grid` und `grid-batch`-Probes. Der bestehende Host
soll genau die `grid-resume`-Route delegieren, ohne Parent-Bundle-, Resume-,
Budget- oder Exportsemantik zu übernehmen.

## Ziel und Scope

Unterstützt wird genau:

```text
family       = session
entrypoint   = grid-resume
legacy_route = execute-cli
```

Öffentliche Aufrufe:

```text
bruce grid-resume --from-run PARTIAL --additional-queries N
bruce grid-resume --from-run PARTIAL --additional-queries N \
  --compatibility-family session
```

Der zweite Aufruf validiert den Host vor jedem Parent-Bundle-Lesen. Ohne die
Option bleibt der bisherige direkte Pfad unverändert. Die bestehende
`CommonRuntimeHostV1Request`- und Delegate-API wird additiv erweitert; kein
zweiter Hosttyp entsteht.

## Prüfungsreihenfolge

Der explizite Pfad muss diese Reihenfolge einhalten:

```text
options(...)
↓
CommonRuntimeHostV1Request prüfen
↓
Parent-Bundle lesen und bounded validieren
↓
additional-queries parsen
↓
Signalhandler registrieren
↓
GridResumeTask erzeugen
↓
execute_cli genau einmal delegieren
```

Vor dem Parsen von `additional-queries` findet beim Parent-Bundle ausschließlich
die bestehende größenbegrenzte Dateilesung über `read_bounded` statt. Die
inhaltliche Prüfung auf INCOMPLETE und Replay-Fähigkeit bleibt unverändert in
der Ausführung von `grid-resume`; sie wird nicht vorgezogen oder verändert.
Ein falscher, unbekannter oder mutierter Familienwert darf weder das Parent-
Bundle öffnen noch den Ausgabepfad verändern. Der Host parst keine Resume-Werte,
installiert keine Signale und besitzt kein Budget.

## Unveränderte Route-Semantik

`grid-resume` bleibt verantwortlich für:

- `--from-run` und bounded Parent-Bundle-Lesen;
- die Prüfung eines unvollständigen, replaybaren Parent-Bundles;
- `--additional-queries` mit bestehenden Grenzen;
- Session-Checkpoint-, Replay-, Cancellation- und Serializationsemantik;
- vollständige oder partielle Resume-Bundlebytes;
- Ausgabe und optionale exklusive Veröffentlichung.

Legacy- und explizite Aufrufe müssen stdout, stderr, Exit-Code, Fehlerhülle,
Sessionversionen und Bundlebytes bytegleich liefern, wenn nur die optionale
Familienbestätigung hinzukommt.

## Hostvertrag

Der Host akzeptiert danach genau `compare`, `grid`, `grid-batch` und
`grid-resume`. Für `grid-resume` gelten ausschließlich der kanonische
Session-Descriptor und `legacy_route == "execute-cli"`.

Die Delegation erfolgt genau einmal. Callback-Exceptions werden unverändert
weitergereicht. Es gibt keine neue Budgeteinheit, keinen Ergebniswrapper,
kein Descriptor-Output, keine neue Bundleversion und keinen neuen Top-Level-
Befehl.

## Abnahmetests

Unit-Tests müssen den kanonischen Resume-Entrypoint, falsche Entrypoints,
falsche Familien, mutierte Descriptorfelder, leere Delegates, genau-einmalige
Delegation und Exception-Passthrough prüfen.

Der Integrationstest muss mindestens abdecken:

- Legacy-/explizite Bytegleichheit bei erfolgreichem Resume;
- vollständige und partielle Resume-Bundles;
- bestehende Resume-Fehler und Exit-Codes;
- Replay-, Session-Budget- und Cancellationoptionen;
- falsche Familien vor fehlendem oder ungültigem Parent-Bundle;
- doppelte und fehlende Familienwerte;
- keine Hostwirkung auf `grid-batch`, `research`, `compatibility` oder `replay`.

## Nichtziele

Kein gemeinsamer Scheduler, kein V2-Statusmodell, keine Budgetaggregation,
keine Worker-, Prozess-, Netzwerk-, GPU-, Plugin-, externe-Store- oder
Cockpit-Ausführung. Stage 3 bleibt eine separate V2-Vertragsentscheidung.
