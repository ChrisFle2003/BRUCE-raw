# Common Runtime Compatibility V1 — Stufe 1 Transfer-Adapter

**Status: IMPLEMENTIERT UND BELEGT.** Diese Spezifikation beschreibt die
Implementierung einer expliziten, read-only Familienbestätigung für die
bereits vorhandenen Transfer-Routen. Sie führt weder eine gemeinsame Runtime
noch einen neuen Transfercontroller ein.

## Ziel

Die bestehende Transfer-Familie soll auf allen sieben vorhandenen
Übergaberouten ausdrücklich bestätigt werden können:

```text
bruce compatibility-plan ... --compatibility-family transfer
bruce compatibility-apply PLAN ... --compatibility-family transfer
bruce experience plan-add ... --compatibility-family transfer
bruce experience apply-add PLAN ... --compatibility-family transfer
bruce experience add ... --compatibility-family transfer
bruce experience revoke ... --compatibility-family transfer
bruce experience replace ... --compatibility-family transfer
```

Ohne die Option bleiben die bisherigen Aufrufe unverändert. Die Option ist
eine Auswahlbestätigung für den bereits ausgeführten Transferpfad; sie ist
kein Berechtigungsnachweis, kein Plan- oder Signaturersatz und kein Start
einer neuen Runtime.

## Repository-Evidenz

**IMPLEMENTIERT UND BELEGT:** Die sieben Zielrouten existieren bereits in
`apps/bruce_cli.cpp`. Sie verwenden `TransferControl` für die optionalen
`--max-transfer-checkpoints`-/`--transfer-cancellation`-Optionen und behalten
ihre jeweiligen Plan-, Import-, Revoke-, Replace- und exklusiven
Veröffentlichungsgrenzen.

**IMPLEMENTIERT UND BELEGT:** Der read-only Descriptor und seine strikte
Familienprüfung existieren in
`include/bruce/application/common_runtime_compatibility_v1.hpp` und
`src/application/common_runtime_compatibility_v1.cpp`. Die Transferbindung
ist `family = "transfer"`, mit `budget_unit =
"transfer-checkpoints-v1"`, Fehlerhülle `simple-error-json` und
Publikationsgrenze `export-preparation`.

**IMPLEMENTIERT UND BELEGT:** Der Stage-1-Transfer-Adapter ist durch
`apps/bruce_cli.cpp`, den Integrationstest
`tests/integration/test_common_runtime_compatibility_v1_stage1_transfer.py`,
CTest `common_runtime_compatibility_v1_stage1_transfer` sowie die Berichte
`runs/common-runtime-compatibility-v1-stage1-transfer-task1.md` und
`runs/common-runtime-compatibility-v1-stage1-transfer-task2.md` belegt.

## Vertrag

### Gültige Auswahl

- Auf jeder der sieben Zielrouten ist `--compatibility-family` optional.
- Der einzige gültige Wert ist exakt `transfer`.
- `session`, `gate` und `management` werden als
  `CommonRuntimeCompatibilityV1FamilyMismatch` abgewiesen.
- Unbekannte Werte werden durch den bestehenden Descriptor-Contract als
  `UnknownCommonRuntimeCompatibilityV1Family` abgewiesen.
- Doppelte Optionen behalten den Parserfehler
  `DuplicateOption: --compatibility-family`.
- Ein fehlender Optionswert bleibt ein kontrollierter
  `MissingOptionValue: --compatibility-family`-Fehler beziehungsweise die
  bestehende Usage-Hülle der jeweiligen Route, sofern der bestehende Parser
  diese Hülle verwendet.

Die sieben Routen erhalten keine gemeinsame neue Optionssyntax für ihre
fachlichen Parameter. Jede Route behält ihre vorhandenen Positionsparameter,
Pflichtoptionen und route-spezifischen Transferoptionen.

### Prüfungsreihenfolge

Nach dem bestehenden Parsen der Route und ihrer Optionen wird der
`CommonRuntimeCompatibilityV1`-Descriptor für den vom Nutzer gelieferten
Wert geladen und mit `require_control_family_compatibility_v1` geprüft. Danach
wird ausschließlich `family == "transfer"` akzeptiert.

Diese read-only Prüfung erfolgt vor jedem Datei-, Plan- oder Storezugriff der
betroffenen Route. Insbesondere darf ein falscher Familienwert nicht zuerst
eine Regeldatei, einen aktiven Regelspeicher, ein Forschungsbundle, einen
Erfahrungs-Snapshot, einen Plan oder einen Ausgabepfad lesen beziehungsweise
eine Ausgabe anlegen. Das Registrieren eines Signalhandlers oder das reine
Parsen von Optionswerten gilt nicht als Datei-/Storezugriff.

### Legacy-Kompatibilität

Ein gültiger Aufruf mit `--compatibility-family transfer` muss im jeweiligen
Erfolgs- und Fehlerpfad dieselben Eigenschaften behalten wie der Aufruf ohne
die Option:

1. stdout und stderr bleiben bytegleich, soweit keine bereits vorhandene
   Transfersteuerungsoption aktiviert ist;
2. erzeugte Plan-, Bundle- und Snapshotbytes bleiben bytegleich;
3. `TransferControl`, Checkpointzählung, Nullbudget, Signalabbruch und die
   Hülle `controlled-transfer-v1` bleiben unverändert;
4. die fachliche erneute Prüfung von Plan, Quelle, Regelinhalt, Basis und
   Ergebnis bleibt unverändert;
5. exklusive Veröffentlichung und Überschreibschutz bleiben unverändert;
6. es wird kein Descriptor in eine Erfolgsantwort eingebettet und kein neues
   Ergebnisfeld erzeugt.

Für Aufrufe, die bereits `--max-transfer-checkpoints` oder
`--transfer-cancellation` verwenden, wird die Familienbestätigung zusätzlich
validiert; sie aktiviert oder verändert diese Steuerung nicht.

### Routenmatrix

| Route | Bestehender Zweck | Eingaben vor der Fachlogik | Veröffentlichung | Kompatibilitätsprüfung |
| --- | --- | --- | --- | --- |
| `compatibility-plan` | Z-Auswahl planen | Quelle, Regeln/aktiver Regelspeicher | neuer Plan | vor Quellen-/Regellesen |
| `compatibility-apply` | Z-Auswahl ausführen | Plan, Quelle, Regeln/aktiver Regelspeicher | optionales Ergebnisbundle | vor Plan-/Quellen-/Regellesen |
| `experience plan-add` | Erfahrungsimport planen | Quelle, optionaler Basis-Snapshot | neuer Plan | vor Quellen-/Basislesen |
| `experience apply-add` | geplanten Import ausführen | Plan, Quelle, optionaler Basis-Snapshot | neuer Snapshot | vor Plan-/Quellen-/Basislesen |
| `experience add` | direkten Import ausführen | Quelle, optionaler Basis-Snapshot | neuer Snapshot | vor Quellen-/Basislesen |
| `experience revoke` | Eintrag widerrufen | Basis-Snapshot | neuer Snapshot | vor Basislesen |
| `experience replace` | Eintrag ersetzen | Basis-Snapshot, Ersatzquelle | neuer Snapshot | vor Basis-/Quellenlesen |

Die Matrix beschreibt die vorhandenen Grenzen; der Adapter darf sie nicht
vereinheitlichen oder neu berechnen.

## Fehler- und Seiteneffektgrenze

Die Familienprüfung ist ausschließlich eine Auswahlvalidierung. Bei einem
Fehler gilt:

- Exit-Code und JSON-Fehlerhülle bleiben der bestehenden CLI-Hülle folgend
  Exit 2 und stderr-JSON;
- stdout bleibt leer;
- kein neuer Plan, kein Bundle und kein Snapshot wird angelegt;
- keine Eingabedatei, kein Regelstore und kein Erfahrungssnapshot wird
  verändert;
- ein bereits existierender Ausgabepfad bleibt unverändert.

Die genaue zuerst sichtbare Fehlermeldung folgt der Reihenfolge des vorhandenen
Optionsparsers. Die neue Familienprüfung darf nur vor I/O verschieben, was für
die geforderte Vorabvalidierung nötig ist; sie darf keine fachliche Prüfung
oder bestehende Fehlerpriorität umsortieren, die erst nach gültiger
Familienauswahl beginnt.

## Abnahmekriterien

1. Für alle sieben Routen ist `transfer` optional akzeptiert.
2. Der explizite und der Legacy-Aufruf liefern für jede Route bytegleiche
   Erfolgsausgaben und identische Fehlerausgaben.
3. Repräsentative Plan-, Bundle- und Snapshotbytes sind bytegleich.
4. Bestehende Transfersteuerung bleibt mit der Familienbestätigung
   kombinierbar; Nullbudget und Cancellation behalten ihre bisherigen
   Fehlerhüllen und Publikationsgrenzen.
5. Falsche bekannte Familien, unbekannte Familien, doppelte Optionen und
   fehlende Werte werden kontrolliert abgewiesen.
6. Jede falsche Familienauswahl wird vor dem jeweiligen Datei-/Storezugriff
   abgewiesen und erzeugt keine Ausgabe.
7. Die Option wird von nicht zu den sieben Transfer-Routen gehörenden
   Befehlen nicht stillschweigend akzeptiert.
8. Die vorhandenen Transfer-, Matrix-, Gate-, Management- und CLI-Dokument-
   tests bleiben grün.

## Konkrete Testbelege

Der Implementierungstest liegt als
`tests/integration/test_common_runtime_compatibility_v1_stage1_transfer.py`
vor und wird als CTest
`common_runtime_compatibility_v1_stage1_transfer` registriert. Er muss
mindestens folgende Fälle abdecken:

- Legacy-/expliziter Bytevergleich für alle sieben Routen;
- Hilfezeilen mit `--compatibility-family transfer` für alle sieben Routen;
- ein kontrollierter Lauf mit vorhandenen Transferoptionen je Plan-/Apply-
  und direkter Experience-Kategorie;
- falsche bekannte Familien und unbekannte Werte gegen fehlende Eingaben für
  alle sieben Routen;
- doppelte Option, fehlender Wert und Verwendung einer fremden Budgetoption;
- Verwendung der Familienoption auf einer Nicht-Transfer-Route;
- Nullbudget ohne erzeugten Ausgabeplan beziehungsweise Snapshot;
- unveränderte Eingabe- und bestehende Ausgabebytes nach Ablehnung.

Die bestehenden Tests
`compatibility_transfer_workflows` und
`experience_transfer_workflows` bleiben der Nachweis für die nativen
Transferverträge. Der neue Stage-1-Test prüft nur die Adaptergrenze und
wiederholt keine vollständige fachliche Transferprüfung.

## Dokumentation und Generierung

Die Implementierung muss die öffentliche Hilfe und die betroffenen
Transferdokumente synchronisieren:

- `docs/cli/commands.md` für alle sieben syntaktischen Aufrufe;
- `docs/cli/compatibility-transfer.md` und
  `docs/cli/experience-transfer.md` für Ablauf, Option und Fehlergrenze;
- `docs/cli/transfer-control.md` für die Kombination mit der bestehenden
  Transfersteuerung;
- `docs/cli/bruce-cli-reference.md` und die zugehörige JSON-Referenz;
- `docs/contracts/06-control-and-publication.md` sowie
  `docs/implementation/184-control-contract-matrix-usage.md` für den
  implementierten Stage-1-Status.

Die generierten CLI-Artefakte
`docs/cli/source-index.md`, `docs/cli/examples.json` und
`docs/cli/validation.md` dürfen ausschließlich über
`docs/cli/verify_examples.py` in einer vollständigen temporären Kopie des
Repositories geprüft beziehungsweise aktualisiert werden. Keine direkte
Handänderung dieser drei Dateien. Die Verifikation muss zusätzlich JSON-
Parsing, lokale Markdown-Links und einen Secret-Scan umfassen.

## Abgrenzung

Diese Stufe führt nicht ein:

- keinen Descriptor-Ausgabemodus;
- keinen gemeinsamen Host, Scheduler, Worker oder Prozessvertrag;
- keine neue Budgeteinheit und keine universelle Zeit-, RAM- oder
  Operationsgrenze;
- keine Änderung an Transferplanformaten, Bundleversionen oder
  Erfahrungsformaten;
- keine neue Regelaktivierung, Signatur oder Autorisierung;
- keine freie Sprache, kein allgemeines Beweisen, keine GPU-/Netzwerk-,
  Plugin- oder externen-Store-Ausführung und keine Phase-DAG-Runtime.

## Offene nächste Stufe

Ein echter gemeinsamer Host oder ein Descriptor-Ausgabemodus benötigt einen
separaten Vertrag. Auch nach diesem Adapter bleiben die fachlichen Transfer-
und Managementgrenzen eigenständig; eine bestätigte Familie ist keine
Erlaubnis, die Budget- oder Fehlerhülle einer anderen Familie zu verwenden.
