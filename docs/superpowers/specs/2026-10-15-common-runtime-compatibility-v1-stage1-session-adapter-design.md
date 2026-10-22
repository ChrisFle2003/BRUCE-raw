# Common Runtime Compatibility V1 — Stufe 1 Session-Adapter

**Status: IMPLEMENTIERT UND BELEGT.** Diese Spezifikation beschreibt eine
optionale, read-only Familienbestätigung für die sechs bestehenden generischen
`execute_cli`-Routen. Die Umsetzung und die Verifikation sind abgeschlossen.

## Ziel

Die vorhandene Session-Kontrollfamilie soll auf den bestehenden generischen
Ausführungsrouten ausdrücklich bestätigt werden können, ohne eine gemeinsame
Runtime einzuführen. Die Bestätigung ist eine Auswahlvalidierung für den
bereits vorhandenen Ausführungspfad. Sie ist kein Berechtigungsnachweis,
kein Replay-Zwang, kein Descriptor-Output und kein Ersatz für die bestehende
Eingabeprüfung.

## Repository-Evidenz

**IMPLEMENTIERT UND BELEGT:** `execute_cli` in
`apps/bruce_cli.cpp` verarbeitet die bestehende Session-Steuerung über
`--verification`, `--max-session-checkpoints`, Replay-/Serialisierungsbudgets
und die vorhandenen Cancellation-Optionen. Die generischen Aufgaben werden
über `execute_application` beziehungsweise `execute_checked_application`
ausgeführt; optionale Bundles werden erst danach exportiert.

**IMPLEMENTIERT UND BELEGT:** Die sechs Zielrouten rufen diesen generischen
Pfad auf:

- `compare`;
- `grid`;
- `grid-batch`;
- `grid-resume`;
- `research` mit CSV-, Exact- und Grid-Varianten;
- `compatibility` mit Kandidaten- beziehungsweise Research-Run-Quelle.

**IMPLEMENTIERT UND BELEGT:** Der read-only Descriptor in
`include/bruce/application/common_runtime_compatibility_v1.hpp` und
`src/application/common_runtime_compatibility_v1.cpp` beschreibt die Session-
Familie mit:

```text
family               = session
budget_unit          = session-checkpoints-v1
checkpoint_order     = path-dependent
error_envelope       = application-execution-failure-v1-v3
publication_boundary = final-export-outside
legacy_route         = execute-cli
```

Der Descriptor wird intern zur Validierung verwendet. Er wird nicht an die
CLI-Antwort angehängt.

## Vertrag

### Gültige Auswahl

Die Option ist auf jeder der sechs Zielrouten optional:

```text
--compatibility-family session
```

Der einzige gültige Wert ist exakt `session`. `gate`, `transfer` und
`management` werden als `CommonRuntimeCompatibilityV1FamilyMismatch`
abgewiesen. Unbekannte Werte werden als
`UnknownCommonRuntimeCompatibilityV1Family` abgewiesen. Doppelte oder fehlende
Werte behalten die bestehenden Parserfehler.

Die Option darf mit den bereits vorhandenen Sessionoptionen kombiniert werden.
Insbesondere ist `--verification replay` **nicht erforderlich**. Ein Aufruf
mit `--compatibility-family session` und ohne `--verification` läuft im
bisherigen Standardmodus `none`; ein Aufruf mit `--verification replay` behält
seine bisherige Replay-Semantik.

### Prüfungsreihenfolge

Auf jeder Zielroute wird die Familienprüfung unmittelbar nach dem bestehenden
`options(...)`-Aufruf ausgeführt. Sie muss vor Datei-, Bundle- oder Storezugriff
liegen. Dazu gehören insbesondere:

- `read_bounded`, `read_samples` und `read_exact_samples` in `research`;
- `read_bounded` des Eingabebundles in `grid-resume`;
- Regel-, Kandidaten- und Research-Run-Lesen in `compatibility`;
- jeder weitere Quellen- oder Bundlezugriff, der vor dem Aufruf von
  `execute_cli` stattfindet.

Das reine Parsen der Optionen, das Erzeugen eines Aufgabenobjekts, die
Validierung route-spezifischer Enumwerte und das Registrieren eines
Signalhandlers gelten nicht als Datei-/Bundlezugriff. Die Familienprüfung
darf keine bestehende fachliche Fehlerpriorität nach einem gültigen
Familienwert neu ordnen.

### Legacy-Kompatibilität

Ohne `--compatibility-family` bleiben alle sechs Aufrufe unverändert. Mit dem
expliziten Wert `session` müssen Erfolgs- und Fehlerpfade dieselben
Eigenschaften behalten:

1. stdout, stderr und Exit-Code bleiben bytegleich, soweit der Aufruf abseits
   der neuen Bestätigungsoption identisch ist;
2. `execute_cli` erhält dieselben Aufgaben, Budgets, Cancellation-Callbacks
   und Replayentscheidungen;
3. erzeugte Run-/Bundlebytes und Session-Versionen bleiben unverändert;
4. `session-checkpoints-v1`, die bestehende pfadabhängige Checkpointreihenfolge
   und die Hülle `application-execution-failure-v1-v3` bleiben unverändert;
5. `INCOMPLETE`, Replayfehler, Nullbudgets und Exportfehler behalten ihre
   bisherigen Fehlergrenzen und Publikationsregeln;
6. kein Descriptor, kein neues JSON-Feld und kein neuer Reportwrapper wird
   ausgegeben.

Die Familienbestätigung aktiviert kein Budget und ersetzt keine vorhandene
`--verification`-, Cancellation- oder Exportoption.

## Routenmatrix

| Route | Bestehender Zweck | Varianten / Eingaben | Prüfung vor | Ausführung |
| --- | --- | --- | --- | --- |
| `compare` | zwei rationale Funktionen vergleichen | Koeffizientenlisten und Domänenoptionen | Taskstart und Export | `FractionComparisonTask` |
| `grid` | einen Grid-Pfad auswerten | exact, modular oder metric | Taskstart und Export | `GridTask` |
| `grid-batch` | mehrere Grid-Pfade auswerten | exact, modular oder metric, Cache und Querybudget | Pfadaufbereitung und Export | `GridBatchTask` |
| `grid-resume` | unvollständigen Grid-Lauf fortsetzen | Parent-Bundle und zusätzliche Queries | Parent-Bundle-Lesen | `GridResumeTask` |
| `research` | Forschungsrun erzeugen | CSV, Exact und Grid-Research; Strategien bleiben routeintern | Trainings-/Holdout-/Grid- und Experience-Lesen | `Task`, `ExactResearchTask` oder `GridResearchTask` |
| `compatibility` | Kandidaten anhand von Regeln auswählen | Kandidaten-CSV oder Research-Run mit `--at` | Regeln-, Kandidaten- oder Run-Lesen | `CompatibilityTask` |

Die Matrix vereinheitlicht keine Aufgaben und keine Datenformate. Die
route-spezifischen Optionslisten, Positionsparameter, Bundleversionen,
Domainprüfungen und Exportpfade bleiben bestehen.

## Fehler- und Seiteneffektgrenze

Bei einer ungültigen Familienauswahl gilt:

- Exit-Code 2 und die bestehende JSON-Fehlerhülle auf stderr;
- stdout bleibt leer;
- der Familienfehler erscheint vor einem Fehler für eine fehlende Eingabe,
  sofern beide Bedingungen gleichzeitig vorliegen;
- keine Eingabedatei, kein Bundle und kein Ausgabepfad wird gelesen oder
  verändert;
- kein Task wird ausgeführt und kein Ergebnis wird veröffentlicht.

Der genaue Parserfehler für eine fehlende oder doppelte Option bleibt der
bestehenden `options(...)`-Implementierung überlassen. Die neue Prüfung darf
nur die geforderte Vorabgrenze ergänzen.

## Abgrenzung

Nicht Bestandteil dieser Stufe sind:

- `replay`, `inspect-run` und `assess-run`;
- `gate-run`, `compatibility-plan` und `compatibility-apply`;
- Transfer- und Managementrouten;
- `task-spec`-Routen;
- `language`- und `dialogue`-Execute-Routen;
- ein Descriptor-Ausgabemodus;
- ein gemeinsamer Host, Scheduler, Worker- oder Prozessvertrag;
- neue Zeit-, RAM-, Operations- oder Signallatenzbudgets;
- eine Änderung an Replay-, Bundle-, Session- oder Fehlerformaten;
- freie Sprache, allgemeines Beweisen, GPU-/Netzwerk-, Plugin- oder
  externe-Store-Ausführung und eine Phase-DAG-Runtime.

## Konkrete TDD-Abnahmetests

Der Implementierungstest wird als
`tests/integration/test_common_runtime_compatibility_v1_stage1_session.py`
angelegt und als CTest
`common_runtime_compatibility_v1_stage1_session` registriert. Er muss
mindestens folgende Fälle abdecken:

- Legacy-/expliziter Bytevergleich für `compare`, `grid`, `grid-batch`,
  `grid-resume`, `research` und `compatibility`;
- CSV-, Exact- und Grid-Research mit expliziter Sessionfamilie;
- Kandidaten- und Research-Run-Variante von `compatibility`;
- explizite Sessionfamilie ohne `--verification replay`;
- explizite Sessionfamilie zusammen mit `--verification replay`,
  `--max-session-checkpoints` und einer bestehenden Cancellationoption;
- falsche bekannte Familien und unbekannte Werte vor fehlenden Dateien oder
  Bundles;
- doppelte und fehlende Familienwerte sowie eine fremde Transferoption;
- keine Annahme der Option auf `replay`, `inspect-run`, `assess-run`,
  `gate-run`, Transfer-, Management-, `task-spec`-, `language`- oder
  `dialogue`-Routen;
- kein Descriptor- oder Wrapperfeld in einer erfolgreichen Sessionantwort;
- keine erzeugte oder veränderte Ausgabedatei nach einer Ablehnung.

## Dokumentation und Generierung

Nach der Implementierung müssen mindestens folgende Dokumente synchronisiert
werden:

- `docs/cli/commands.md` für die sechs Syntaxformen und ihre Varianten;
- `docs/cli/bruce-cli-reference.md` und
  `docs/cli/bruce-cli-reference.json`;
- `docs/cli/results-and-control.md` für die Bedeutung der optionalen
  Familienbestätigung und die fehlende Replaypflicht;
- `docs/contracts/06-control-and-publication.md` für die Session-Route und
  Stage-1-Statusmatrix;
- `docs/implementation/184-control-contract-matrix-usage.md` für die
  Validierung-vor-I/O-Regel.

Die generierten Dateien `docs/cli/source-index.md`,
`docs/cli/examples.json` und `docs/cli/validation.md` dürfen ausschließlich
über `docs/cli/verify_examples.py` in einer vollständigen temporären Kopie
geprüft beziehungsweise aktualisiert werden. Zusätzlich sind JSON-Parsing,
lokale Markdown-Links und ein Secret-Scan auszuführen.

## Abnahmekriterien

1. Die Option wird auf genau den sechs generischen `execute_cli`-Routen
   akzeptiert.
2. Nur der Wert `session` wird akzeptiert.
3. Die Prüfung erfolgt direkt nach `options(...)` und vor jedem Datei-/Bundle-
   oder Storezugriff der Route.
4. Ein gültiger Wert erfordert kein `--verification replay`.
5. Legacy-Ausgaben, Fehler, Exit-Codes, Bundles, Sessionversionen,
   Checkpointreihenfolge und Exportgrenze bleiben unverändert.
6. Falsche, unbekannte, doppelte und fehlende Familienwerte werden kontrolliert
   abgewiesen.
7. Nichtzielrouten akzeptieren die Option nicht stillschweigend.
8. Bestehende Session-, Matrix-, Transfer-, Management- und CLI-Tests bleiben
   grün.

## Statusgrenze

Der Adapter ist **IMPLEMENTIERT UND BELEGT** durch
`apps/bruce_cli.cpp`, den Integrationstest
`tests/integration/test_common_runtime_compatibility_v1_stage1_session.py`,
CTest `common_runtime_compatibility_v1_stage1_session` sowie die Berichte
`runs/common-runtime-compatibility-v1-stage1-session-task1.md` und
`runs/common-runtime-compatibility-v1-stage1-session-task2.md`. Die Nachweise
belegen nur die sechs CLI-Grenzen; sie belegen keine gemeinsame Runtime und
keine der in der Abgrenzung genannten Zukunftsfunktionen.
