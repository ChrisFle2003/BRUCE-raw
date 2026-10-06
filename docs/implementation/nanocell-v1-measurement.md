# NanoCellExecutorV1 – getrennte Größenmessung

## Messkontext

Gemessen am 27.09.2026 auf Linux 6.12.107+deb13-amd64 x86_64 mit GCC 14.2.0 (C++20), CMake 3.31.6, Ninja 1.12.1, OpenSSL 3.5.7 und gmpxx 6.3.0. Die frische vollständige Quellkopie lag unter `/tmp/bruce-nanocell-final-release-9zGglS` (ohne vorhandene `.git`-, `.agents`-, `.codex`- und `build*`-Artefakte). Konfiguration: `Release`, `BUILD_TESTING=ON`; der Probe-Compiler erhielt `-O3 -DNDEBUG -Wall -Wextra -Werror`.

Ausgeführte Befehle (der Kopierbefehl wurde im Quellwurzelverzeichnis gestartet):

```sh
probe_copy_dir=$(mktemp -d /tmp/bruce-nanocell-final-release-XXXXXX)
tar --exclude='./.git' --exclude='./.codex' --exclude='./.agents' --exclude='./build' --exclude='./build-*' -cf - . | tar -C "$probe_copy_dir" -xf -
cmake -S /tmp/bruce-nanocell-final-release-9zGglS -B /tmp/bruce-nanocell-final-release-9zGglS/release -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build /tmp/bruce-nanocell-final-release-9zGglS/release --target bruce_nanocell_tests -j 4
c++ -std=c++20 -O3 -DNDEBUG -Wall -Wextra -Werror -I /tmp/bruce-nanocell-final-release-9zGglS/include /tmp/bruce-nanocell-sdd-UKQWam/nanocell_measure_probe.cpp /tmp/bruce-nanocell-final-release-9zGglS/release/libbruce_core.a -lcrypto -lgmpxx -lgmp -o /tmp/bruce-nanocell-sdd-UKQWam/nanocell_measure_probe
/tmp/bruce-nanocell-sdd-UKQWam/nanocell_measure_probe
```

Für eine Wiederholung einen neuen `probe_copy_dir` verwenden, die folgenden Befehle auf dessen Pfad anpassen und den unten stehenden Probequelltext als `nanocell_measure_probe.cpp` unter `/tmp` speichern. Die Sonde ist kein Produktionscode und verändert keine öffentliche API.

## Definition der Zahlen

Alle Zahlen sind Bytes. `fixed core` zählt exakt die acht Werte in `core_bytes`; `active projection` zählt genau ein ausgewähltes Byte daraus und allokiert keinen weiteren Core. Bei TechniqueDefinition und AnswerPoint ist `strings` die Summe der belegten Stringlängen, unabhängig davon, ob eine Implementierung Small-String-Optimierung verwendet. `program_occupied` und `trace_occupied` sind Anzahl genutzter Vektorelemente mal `sizeof(Element)`; dies enthält ABI-Padding der Elemente, aber keine ungenutzte Vektorkapazität und keinen Allocator-Overhead. `logical_dynamic_total` ist nur die Summe dieser definierten Nutzdaten. `program_raw` zählt zwei Rohbytes je Opcode/Operand-Paar. `trace_raw` zählt pro Eintrag die drei logischen Werte mit 1 + 8 + 1 Bytes, nicht das ASCII-Serialisierungsformat. `bundle_bytes` ist `std::string::size()` des tatsächlich erzeugten `BRUCE_NANOCELL_RUN_V1` inklusive Header, Hashes und Framing.

`sizeof(...)` beschreibt ausschließlich die Compiler-/ABI-spezifische C++-Objekthülle; weder diese Werte noch die Summen oben sind eine Gesamt-RAM-, Heap-, Gesamt-Runtime- oder Performance-Messung.

## Ergebnis

- Fester Core-Payload: **8 Bytes**.
- Aktive Byteprojektion: **1 Byte**, Alias auf eines der acht Corebytes.
- Compiler-/ABI-spezifische Objekthüllen: `sizeof(NanoNeuronCoreV1)=48`, `sizeof(NanoCellTechniqueDefinitionV1)=128`, `sizeof(NanoCellAnswerPointV1)=304`, `sizeof(NanoCellByteOpV1)=8`, `sizeof(NanoCellTraceEntryV1)=24` Bytes.

| Feste TechniqueDefinition | String-Nutzdaten | Programm-Rohbytes | Genutzte Vektorelement-Bytes | Definierte dynamische Nutzdaten |
| --- | ---: | ---: | ---: | ---: |
| `masked-match-v1` | 100 | 2 | 8 | 108 |
| `rotate-match-v1` | 100 | 4 | 16 | 116 |
| `context-conflict-v1` | 104 | 2 | 8 | 112 |

Die vier Runs verwenden den Core `10 20 XX 40 50 60 70 80` (aktiver Index 2), Kontext-ID `reference.context_1`, Kontextbytes `nanocell-reference-context-v1`, die festen Fabriktechniken und Tracebudget 18. `MATCHED` verwendet `masked-match-v1` mit `XX=A5`, `NO_MATCH` dieselbe Technik mit `XX=B5`, `CONTEXT_REJECTED` `context-conflict-v1` mit `XX=A5`, `INCOMPLETE` `rotate-match-v1` mit `XX=82` und Operationsbudget 1. Alle anderen Operationsbudgets sind 16.

| Run | Traceeinträge | Trace-Rohwerte | Genutzte Trace-Vektorbytes | AnswerPoint-Strings | Definierte AnswerPoint-Nutzdaten | Serialisiertes Bundle |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `MATCHED` | 3 | 30 | 72 | 283 | 355 | 833 |
| `NO_MATCH` | 3 | 30 | 72 | 283 | 355 | 833 |
| `CONTEXT_REJECTED` | 2 | 20 | 48 | 287 | 335 | 827 |
| `INCOMPLETE` | 3 | 30 | 72 | 283 | 355 | 825 |

Cache: **nicht implementiert**. Es gibt daher keinen Cache-Payload zu messen.

## Vollständige nicht-produktive Sonde

```cpp
#include "bruce/nanocell/nanocell.hpp"
#include "bruce/evidence/bundle.hpp"
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>

namespace {
bruce::NanoCellRequestV1 request_for(bruce::NanoCellReferenceTechniqueV1 reference, std::uint8_t active) {
    bruce::NanoCellRequestV1 request;
    request.core.core_bytes={0x10,0x20,active,0x40,0x50,0x60,0x70,0x80};
    request.core.active_index=2;
    request.context.context_id="reference.context_1";
    request.context.context_bytes="nanocell-reference-context-v1";
    request.context.context_digest=bruce::sha256(request.context.context_bytes);
    request.technique=bruce::nanocell_reference_technique_v1(reference);
    request.max_operations=16;
    request.max_trace_entries=18;
    return request;
}

std::size_t technique_string_payload(const bruce::NanoCellTechniqueDefinitionV1& technique) {
    return technique.technique_id.size()+technique.technique_version.size()+
           technique.required_context_digest.size();
}

void print_technique(const char* label, bruce::NanoCellReferenceTechniqueV1 reference) {
    const auto technique=bruce::nanocell_reference_technique_v1(reference);
    const auto strings=technique_string_payload(technique);
    const auto raw_program=technique.program.size()*2;
    const auto occupied_program=technique.program.size()*sizeof(bruce::NanoCellByteOpV1);
    std::cout<<"technique "<<label<<" strings="<<strings<<" program_raw="<<raw_program
             <<" program_occupied="<<occupied_program
             <<" logical_dynamic_total="<<strings+occupied_program<<'\n';
}

void print_run(const char* label, const bruce::NanoCellRequestV1& request) {
    const auto run=bruce::execute_nanocell_v1(request);
    const auto& answer=run.answer;
    const auto answer_strings=answer.answer_version.size()+answer.technique_id.size()+
        answer.technique_version.size()+answer.core_layout_version.size()+
        answer.core_digest.size()+answer.context_id.size()+answer.context_digest.size()+
        answer.technique_digest.size();
    const auto trace_raw=answer.trace.size()*
        (sizeof(std::uint8_t)+sizeof(std::uint64_t)+sizeof(std::uint8_t));
    const auto trace_occupied=answer.trace.size()*sizeof(bruce::NanoCellTraceEntryV1);
    const auto bundle=bruce::pack_nanocell_run_v1(run);
    std::cout<<"run "<<label<<" status="<<static_cast<unsigned>(answer.status)
             <<" trace_entries="<<answer.trace.size()<<" trace_raw="<<trace_raw
             <<" trace_occupied="<<trace_occupied
             <<" answer_strings="<<answer_strings
             <<" answer_logical_dynamic_total="<<answer_strings+trace_occupied
             <<" bundle_bytes="<<bundle.size()<<'\n';
}
}

int main() {
    using T=bruce::NanoCellReferenceTechniqueV1;
    std::cout<<"core_fixed_bytes=8 active_projection_bytes=1\n";
    std::cout<<"sizeof_core_shell="<<sizeof(bruce::NanoNeuronCoreV1)
             <<" sizeof_technique_shell="<<sizeof(bruce::NanoCellTechniqueDefinitionV1)
             <<" sizeof_answer_shell="<<sizeof(bruce::NanoCellAnswerPointV1)
             <<" sizeof_op_element="<<sizeof(bruce::NanoCellByteOpV1)
             <<" sizeof_trace_element="<<sizeof(bruce::NanoCellTraceEntryV1)<<'\n';
    print_technique("masked_match",T::masked_match);
    print_technique("rotate_match",T::rotate_match);
    print_technique("context_conflict",T::context_conflict);
    print_run("MATCHED",request_for(T::masked_match,0xA5));
    print_run("NO_MATCH",request_for(T::masked_match,0xB5));
    print_run("CONTEXT_REJECTED",request_for(T::context_conflict,0xA5));
    auto incomplete=request_for(T::rotate_match,0x82);
    incomplete.max_operations=1;
    print_run("INCOMPLETE",incomplete);
}
```
