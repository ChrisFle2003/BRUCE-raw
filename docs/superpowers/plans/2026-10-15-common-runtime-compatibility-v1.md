# Common Runtime Compatibility V1 — Implementierungsplan

> **Ausführungsmodus:** Subagent-driven development. Jede Aufgabe wird
> einzeln implementiert, geprüft und unabhängig reviewt.

## Ziel

Stufe 0 des freigegebenen Kompatibilitätsvertrags als read-only,
maschinenprüfbares Beschreibungsobjekt liefern. Die bestehende Session-, Gate-,
Transfer- und Management-Semantik bleibt unverändert. Es entsteht kein neuer
Scheduler und keine neue öffentliche CLI-Option.

## Geplanter Umfang

Neu:

- `include/bruce/application/common_runtime_compatibility_v1.hpp`
- `src/application/common_runtime_compatibility_v1.cpp`
- `tests/unit/common_runtime_compatibility_v1_tests.cpp`
- `tests/integration/test_common_runtime_compatibility_v1.py`
- `runs/common-runtime-compatibility-v1-2026-10-15.md`

Zu ändern:

- `CMakeLists.txt` für Core-Quelle, Unit-Test und Integrationstest;
- `scripts/verify_control_contract_matrix.py`, nur falls der neue Bericht
  ohne Änderung bestehender Ausgaben eingebunden werden kann;
- `docs/contracts/06-control-and-publication.md` und
  `docs/implementation/184-control-contract-matrix-usage.md` mit dem
  tatsächlichen Nachweis;
- generierte CLI-Dateien ausschließlich über den bestehenden Verifier, falls
  sich der dokumentierte Bestand ändert.

## Vertrag

Das Modell beschreibt genau einen bereits vorhandenen Pfad:

```text
contract = "bruce-common-runtime-compatibility-v1"
family = "session" | "gate" | "transfer" | "management"
budget_unit
checkpoint_order
error_envelope
publication_boundary
legacy_route
```

Die Darstellung ist kanonisch, bounded und read-only. Sie ist kein
Berechtigungstoken und darf keine Eingabeprüfung oder menschliche Freigabe
ersetzen.

## Task 1 — Bounded Modell und Codec

1. Unit-Tests zuerst: jede Familie, exakte Feldreihenfolge, LF-Terminierung,
   unbekannte/fehlende/duplizierte Felder, Größen- und Identifiergrenzen.
2. Modell und kanonischen JSON-Codec implementieren.
3. Keine CLI-, Datei- oder Runtime-Nebenwirkungen im Core-Modul.
4. Unit-Test und bestehende Kontrolltests ausführen.

## Task 2 — Matrixbindung

1. Die vier tatsächlichen Familienbeschreibungen aus den bestehenden
   Kontrollverträgen ableiten.
2. `control_contract_matrix` um eine rein lesende Prüfung des neuen Objekts
   ergänzen, ohne die bisherige Ausgabe oder Fehlerhülle zu ändern.
3. Prüfen, dass alte Session-/Gate-/Transfer-/Management-Aufrufe weiterhin
   bytegleich bleiben.
4. Keine automatische Auswahl oder Normalisierung der Familien einführen.

## Task 3 — Reale Workflows und Negativfälle

1. Temporäre CLI-Aufrufe für jede Familie ausführen.
2. Nullbudget, Cancellation, `INCOMPLETE`, Exportgrenze und Fehlerhülle gegen
   die bestehende Matrix prüfen.
3. Mutation, falsche Familie, falsche Einheit und falsche Veröffentlichungs-
   grenze ablehnen.
4. Den Bericht als lokale Konsistenzevidenz kennzeichnen.

## Task 4 — Dokumentation und Verifikation

1. Contract-/Matrixdokumentation aktualisieren.
2. `docs/cli/verify_examples.py` nur in vollständiger temporärer Kopie
   verwenden, falls generierte CLI-Artefakte betroffen sind.
3. Lokale Markdown-Links und relevante CTest-Ziele prüfen.
4. Bericht mit Exit-Codes, Testzahlen, Secret-Scan und bekannten
   Umgebungsgrenzen schreiben.

## Verifikation

```sh
cmake --build /tmp/bruce-v14-core-build -j2
ctest --test-dir /tmp/bruce-v14-core-build -R '^(common_runtime_compatibility_v1_contracts|control_contract_matrix|control_contract_matrix_unit)$' --output-on-failure
python3 tests/integration/test_common_runtime_compatibility_v1.py /tmp/bruce-v14-core-build/bruce
python3 scripts/check_local_doc_links.py
```

## Ausdrückliche Nichtziele

- keine gemeinsame Runtime;
- keine neue CLI-Option;
- keine Änderung vorhandener Fehlerhüllen, Budgets oder Exit-Codes;
- keine Worker-, Netzwerk-, GPU-, Plugin- oder externen-Store-Funktion;
- keine freie Sprache und kein allgemeines Beweisen;
- keine automatische Regelaktivierung oder menschliche Freigabe.

## Historische offene Entscheidungen nach Stufe 0

Die erste Frage wurde durch die nachfolgenden Stage-1-Adapter aufgelöst:
Gate, Session, Transfer und Management besitzen jeweils einen eigenen,
read-only CLI-Adapter. Die folgenden Punkte bleiben für spätere Stufen offen:

- Wird ein späterer Adapter nur lokal oder auch im Cockpit angeboten?
- Welche neue Fehlerhülle wäre für eine echte Common Runtime V2 zulässig?
