# Common Runtime Compatibility V1 — Stufe 1 Management-Adapter

**Status: IMPLEMENTIERT UND BELEGT.** Diese Stufe ergänzt die explizite,
read-only Familienbestätigung für beide bestehenden Management-Inspektionen.

## Vertrag

Die Option

```text
--compatibility-family management
```

ist ausschließlich für folgende Routen zulässig:

```text
bruce experience inspect SNAPSHOT --compatibility-family management
bruce rules-status STORE --compatibility-family management
```

Ohne die Option bleiben beide Legacy-Aufrufe unverändert. Die normale
stdout-/stderr-Ausgabe, Exit-Codes, Management-Budgets, Cancellation und
Read-only-Grenzen bleiben bytegleich.

Die Prüfung verwendet den bestehenden
`control_family_compatibility_v1("management")`-Descriptor und erfolgt vor
dem jeweiligen Datei- oder Storezugriff. Der Descriptor wird nicht ausgegeben.

## Negativfälle

- `session`, `gate` und `transfer` werden als
  `CommonRuntimeCompatibilityV1FamilyMismatch` abgewiesen.
- Unbekannte Werte werden als `UnknownCommonRuntimeCompatibilityV1Family`
  abgewiesen.
- Doppelte oder fehlende Werte behalten die Parserfehler.
- Transferoptionen bleiben unzulässige Managementoptionen.
- Eine falsche Familie mit fehlender Datei erzeugt keinen Dateifehler, weil
  die Familienprüfung zuerst erfolgt.

## Abgrenzung

Kein Descriptor-Ausgabemodus, kein neuer Controller, keine neue Budgeteinheit
und keine Mutation von Regelspeichern. Diese Management-Stufe verändert keine
Session- oder Transferpfade.

## Beleg

Der Adapter ist durch `apps/bruce_cli.cpp`, den Integrationstest
`tests/integration/test_common_runtime_compatibility_v1_stage1_management.py`,
CTest `common_runtime_compatibility_v1_stage1_management` und den Bericht
`runs/common-runtime-compatibility-v1-stage1-management-task1.md` belegt.
