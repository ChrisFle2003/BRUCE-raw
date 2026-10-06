# NanoCellExecutorV1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Einen eigenständigen, deterministischen C++20-Referenzexecutor für drei fest verdrahtete NanoCell-Byte-Techniken mit kanonischem Replaybundle liefern.

**Architecture:** Der neue Baustein lebt ausschließlich unter `bruce/nanocell` und besitzt einen eigenen Request-, AnswerPoint-, Trace- und Bundlevertrag. Er wird nicht in `ApplicationTask`, `ExecutionSession`, CLI, Gates, Assessment oder Sprache eingehängt. Die drei Referenztechniken werden im Bibliothekscode fest definiert; der Executor lehnt jede abweichende Technikdefinition ab.

**Tech Stack:** C++20, STL, bestehendes OpenSSL-SHA-256 über `bruce::sha256`, CMake, Ninja und CTest.

**Spec:** `docs/superpowers/specs/2026-09-27-nanocell-executor-v1-design.md`

## Global Constraints

- Vor jedem Produktionsschritt muss die in Task 1 beschriebene Trace-Korrektur fachlich bestätigt und in der Spezifikation übernommen sein; die aktuelle Grenze `1..17` kann Start, 16 Operatorergebnisse und Terminaltrace nicht zugleich darstellen.
- Der korrigierte V1-Wert lautet `max_trace_entries: UInt64 2..18`: Start und Terminal sind immer Traceeinträge; vor einem Opcode sind dessen Ergebnis- und der reservierte Terminaleintrag budgetierbar.
- `core_bytes` bleibt ein `std::array<std::uint8_t, 8>`; `active_index` ist `0..7`, und nur `core_bytes[active_index]` wird als Byteprogrammzustand verwendet.
- Request-, Answer- und Technikversionen bleiben exakt `nanocell-request-v1`, `nanocell-answer-v1` und `nanocell-technique-v1`; `core_layout_version` bleibt exakt `nanoneuron-core-v1`.
- Kontext- und Technik-IDs akzeptieren nur 1..64 ASCII-Zeichen aus `[A-Za-z0-9_.-]`; Kontextbytes sind 0..4096 Bytes, Programme 1..16 Operatoren und `max_operations` ist 0..16.
- Der V1-Opcodeumfang ist exakt `AND`, `OR`, `XOR`, `NOT`, `ROTL`, `SHL`, `SHR`. Logische Shifts und Rotationen verwenden nur Weiten `0..7`; unbekannte Opcodes und nichtkanonische unbenutzte Operanden sind Fehler.
- Nur `masked-match-v1`, `rotate-match-v1` und `context-conflict-v1` sind zulässige Techniken. Es gibt keine Plugin-, Datei-, Netzwerk-, Datenbank-, Experience-, GPU- oder Parallelpfade.
- Ein Budgetstopp liefert `INCOMPLETE`, keinen Match. Ein wahrer `NanoCellControlV1`-Callback wird nur vor einem Opcode geprüft und wirft `Error("NanoCellCancelled")` ohne AnswerPoint oder Bundle.
- Das Bundle heißt exakt `BRUCE_NANOCELL_RUN_V1`; es ist length-prefix-kodiert, mit SHA-256 abgesichert, kanonisch und schreibt selbst keine Dateien.
- `core_digest` und `technique_digest` verwenden jeweils einen separaten privaten, domänengetrennten binären Feldstrom. Jedes Feld ist mit seiner Byteanzahl als vorzeichenloser 8-Byte-Big-Endian-Wert und danach seinen Rohbytes kodiert; SHA-256 deckt den gesamten jeweiligen Strom ab. Für den Core ist die feste Reihenfolge `nanocell-core-digest-v1`, `core_layout_version`, `active_index` als ein Rohbyte, acht rohe `core_bytes`. Für die Technik ist sie `nanocell-technique-digest-v1`, `technique_id`, `technique_version`, `required_context_digest` als 64 ASCII-Hexbytes, `program` als rohe Opcode-/Operand-Bytepaare, `match_mask` als ein Rohbyte, `match_value` als ein Rohbyte. Die Enumwerte der Opcodes sind `AND=0`, `OR=1`, `XOR=2`, `NOT=3`, `ROTL=4`, `SHL=5`, `SHR=6`. Auch Einbytewerte sind Längenfelder. ASCII-Dezimallängen mit `:` gelten nur für den Request-/Answer-/Trace-/Bundlecodec `BRUCE_NANOCELL_RUN_V1`, nicht für diese Provenienz-Digests.
- Bestehende `ApplicationTask`-Varianten, `ExecutionSession`, Application-Bundles, CLI, Renderer, Gates und Sprachschnittstelle bleiben unverändert.
- Das Arbeitsverzeichnis ist kein nutzbares Git-Repository. Keine Commits oder Branch-Operationen versuchen; für jeden Lauf eine vollständige, frische Kopie unter `/tmp` konfigurieren und die ausgeführten Befehle samt Ergebnis im Arbeitsbericht festhalten.
- Keine Paketinstallation und keine externen Netz- oder Systemschreibvorgänge.

## Review Focus

- **Trace-Kapazität am Rand:** Ein Programm mit 16 Opcodes braucht Start, 16 Ergebnis- und einen Terminaleintrag. Die drei bewusst einzigen V1-Fabriken haben jedoch höchstens zwei Opcodes; Task 3 prüft deren Budgetgrenzen und die Formel `1 + n + 1`, ohne einen vierten oder konfigurierbaren Technikpfad einzuführen. Ein echter 16-Opcode-Ausführungstest ist ein V2-Freigabekriterium zusammen mit einer ausdrücklich neu freigegebenen Referenztechnik.
- **Byteoperationen ohne undefiniertes Verhalten:** `SHL`, `SHR` und `ROTL` bei Weite 0 und 7 müssen für alle 256 Eingabewerte korrekt bleiben; Task 2 prüft sie gegen unabhängige Referenzberechnungen.
- **Kontext vor Bytegleichheit:** Ein korrekt aussehendes Ergebnisbyte darf bei anderem gebundenem Kontext nie `MATCHED` werden; Task 3 prüft `context-conflict-v1` ohne Operatorausführung.
- **Manipuliertes, neu gehashtes Bundle:** Eine neu berechnete äußere Prüfsumme darf eine veränderte Anfrage, Antwort oder Trace nicht legitimieren; Task 4 manipuliert je ein Feld und erneuert den äußeren Digest.
- **Provenienz trotz gleichem Ergebnisbyte:** Technik-ID, Technikdigest, Kontext-ID und Kontextdigest müssen Teil des AnswerPoints bleiben; Task 3 vergleicht zwei Läufe mit gleichem `final_state` und verschiedener Herkunft.

---

## File Structure

- `docs/superpowers/specs/2026-09-27-nanocell-executor-v1-design.md` – verbindlicher V1-Vertrag; Task 1 korrigiert ausschließlich den widersprüchlichen Tracebereich.
- `include/bruce/nanocell/nanocell.hpp` – öffentliche, kleine Daten- und Funktionsschnittstelle für Byteoperatoren, fest verdrahtete Techniken, Ausführung und Replay.
- `src/nanocell/nanocell.cpp` – private Validierung, kanonische Kodierung, Referenzausführung und Replay-Verifikation.
- `tests/unit/nanocell_tests.cpp` – eigenständiger C++-Contract-Test für Operatoren, Ausführung, Budgets, Abbruch, Provenienz und Replay-Manipulationen.
- `CMakeLists.txt` – Eintrag der neuen Bibliotheksquelle und des einzelnen Testziels `nanocell_contracts`.

### Task 1: Tracevertrag fachlich schließen

**Files:**
- Modify: `docs/superpowers/specs/2026-09-27-nanocell-executor-v1-design.md: NanoCellRequestV1 und Ausführung und AnswerPoint`
- Test: fachliche Spezifikationsprüfung durch den Nutzer vor jeder C++-Änderung

**Interfaces:**
- Consumes: den freigegebenen NanoCell-V1-Vertrag.
- Produces: die eindeutige Budgetregel `2..18`, auf die die späteren Tests und der Executor bauen.

- [ ] **Step 1: Dokumentiere die konkrete Widerspruchsprobe in der Spezifikation**

  Ergänze direkt neben `max_trace_entries`, dass `1..17` nicht gleichzeitig Start, 16 Operatorergebnisse und Terminal abbilden kann.

- [ ] **Step 2: Ersetze ausschließlich den Tracebereich und die Vorabprüfung**

  Setze `max_trace_entries: UInt64 2..18` und ergänze: Vor einem Opcode müssen ein Ergebnis- und der abschließende Terminaleintrag noch Platz haben. Bei fehlendem Platz wird ohne weiteren Opcode `INCOMPLETE` erzeugt.

- [ ] **Step 3: Prüfe die Änderung fachlich gegen die Grenzfälle**

  Erwartet: 16 Opcodes mit 18 Einträgen sind vollständig; bei 17 wird vor dem letzten nicht mehr ausführbaren Opcode `INCOMPLETE`; `CONTEXT_REJECTED` benötigt mindestens Start und Terminal.

- [ ] **Step 4: Hole die explizite Freigabe des korrigierten Vertrags ein**

  Erwartet: keine C++-Datei und keine Build-Konfiguration wird verändert, bevor die Korrektur bestätigt ist.

### Task 2: Byteoperatoren, Datentypen und CMake-Testziel

**Files:**
- Create: `include/bruce/nanocell/nanocell.hpp`
- Create: `src/nanocell/nanocell.cpp`
- Create: `tests/unit/nanocell_tests.cpp`
- Modify: `CMakeLists.txt: bruce_core source list und BUILD_TESTING test targets`

**Interfaces:**
- Consumes: den in Task 1 bestätigten Tracevertrag sowie `bruce::Error` aus `bruce/core/types.hpp` und `bruce::sha256` aus `bruce/evidence/bundle.hpp`.
- Produces:

  ```cpp
  namespace bruce {
  enum class NanoCellOpCodeV1 { bit_and, bit_or, bit_xor, bit_not, rotl, shl, shr };
  struct NanoCellByteOpV1 { NanoCellOpCodeV1 code; std::uint8_t operand{}; };
  std::uint8_t and8(std::uint8_t a, std::uint8_t b);
  std::uint8_t or8(std::uint8_t a, std::uint8_t b);
  std::uint8_t xor8(std::uint8_t a, std::uint8_t b);
  std::uint8_t apply_nanocell_byte_op_v1(std::uint8_t state, const NanoCellByteOpV1& op);

  struct NanoNeuronCoreV1 { std::array<std::uint8_t, 8> core_bytes{}; std::uint8_t active_index{}; std::string core_layout_version{"nanoneuron-core-v1"}; };
  struct NanoCellContextV1 { std::string context_id; std::string context_bytes; std::string context_digest; };
  struct NanoCellTechniqueDefinitionV1 { std::string technique_id; std::string technique_version{"nanocell-technique-v1"}; std::string required_context_digest; std::vector<NanoCellByteOpV1> program; std::uint8_t match_mask{}; std::uint8_t match_value{}; };
  enum class NanoCellReferenceTechniqueV1 { masked_match, rotate_match, context_conflict };
  NanoCellTechniqueDefinitionV1 nanocell_reference_technique_v1(NanoCellReferenceTechniqueV1 technique);
  }
  ```

- [ ] **Step 1: Schreibe den fehlschlagenden Operatorvertrag in `tests/unit/nanocell_tests.cpp`**

  Baue unabhängige Referenzfunktionen im Test ein und prüfe `and8`, `or8` und `xor8` für alle 65.536 geordneten Bytepaare. Prüfe `NOT` sowie jede Shift- und Rotationsweite 0..7 für alle 256 Bytes; nenne die Blöcke `binary_truth_tables`, `unary_truth_tables` und `shift_rotation_truth_tables`.

- [ ] **Step 2: Ergänze den minimalen CMake-Testeintrag und führe den Test rot aus**

  Füge `src/nanocell/nanocell.cpp` zu `bruce_core` hinzu. Lege `bruce_nanocell_tests` aus `tests/unit/nanocell_tests.cpp` an, linke es gegen `bruce_core`, übernehme `-Wall -Wextra -Werror` und registriere `add_test(NAME nanocell_contracts COMMAND bruce_nanocell_tests)`.

  Run: frische vollständige Kopie nach `/tmp`, dann `cmake -S <copy> -B <build> -G Ninja -DBUILD_TESTING=ON`, `cmake --build <build> --target bruce_nanocell_tests` und `ctest --test-dir <build> -R '^nanocell_contracts$' --output-on-failure`.

  Expected: FAIL, weil die NanoCell-Schnittstelle noch fehlt.

- [ ] **Step 3: Implementiere die Daten- und Operatoroberfläche in `include/bruce/nanocell/nanocell.hpp` und `src/nanocell/nanocell.cpp`**

  `NOT` verlangt Operand 0; `AND`, `OR` und `XOR` verwenden den Operand als Byte. `ROTL`, `SHL` und `SHR` akzeptieren nur Operand 0..7 und berechnen in mindestens `unsigned` Breite mit anschließender Rückwandlung zu `std::uint8_t`, damit keine Shift- oder Promotionsfalle entsteht. Die drei Fabrikdefinitionen sind vollständig fest: `masked-match-v1` ist `AND 0xF0` mit Maske/Wert `0xF0/0xA0`; `rotate-match-v1` ist `ROTL 1`, `AND 0x0F` mit `0x0F/0x05`; `context-conflict-v1` ist der Maskenlauf mit einem anderen fest kompilierten Kontextdigest. Noch keine Ausführung, Validierung oder Serialisierung hinzufügen.

- [ ] **Step 4: Führe den reinen Operatorvertrag grün aus**

  Run: `ctest --test-dir <build> -R '^nanocell_contracts$' --output-on-failure` nach erneuter Konfiguration der frischen Kopie.

  Expected: PASS; alle vollständigen Wahrheitstabellen laufen ohne Warnung oder undefiniertes Shiftverhalten.

### Task 3: Deterministische Einzelzell-Ausführung mit Kontext, Budget und Provenienz

**Files:**
- Modify: `include/bruce/nanocell/nanocell.hpp`
- Modify: `src/nanocell/nanocell.cpp`
- Modify: `tests/unit/nanocell_tests.cpp`

**Interfaces:**
- Consumes: `NanoNeuronCoreV1`, `NanoCellContextV1`, `NanoCellTechniqueDefinitionV1`, `NanoCellByteOpV1` und die festen Techniken aus Task 2.
- Produces:

  ```cpp
  namespace bruce {
  enum class NanoCellStatusV1 { matched, no_match, context_rejected, incomplete };
  enum class NanoCellTraceKindV1 { start, operation, terminal };
  struct NanoCellTraceEntryV1 { NanoCellTraceKindV1 kind; std::uint64_t operations_used; std::uint8_t state; };
  struct NanoCellRequestV1 { std::string request_version{"nanocell-request-v1"}; NanoNeuronCoreV1 core; NanoCellContextV1 context; NanoCellTechniqueDefinitionV1 technique; std::uint64_t max_operations{}; std::uint64_t max_trace_entries{}; };
  struct NanoCellAnswerPointV1 { std::string answer_version{"nanocell-answer-v1"}; NanoCellStatusV1 status; std::optional<std::uint8_t> final_state; std::string technique_id; std::string technique_version; std::string core_layout_version; std::uint8_t active_index{}; std::string core_digest; std::string context_id; std::string context_digest; std::string technique_digest; std::uint64_t operations_used{}; std::vector<NanoCellTraceEntryV1> trace; };
  struct NanoCellControlV1 { std::function<bool()> cancelled; };
  struct NanoCellRunV1 { NanoCellRequestV1 request; NanoCellAnswerPointV1 answer; };
  NanoCellRunV1 execute_nanocell_v1(const NanoCellRequestV1& request, const NanoCellControlV1& control = {});
  }
  ```

- [ ] **Step 1: Schreibe die fehlschlagenden Ausführungsfälle in `tests/unit/nanocell_tests.cpp`**

  Erzeuge Testhelfer mit einem 8-Byte-Core und SHA-256-korrekten Kontexten. Prüfe: `masked-match-v1` mit aktivem Byte `0xA5` ergibt `MATCHED`; ein anderer oberer Nibble ergibt `NO_MATCH`; `rotate-match-v1` mit `0x82` zeichnet `0x82 -> 0x05 -> 0x05` (Start, beide Operatorergebnisse) vor dem Terminal auf; `context-conflict-v1` mit gleichem Byte ergibt `CONTEXT_REJECTED` und `operations_used == 0`.

- [ ] **Step 2: Ergänze die Budget-, Eingabe- und Abbruchtests und verifiziere Rot**

  Prüfe `max_operations == 0` vor dem ersten Opcode und einen Stopp mitten im Rotationsprogramm als `INCOMPLETE`, jeweils ohne Match. Prüfe die Traceformel `1 + n + 1` für die maximal zulässige Programmlänge sowie die festen V1-Grenzfälle, ohne eine vierte oder konfigurierbare Technik auszuführen. Prüfe ungültige Versionen, IDs, Kontextdigest, Layout, Index, Programmgröße, unbekannte oder ungültig parametrisierte Opcodes, abweichende Technikfelder und die Tracegrenzen. Prüfe einen Callback, der vor dem ersten Opcode wahr wird, auf `NanoCellCancelled` ohne zurückgegebenen Lauf.

  Run: `ctest --test-dir <build> -R '^nanocell_contracts$' --output-on-failure`.

  Expected: FAIL, weil `execute_nanocell_v1` fehlt.

- [ ] **Step 3: Implementiere `execute_nanocell_v1` als abgeschlossene Referenzmaschine**

  Validiere zuerst Request, Kontextdigest und die exakte Gleichheit zu einer der drei Fabriktechniken. Berechne `core_digest` und `technique_digest` nach den oben festgelegten separaten binären Digestströmen; `context_digest` ist immer `sha256(context_bytes)`. Füge Starttrace an, prüfe dann Kontextgleichheit vor Callback und Opcode. Vor jedem Opcode prüfe Callback, Operationsbudget und den Platz für Ergebnis plus Terminal. Nach vollständigem Programm oder Budgetstopp füge genau einen Terminaltrace an. Nur `MATCHED`, `NO_MATCH` und `INCOMPLETE` erhalten `final_state`; `CONTEXT_REJECTED` nicht.

- [ ] **Step 4: Prüfe Provenienz, Ausführung und Abbruch grün**

  Ergänze den Test, dass zwei Läufe mit gleichem `final_state` aber anderer fester Technik oder anderem Kontext nicht dieselben Felder `technique_id`, `technique_digest`, `context_id` und `context_digest` besitzen. Fixiere außerdem einen unabhängig berechneten, hart kodierten Digestvektor aus einem gültigen Request für Core und Technik; prüfe Execute und später Pack/Replay gegen genau diese Werte. Führe danach den NanoCell-Vertrag aus.

  Run: `ctest --test-dir <build> -R '^nanocell_contracts$' --output-on-failure`.

  Expected: PASS; `MATCHED` bleibt ausschließlich ein lokaler Byte-/Kontextbefund und erzeugt keine Application-Gates.

### Task 4: Kanonisches NanoCell-Bundle und Replay-Verifikation

**Files:**
- Modify: `include/bruce/nanocell/nanocell.hpp`
- Modify: `src/nanocell/nanocell.cpp`
- Modify: `tests/unit/nanocell_tests.cpp`

**Interfaces:**
- Consumes: `NanoCellRunV1` und `execute_nanocell_v1` aus Task 3 sowie `bruce::sha256`.
- Produces:

  ```cpp
  namespace bruce {
  std::string pack_nanocell_run_v1(const NanoCellRunV1& run);
  NanoCellRunV1 replay_nanocell_run_v1(std::string_view bundle);
  }
  ```

- [ ] **Step 1: Schreibe die fehlschlagenden Replay- und Manipulationstests**

  Für `MATCHED`, `NO_MATCH`, `CONTEXT_REJECTED` und `INCOMPLETE` muss `pack_nanocell_run_v1(replay_nanocell_run_v1(bundle)) == bundle` gelten. Ein erneuter Execute- und Packlauf mit identischem Request muss dieselben Bytes liefern. Der Abbruchfall bleibt ein Fehler und besitzt absichtlich kein Bundle. Teste beschädigten äußeren Digest, unbekannten Header, Restbytes, führende Null in einer Länge sowie je eine manipulierte Request-, Answer- und Trace-Nutzlast, bei der der äußere Digest absichtlich neu berechnet wird.

- [ ] **Step 2: Führe den Replaytest rot aus**

  Run: `ctest --test-dir <build> -R '^nanocell_contracts$' --output-on-failure`.

  Expected: FAIL, weil Packen und Replay fehlen.

- [ ] **Step 3: Implementiere `pack_nanocell_run_v1` und `replay_nanocell_run_v1`**

  Verwende das etablierte ASCII-Length-Prefix-Schema `decimal-length ':' bytes` ohne führende Null. Der Bundlekörper enthält in fester Reihenfolge kanonische Requestbytes, deren SHA-256, kanonische Answer-Metadaten ohne Trace und kanonische Tracebytes; davor steht `BRUCE_NANOCELL_RUN_V1\n`, danach steht der SHA-256 des gesamten Körpers in der zweiten Zeile. Replay prüft zunächst Größen, Header, Digest und Feldgrenzen, dekodiert strikt, validiert den Request erneut, führt ihn ohne Control erneut aus und verlangt die exakte Gleichheit von Answer und Trace; abschließend verlangt er byteidentisches Repacken. Begrenze Bundle und einzelne variable Felder auf die aus Requestgrenzen ableitbaren sicheren Größen; keine Dateiausgabe implementieren.

- [ ] **Step 4: Führe den fokussierten und vollständigen Nachweis aus**

  Run: frische vollständige Kopie nach `/tmp`; `cmake -S <copy> -B <build> -G Ninja -DBUILD_TESTING=ON`; `cmake --build <build> --parallel 8`; `ctest --test-dir <build> -R '^nanocell_contracts$' --output-on-failure`; anschließend `ctest --test-dir <build> --output-on-failure --parallel 8`.

  Expected: der NanoCell-Contract-Test und der gesamte vorhandene Core-Testlauf bestehen. Berichte die tatsächlich gezählte Anzahl, statt eine Testzahl vorauszusetzen.

## Self-Review

1. **Spec coverage:** Der Plan deckt Bytekern, alle drei festen Techniken, Contextbindung, Budgets, Trace, Abbruch, AnswerPoint-Provenienz, kanonisches Bundle, Replay und Volltest ab. Application-Adapter, CLI, Gates, Experience, Plugins und Phase-DAG bleiben wie verlangt außerhalb des Plans.
2. **Step scan:** Jede Produktionsaufgabe hat einen konkreten Test, erwarteten Rotlauf, benannte Schnittstellen, minimale Implementierung und Grünlauf. Task 1 ist absichtlich ein Freigabegate, weil die freigegebene Spezifikation selbst einen arithmetischen Tracewiderspruch enthält.
3. **Type consistency:** Alle späteren Aufgaben nutzen die in Task 2 und 3 definierten Namen `NanoCellRunV1`, `execute_nanocell_v1`, `pack_nanocell_run_v1` und `replay_nanocell_run_v1`; kein `ApplicationRun`- oder CLI-Typ tritt auf.
4. **Review Focus:** Alle fünf Risiken sind in Tasks 2 bis 4 jeweils mit einem konkret benannten Test abgedeckt, einschließlich der erneuerten äußeren Prüfsumme.
5. **Proportion:** Der Plan legt Verträge, Grenzwerte und Prüfschritte fest, ohne Implementierungslogik auszuschreiben; seine vier kleinen, einzeln prüfbaren Aufgaben entsprechen dem eng abgegrenzten Referenzpaket.
