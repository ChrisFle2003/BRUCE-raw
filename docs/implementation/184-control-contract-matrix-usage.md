# Core 0.102: Steuerungsvertragsmatrix und Common Runtime Compatibility V1

**IMPLEMENTIERT UND BELEGT:** Dieses Paket führt keine neue Controller-Runtime
ein. Es macht den bereits vorhandenen Unterschied zwischen Sitzung, Gate,
Transfer und read-only Management-Inspektion ausführbar nachweisbar und ordnet
ihn einem stabilen, kanonischen Kompatibilitätsvertrag zu.

## Gelieferter Umfang

- [ADR-015](../adr/015-control-contract-compatibility.md) erhält die
  Entscheidung, die bestehenden Verträge zunächst kompatibel zu bewahren
  statt sie stillschweigend zu normalisieren.
- Die [Steuerungsvertragsmatrix](../contracts/06-control-and-publication.md#kompatibilitätsmatrix-der-steuerungswege)
  benennt Budgeteinheit, Priorität, Fehlerhülle, Veröffentlichungsgrenze und
  CTest-Nachweis pro Familie. Sie enthält jetzt auch die Management-Familie.
- `CommonRuntimeCompatibilityV1` in
  [`common_runtime_compatibility_v1.hpp`](../../include/bruce/application/common_runtime_compatibility_v1.hpp)
  und die zugehörige Implementierung liefern für jede Familie einen bounded,
  strikt kanonischen JSON-Descriptor. Die API ist read-only und wählt keinen
  Ausführungspfad.
- `scripts/verify_control_contract_matrix.py` führt temporäre echte
  CLI-Aufrufe aus. Es prüft die verschiedenen Fehlerhüllen bei einem
  Nullbudget, fehlende Ziel-/Tempdateien nach einem Abbruch und einen
  replaybaren nativen `INCOMPLETE`-Befund.
- CTest registriert diesen Prüfer als `control_contract_matrix` und seinen
  Fehlermaskierungs-Negativtest als `control_contract_matrix_unit`.

Stage 1 ergänzt explizite read-only Familienbestätigungen für die sechs
generischen Session-Routen (`compare`, `grid`, `grid-batch`, `grid-resume`,
`research` und `compatibility`), `gate-run`, die beiden
Management-Inspektionsrouten und alle sieben Transfer-Routen
(`compatibility-plan`, `compatibility-apply`, `experience plan-add`,
`experience apply-add`, `experience add`, `experience revoke` und
`experience replace`). Die Transfer-Routen akzeptieren ausschließlich
`--compatibility-family transfer`; die Prüfung erfolgt vor ihrem jeweiligen
Datei-/Plan-/Storezugriff. Es wurden keine neuen C++-Controller,
Bundleversionen, Ergebnisfelder oder mathematischen, Gate-, Management- und
Replaysemantiken eingeführt.
Auf den sechs generischen Session-Routen wird ausschließlich der Wert
`--compatibility-family session` akzeptiert. Die Prüfung erfolgt direkt nach
dem Optionsparsen und vor dem jeweiligen Datei-, Bundle- oder Storezugriff;
`--verification replay` bleibt optional. CSV-, Exact- und Grid-Research sowie
beide Quellenformen von `compatibility` teilen denselben read-only Vertrag.
Die Bestätigung erzeugt keinen Descriptor und ändert keine Session-, Fehler-,
Export- oder Bundlesemantik. `compatibility-plan` und `compatibility-apply`
bleiben Transfer-Routen.

**IMPLEMENTIERT UND BELEGT:** Die anschließenden Stage-2-Probes ergänzen einen
bounded In-Process-Host für genau
`compare --compatibility-family session` und
`grid --compatibility-family session` sowie
`grid-batch --compatibility-family session` und
`grid-resume --compatibility-family session`. Der Host prüft den kanonischen
Session-Descriptor und delegiert den bestehenden `execute_cli`-Pfad jeweils
einmalig. Er führt keine eigene Session, kein Budget, keinen Path-/Cache-/Query-
Pfad, keinen Output- oder Exportpfad und keine neue öffentliche Route ein.
Legacy- und explizite Compare-/Grid-/Grid-Batch-/Grid-Resume-Aufrufe sind bei Erfolg,
vollständigen und partiellen Bundle-Exports, Cache-/Query-Fehlern und
bestehenden Ausnahmen bytegleich. Die Vertrags- und Integrationsnachweise stehen in den
archivalen lokalen, nicht versionierten Pfaden
`runs/common-runtime-host-v1-compare.md` und
`runs/common-runtime-host-v1-grid.md`; die
Compare-Spezifikation und der Plan liegen unter
[`docs/superpowers/specs/2026-10-15-common-runtime-host-v1-compare-design.md`](../superpowers/specs/2026-10-15-common-runtime-host-v1-compare-design.md)
und
[`docs/superpowers/plans/2026-10-15-common-runtime-host-v1-compare.md`](../superpowers/plans/2026-10-15-common-runtime-host-v1-compare.md).
Die Grid-Spezifikation und der Plan liegen unter
[`docs/superpowers/specs/2026-10-15-common-runtime-host-v1-grid-design.md`](../superpowers/specs/2026-10-15-common-runtime-host-v1-grid-design.md)
und
[`docs/superpowers/plans/2026-10-15-common-runtime-host-v1-grid.md`](../superpowers/plans/2026-10-15-common-runtime-host-v1-grid.md).
Die Grid-Batch-Spezifikation und der Plan liegen unter
[`docs/superpowers/specs/2026-10-15-common-runtime-host-v1-grid-batch-design.md`](../superpowers/specs/2026-10-15-common-runtime-host-v1-grid-batch-design.md)
und
[`docs/superpowers/plans/2026-10-15-common-runtime-host-v1-grid-batch.md`](../superpowers/plans/2026-10-15-common-runtime-host-v1-grid-batch.md).
Der reproduzierbare Nachweis steht im archivalen lokalen, nicht versionierten Pfad
`runs/common-runtime-host-v1-grid-batch.md`;
der damalige Grid-Batch-Integrationstest umfasst sechs Python-Testfälle und die
damalige Stage-2-/Stage-1-Regression bestand mit 15/15 Tests.

Die vierte Probe `grid-resume` validiert den Host unmittelbar nach dem
Optionsparsen und vor `read_bounded`. Der bestehende Delegate liest den Parent,
parst `additional-queries`, installiert Signale und führt `GridResumeTask` über
`execute_cli` genau einmal aus. INCOMPLETE-/Replay-Prüfung, Resume-Copy-,
Session-Budget-, Cancellation- und Exportsemantik verbleiben im bestehenden
Pfad. Der Host erhält keine Parent-, Query-, Budget- oder Bundle-Requestfelder.
Der Nachweis steht im archivalen lokalen, nicht versionierten Pfad
`runs/common-runtime-host-v1-grid-resume.md`, mit
[Design](../superpowers/specs/2026-10-15-common-runtime-host-v1-grid-resume-design.md),
[Plan](../superpowers/plans/2026-10-15-common-runtime-host-v1-grid-resume.md) und
[Integrationstest](../../tests/integration/test_common_runtime_compatibility_v1_stage2_grid_resume.py).
Für Grid-Resume bestehen acht Python-Testfälle und die fokussierte Regression
mit 19/19 CTests, einschließlich `grid_resume_workflows`, `resume_copy_control`
und `resume_copy_workflows`.
Weitere Familien oder eine allgemeine Runtime sind damit nicht eingeführt.

Die Matrix behauptet weder eine universelle Zeit- oder Speichergrenze noch
eine gemeinsame Budgeteinheit. Der Descriptor fasst die Unterschiede nur
beschreibend zusammen.

Im aktuellen Core-0.102-Quellstand verwenden auch `experience revoke` und
`experience replace` bei Angabe mindestens einer Transferoption den
bestehenden Transfercontroller. Ohne diese Optionen bleiben sie direkte
native Verwaltungsaufrufe. Diese Zuordnung ist Bestand; die Matrix führt
keinen zusätzlichen Controller für sie ein.

## Lokale Prüfung

Der reproduzierbare Prüfpfad für diesen Arbeitsstand verwendet den direkten
nativen Build in `/tmp/bruce-v14-core-build`:

```bash
cmake --build /tmp/bruce-v14-core-build -j2
ctest --test-dir /tmp/bruce-v14-core-build -R \
  '^(common_runtime_host_v1_contracts|common_runtime_compatibility_v1_stage2_compare|common_runtime_compatibility_v1_stage2_grid|common_runtime_compatibility_v1_stage2_grid_batch|common_runtime_compatibility_v1_stage2_grid_resume|common_runtime_compatibility_v1_contracts|common_runtime_matrix_binding|common_runtime_compatibility_v1_workflows|common_runtime_compatibility_v1_stage1_gate|common_runtime_compatibility_v1_stage1_management|common_runtime_compatibility_v1_stage1_transfer|common_runtime_compatibility_v1_stage1_session|execution_session_states|execution_session_workflows|control_contract_matrix|control_contract_matrix_unit|grid_partial_workflows|grid_cache_contracts|grid_cache_workflows|grid_resume_workflows|resume_copy_control|resume_copy_workflows)$' \
  --output-on-failure
python3 tests/integration/test_common_runtime_compatibility_v1.py \
  /tmp/bruce-v14-core-build/bruce \
  /tmp/bruce-v14-core-build/bruce_common_runtime_matrix_binding_tests
python3 scripts/check_local_doc_links.py
```

Der bestehende Prüfer kann separat laufen:

```bash
ctest --test-dir /tmp/bruce-v14-core-build -R '^control_contract_matrix$' \
  --output-on-failure
```

## Verifikationsbefund für Common Runtime Compatibility V1

Am 15. Oktober 2026 bestand der direkte Build mit Exit 0. Der Stage-0-
Fokuslauf bestand mit **5/5** Tests: Codec-Vertrag, Matrixbindung, reale
Common-Runtime-Workflows sowie die bestehende Vertragsmatrix und deren
Negativtest. Der Stage-1-Fokuslauf bestand mit **6/6** Tests und prüfte
Legacy-/explizite Bytegleichheit, Hilfe, Optionskombinationen, Vorabvalidierung
und falsche Optionen. Der direkte Workflow-Prüfer bestand mit **30
Python-Assertions**;
der Matrix-Binding-Test bestätigte vier Familien und wies sechs absichtlich
mutierte Deskriptoren zurück. Der lokale Markdown-Linkprüfer bestätigte
**434 Dokumente, 2480 Links und 0 gebrochene Links**.

Die Stage-1-Aufrufe und die beiden Stage-2-Compare-/Grid-Hosts sind in der öffentlichen Hilfe und im CLI-Handbuch
dokumentiert. Die normalen Gate-, Management- und Transfer-Antworten enthalten
weiterhin keinen Descriptor; die Optionen validieren die interne read-only
Beschreibung vor dem jeweiligen Datei-/Plan-/Storezugriff. Ein gültiger
Transferwert ändert weder Transfercontroller, Fehlerhülle, Plan-/Bundle- oder
Snapshotbytes noch exklusive Veröffentlichung.
Der vollständige Task-2-Bericht mit Build, fokussierten Tests, CLI-Verifier,
Secret-Scan und Umgebungsgrenzen steht im archivalen lokalen, nicht versionierten Pfad
`runs/common-runtime-compatibility-v1-2026-10-15.md`.

Die Prüfung ist lokal. Historische oder Remote-Nachweise wurden nicht als
Ersatz verwendet.

## Bewusst offen

`experience inspect` und `rules-status` besitzen jetzt einen eigenen
read-only Management-Inspektionscontroller, wenn ihre optionalen
Management-Optionen angegeben werden. Zusätzlich akzeptieren beide Routen
die explizite Bestätigung `--compatibility-family management`; diese validiert
den bestehenden Descriptor vor dem Datei-/Storezugriff, aktiviert aber keinen
Controller und verändert die Legacy-Ausgabe nicht. Ohne diese Optionen bleiben
die Legacy-Aufrufe unverändert. Die Matrix und der Descriptor ändern deshalb
keine bestehenden Verwaltungssemantiken.
Die sieben Transfer-Routen besitzen parallel die Familienbestätigung
`--compatibility-family transfer`; diese ist eine Auswahlvalidierung vor I/O
und keine neue Laufzeit oder Berechtigung.

## V1-Descriptor: aktuelles kanonisches Binding

| Familie | Budgeteinheit | Reihenfolge | Fehlerhülle | Publikationsgrenze | Legacy-Route |
| --- | --- | --- | --- | --- | --- |
| Session | `session-checkpoints-v1` | `path-dependent` | `application-execution-failure-v1-v3` | `final-export-outside` | `execute-cli` |
| Gate | `session-checkpoints-v1` | `signal-before-consumption` | `simple-error-json` | `no-export` | `gate-run` |
| Transfer | `transfer-checkpoints-v1` | `signal-budget-consume` | `simple-error-json` | `export-preparation` | `transfer-cli` |
| Management | `management-checkpoints-v1` | `signal-budget-consume` | `simple-error-json` | `read-only` | `experience-inspect+rules-status` |

Der Session-Wert `application-execution-failure-v1-v3` ist eine
Kompatibilitätszusammenfassung. Die konkrete Legacy-Ausgabe bleibt je nach
aktivem Sitzungs- oder Serialisierungsbudget `-v1`, `-v2` oder `-v3`.
