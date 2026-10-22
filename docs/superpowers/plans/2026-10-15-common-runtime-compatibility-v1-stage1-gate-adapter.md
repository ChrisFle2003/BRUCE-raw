# Common Runtime Compatibility V1 — Stufe-1-Gate-Adapter

> Ausführung: subagent-driven development mit test-first Verifikation.

## Ziel

Eine einzige explizite Familienbestätigung für `gate-run` ergänzen, ohne die
Legacy-Ausgabe oder bestehende Kontrollsemantik zu ändern.

## Aufgaben

### Task 1 — CLI-Adapter und Tests

- Zuerst Unit-/Integrationstests für `--compatibility-family gate` schreiben.
- `apps/bruce_cli.cpp` um die Option nur für `gate-run` erweitern.
- Die bestehende Descriptor-API intern prüfen, bevor die Eingabedatei gelesen
  wird.
- Hilfezeile und CMake-Testregistrierung aktualisieren.
- Keine neue stdout-Struktur und keine Änderungen an anderen Routen.

### Task 2 — unabhängige Review-Prüfung

- Legacy-/explizite Bytegleichheit, Negativfälle und Dateigrenze prüfen.
- Build und fokussierte Tests frisch ausführen.

### Task 3 — Dokumentation und Abschlussnachweis

- Contract-/Spec-Dokumente auf den implementierten Stage-1-Status bringen.
- CLI-Generierung nur über den bestehenden Verifier in vollständiger
  temporärer Kopie prüfen, falls die Hilfe-Artefakte betroffen sind.
- Bericht mit Status, Testzahlen und Grenzen schreiben.

## Verifikation

```sh
cmake --build /tmp/bruce-v14-core-build -j2
ctest --test-dir /tmp/bruce-v14-core-build -R \
  '^(common_runtime_compatibility_v1_contracts|common_runtime_matrix_binding|common_runtime_compatibility_v1_workflows|common_runtime_compatibility_v1_stage1_gate)$' \
  --output-on-failure
python3 scripts/check_local_doc_links.py
```

## Nichtziele

- keine gemeinsame Runtime;
- kein Descriptor-Output;
- keine Session-, Transfer- oder Management-Adapter;
- keine Änderung an bestehenden Fehlerhüllen, Budgets oder Exit-Codes.
