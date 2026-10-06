# NanoCellExecutorV1 – deterministischer Byte-Referenzvertrag

## Status und Evidenz

`IMPLEMENTIERT UND BELEGT`: Der aktive Core besitzt acht `ApplicationTask`-Varianten, gemeinsame lineare Checked-Execution-Sessions, native Bundle-Replays, lokale Budgets und Gateberichte. Nachweis: `include/bruce/application/application_task.hpp`, `include/bruce/application/execution_session.hpp`, `src/application/execution_session.cpp` und die aktuelle Testsuite.

`DOKUMENTIERT / NICHT IM CODE BESTÄTIGT`: Die Roadmap beschreibt für M5 kompakte NanoNeuron-Zustände, feste Byteoperator-Techniken und AnswerPoints; die Dateien `BRUCE NanoCell Deterministische.md` und `BRUCE NanoCell.txt` enthalten weitergehende Konzepte. Diese Texte belegen keine NanoCell-Runtime.

`VORSCHLAG`: Dieser Vertrag definiert den kleinsten eigenständigen Referenzpfad `NanoCellExecutorV1`. Er ist vor einer Aufnahme in `ApplicationTask`, CLI oder einem allgemeinen Phase-DAG zu realisieren.

`NICHT TEIL DIESES PAKETS`: NanoCell-Code, dynamische Plugins, LLM/Netzwerk, Datenbank, automatische Erfahrungspromotion, GPU/Parallelisierung, allgemeine Scheduler, Affine-Graphen, kryptografische Suche sowie Änderungen an mathematischen Forschungsstrategien.

## Ziel

Eine NanoCell soll zunächst als kleine, vollständig deterministische Byte-Maschine nachweisbar werden. Dieselben Eingabebytes, Technikdefinition, Kontext und Budgets müssen dieselben `AnswerPoint`-Daten, denselben Operatortrace und dasselbe Replaybundle ergeben. Ein identisches Ergebnisbyte darf unterschiedliche Technik- oder Kontextherkunft nicht verlieren.

Erfolg bedeutet **nicht** mathematische Wahrheit, semantisches Verständnis, Lernfreigabe oder eine ausgeführte Phase-DAG. Er bedeutet nur: Der feste Bytevertrag wurde unter seinen Grenzen ausgeführt und reproduziert.

## Abgrenzung und Reihenfolge

Die bestehende `ExecutionSession` bleibt vorerst unverändert. Der NanoCell-Referenzlauf erhält einen eigenen Vertrag und sein eigenes Bundle; erst nach vollständiger Referenzabnahme entscheidet ein separater Adaptervertrag, ob und wie er zu `ApplicationTask`, dessen Renderer, Gates und Checked-Execution hinzugefügt wird.

```text
V1 jetzt:       Request -> NanoCellExecutorV1 -> AnswerPoint + Trace -> NanoCell replay
später, separat: geprüfter NanoCell-Adapter -> ApplicationTask -> CLI
noch später:     mehrere Adapter mit Abhängigkeiten -> Phase-DAG
```

Damit bleibt ein fehlerhafter oder unvollständiger NanoCell-Referenzlauf von bestehenden Forschungs-, Grid-, Vergleichs- und Sprachbundles isoliert.

## Verglichene Ansätze

| Ansatz | Vorteil | Risiko | Entscheidung |
| --- | --- | --- | --- |
| Direkt neue `ApplicationTask`-Variante | sofortige CLI-Integration | Gate-, Assessment- und Bundle-Semantik wären vor dem Bytekern offen | nicht jetzt |
| Eigenständiger Byte-Referenzexecutor | kleine Testfläche, klarer Replayvertrag, keine Gateverwechslung | noch kein CLI-Befehl | **V1** |
| NanoCell zusammen mit Phase-DAG | ein gemeinsamer großer Zielentwurf | zwei ungeprüfte Runtime-Verträge koppeln sich | nicht jetzt |

## Datenvertrag V1

Alle Namen, Formate und Grenzwerte unten sind Teil des künftigen V1-Vertrags; eine spätere Erweiterung benötigt eine neue Version.

### NanoNeuronCoreV1

```text
core_bytes: exakt 8 Bytes
active_index: UInt8 0..7
active_state: core_bytes[active_index]
core_layout_version: "nanoneuron-core-v1"
```

Die acht Bytes sind der kanonische Core-Datensatz. `active_state` ist nur die aktive Byteprojektion für die Byteprogramme; sie darf nicht als vollständiger Speicherbedarf oder als Bedeutung des Neurons ausgegeben werden.

### ContextV1

```text
context_id: 1..64 ASCII-Zeichen [A-Za-z0-9_.-]
context_bytes: 0..4096 Bytes
context_digest: SHA-256(context_bytes)
```

Der Kontext wird als getrennte Eingabe gebunden. Kein Operator darf aus einem übereinstimmenden Ergebnisbyte auf Kontextgleichheit schließen.

### TechniqueDefinitionV1

Eine Technik ist eine feste, versionierte Definition mit:

```text
technique_id: 1..64 ASCII-Zeichen [A-Za-z0-9_.-]
technique_version: "nanocell-technique-v1"
required_context_digest: 64 Hexzeichen
program: 1..16 ByteOpV1
match_mask: UInt8
match_value: UInt8
```

`ByteOpV1` ist genau einer der folgenden Operatoren:

```text
AND literal-byte | OR literal-byte | XOR literal-byte | NOT
ROTL 0..7 | SHL 0..7 | SHR 0..7
```

`SHL` und `SHR` sind logische Operationen auf einem UInt8; die frei werdenden Bits sind null. Rotation ist modulo acht definiert. Ein Shift größer als sieben, ein unbekannter Opcode, leeres Programm, mehr als 16 Operatoren oder eine ungültige Kontextkennung sind Eingabefehler. Sprünge, Seitentabellen und dynamische Operatorprogramme sind bewusst nicht V1.

### NanoCellRequestV1 und Budget

```text
request_version: "nanocell-request-v1"
core: NanoNeuronCoreV1
context: ContextV1
technique: TechniqueDefinitionV1
max_operations: UInt64 0..16
max_trace_entries: UInt64 2..18
```

Die Tracegrenze `1..17` kann nicht gleichzeitig die Startprojektion, 16 Operatorergebnisse und den abschließenden Terminaleintrag abbilden: dafür sind bis zu 18 Einträge erforderlich. Jeder tatsächlich ausgeführte Opcode verbraucht genau eine Operation; Startprojektion, jedes Operatorergebnis und der abschließende Terminaleintrag zählen jeweils als Traceeintrag. Vor einem Opcode werden Operationsgrenze und Traceplatz geprüft. Für die Ausführung des Opcodes müssen sowohl sein Ergebniseintrag als auch der abschließende Terminaleintrag noch Platz haben. Fehlt einer dieser Plätze, wird ohne Ausführung eines weiteren Opcodes `INCOMPLETE` mit Terminaleintrag erzeugt. Daher benötigt auch `CONTEXT_REJECTED` mindestens zwei Einträge: Startprojektion und Terminaleintrag. Ein Budgetstopp erzeugt einen typisierten, replaybaren `INCOMPLETE`-Befund, keinen Fehler und keinen Match.

## Ausführung und AnswerPoint

1. Prüfe Versionen, Bytegrößen, Grenzen und beide Kontextdigests.
2. Leite `active_state` aus `core_bytes[active_index]` ab und füge ihn als Starttrace ein.
3. Wenn der tatsächliche Kontextdigest nicht dem erforderlichen Technikdigest entspricht, liefere ohne Operatorausführung `CONTEXT_REJECTED`.
4. Führe die feste Operatorliste der Reihe nach aus, mit einem Traceeintrag je Ergebnisbyte.
5. Bei Budgetstopp liefere `INCOMPLETE`; andernfalls ist `MATCHED`, wenn `(final_state & match_mask) == match_value`, sonst `NO_MATCH`.

`AnswerPointV1` enthält mindestens:

```text
answer_version: "nanocell-answer-v1"
status: MATCHED | NO_MATCH | CONTEXT_REJECTED | INCOMPLETE
final_state: UInt8, nur bei MATCHED/NO_MATCH/INCOMPLETE
technique_id, technique_version
core_layout_version, active_index
core_digest, context_id, context_digest, technique_digest
operations_used, trace
```

`MATCHED` ist ein Byte-/Kontextmatch für diese feste Technik. Er ist keine semantische Behauptung, kein `VERIFIED_ON_SAMPLES`, kein allgemeiner Beweis und öffnet keine Learn-/Publish-Gates.

### Core- und Technikdigests V1

`core_digest` und `technique_digest` sind SHA-256 über je einen privaten, domänengetrennten binären Feldstrom. Jedes Feld besteht aus seiner Byteanzahl als vorzeichenloser 8-Byte-Big-Endian-Wert, gefolgt von genau diesen Rohbytes. Auch Einbytewerte sind vollständige Felder mit Länge 1. Die Feldreihenfolge ist verbindlich:

```text
core_digest:
  "nanocell-core-digest-v1" (ASCII-Domäne)
  core_layout_version (ASCII-Bytes)
  active_index (ein Rohbyte)
  core_bytes (acht Rohbytes in Arrayreihenfolge)

technique_digest:
  "nanocell-technique-digest-v1" (ASCII-Domäne)
  technique_id (ASCII-Bytes)
  technique_version (ASCII-Bytes)
  required_context_digest (64 ASCII-Hexbytes)
  program (je Opcode sein Enumwert als Rohbyte, direkt gefolgt vom Operanden als Rohbyte)
  match_mask (ein Rohbyte)
  match_value (ein Rohbyte)
```

Die Opcodewerte in V1 sind `AND=0`, `OR=1`, `XOR=2`, `NOT=3`, `ROTL=4`, `SHL=5`, `SHR=6`. `context_digest` bleibt separat `SHA-256(context_bytes)` als 64-stellige Kleinbuchstaben-Hexdarstellung. Die ASCII-Dezimallängen mit `:` gelten ausschließlich für den unten beschriebenen Request-/Answer-/Trace-/Bundlecodec `BRUCE_NANOCELL_RUN_V1`, nicht für diese beiden Provenienz-Digestströme. Ein festes Beispiel und der unabhängige Digestvektor stehen in `tests/unit/nanocell_tests.cpp`.

Fester gültiger Vektor: `core_bytes=10 20 A5 40 50 60 70 80` (Hex), `active_index=2`, `core_layout_version=nanoneuron-core-v1`, Kontext-ID `reference.context_1`, Kontextbytes `nanocell-reference-context-v1` und die unveränderte feste Technik `masked-match-v1`. Bei `max_operations=16` und `max_trace_entries=18` ist der Lauf `MATCHED` mit `final_state=A0`. Der Kontextdigest lautet `5b47363caa3874e531410c47f94eafffb38576742029c792740883de00b0ae64`; unabhängig berechnet ergeben sich `core_digest=df6c4838075217518af6bd509dba5f4dfef027f3fcf767557f47c9111062f024` und `technique_digest=8520761a8f085f128fd6d484133f3b9ef5325d9d7af3186bd190e1061b7c12bb`. Execute sowie Pack/Replay müssen diese Werte erhalten.

## Referenztechniken und Testvektoren

Die erste Implementierung enthält ausschließlich diese drei fest verdrahteten Referenztechniken; sie sind Fixtures, keine konfigurierbaren Plugins.

| ID | Programm und Bedingung | Erwartete Grenze |
| --- | --- | --- |
| `masked-match-v1` | `AND 0xF0`; match `0xA0` | Match nur für obere Nibble `A` |
| `rotate-match-v1` | `ROTL 1`, `AND 0x0F`; match `0x05` | Rotation muss vor Match sichtbar im Trace stehen |
| `context-conflict-v1` | wie `masked-match-v1`, aber mit einem anderen gebundenen Kontextdigest | gleiches Byte darf `CONTEXT_REJECTED` liefern |

Zusätzlich definiert eine reine Byteoperator-Referenzfunktion `and8(a,b)`, `or8(a,b)` und `xor8(a,b)`. Ihre Wahrheitstabellen werden über alle 65.536 geordneten Bytepaare geprüft. Unary NOT, alle Shiftweiten und alle Rotationsweiten werden über alle 256 Eingabebytes geprüft.

## Replay- und Speichervertrag

`BRUCE_NANOCELL_RUN_V1` enthält mit ASCII-Dezimallängen (`decimal-length ':' bytes`) kanonisch length-prefix-kodiert:

```text
request bytes + request SHA-256
answer bytes + trace bytes
outer SHA-256
```

Replay prüft Header, Längen, äußeren Digest, kanonische Requestform, Core-/Kontext-/Technikdigests, Operatortrace, Zähler, Status und rekodiert anschließend byteidentisch. Veränderte Bytes, erneuerte äußere Prüfsumme mit manipuliertem Inhalt, unbekannte Versionen, nichtkanonische Längen und Restbytes schlagen fehl. Das Bundle speichert keine Experience und schreibt keine Dateien selbst.

## Abbruch, Fehler und Rückfall

`NanoCellControlV1` darf einen optionalen booleschen Abbruchcallback **nur vor einem Opcode** prüfen. Ein ausgelöster Callback erzeugt einen klaren `NanoCellCancelled`-Fehler ohne `AnswerPoint` und ohne Bundle. Er ist absichtlich von `INCOMPLETE` durch native Budgetgrenzen getrennt.

Ein Fehler beim Kontext-/Formatcheck oder beim Replay erzeugt ebenfalls keinen Teilbefund. V1 besitzt keine globale Zeit-, RAM- oder Threadgrenze und keine Abbruchweitergabe aus CLI oder Sprachschicht.

Der Referenzexecutor ist ein separater Bibliotheksbaustein. Kann er nicht gebaut oder geprüft werden, wird er nicht an den bestehenden Core angeschlossen; vorhandene Bundles, CLI-Aufrufe und die Sprachschnittstelle bleiben unverändert.

## Abnahme

- Unit: vollständige Wahrheitstabellen für AND/OR/XOR; vollständige 256-Werte-Tabellen für NOT, jede Shift- und Rotationsweite.
- Unit: jedes ungültige Feld, Kontextdigestkonflikt, Budget vor dem ersten Opcode und Budget mitten im Programm.
- Unit: gleicher Ergebnisbytewert mit anderer Technik-/Kontextherkunft ist nicht gleiches `AnswerPoint`-Provenienzobjekt.
- Replay: Erfolg, `NO_MATCH`, `CONTEXT_REJECTED`, `INCOMPLETE`, Abbruchfehler und alle relevanten Manipulationsarten.
- Integration: alle drei Referenztechniken, Neustart/Replay und byteidentische Bundlewiederholung.
- Messung: getrennt berichten für 8-Byte-Core, aktive Byteprojektion, TechniqueDefinition, Trace, AnswerPoint, Bundle und optionalen Cache. Keine Gesamt-Runtimebehauptung aus dem aktiven Byte ableiten.

## Ausdrücklich offen nach V1

1. Adapter in `ApplicationTask`, Renderer, Assessment und Gatebericht.
2. CLI-Import/Export, Dateigrenzen und Signalcancellation für NanoCell.
3. Experience-Speicherung und ein eigener, quellengebundener Lernvertrag.
4. Sprungtabellen, größere Operatorprogramme, konfigurierbare Techniken und dynamische Plugins.
5. Mehrere NanoCells mit Abhängigkeiten sowie eine echte Phase-DAG; eine spätere DAG muss Zustand, Ergebniswert und Provenienz getrennt modellieren.
6. NanoCell-/DAG-Parallelisierung oder GPU-Ausführung.

## Freigabegate

Erst nach fachlicher Prüfung dieser Spezifikation wird ein eigener Implementierungsplan erstellt. Er beginnt mit Byteoperator- und Replaytests vor Produktionscode, führt keine globale Runtime ein und schließt mit einem frischen vollständigen Core-Testlauf.
