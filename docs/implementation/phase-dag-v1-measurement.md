# Phase-DAG V1 – Größenmessung

## Status und Aussagegrenze

**IMPLEMENTIERT UND BELEGT.** Ein frischer Release-Build und die vollständige
CTest-Suite bestanden am 12. Oktober 2026 mit **149/149** Testgruppen. Der
Phase-DAG-Vertrag bestand zusätzlich isoliert als `phase_dag_contracts`.
Die Prüfung lief aus einer vollständigen temporären Kopie unter `/tmp`, weil
der Name des gelieferten Arbeitsordners Leerzeichen enthält und `pkgconf` diese
bei gesetztem Sysroot nicht korrekt an CMake weitergibt.

Diese Aufnahme beschreibt ausschließlich die serialisierte Größe eines festen
Phase-DAG-V1-Beispiels. Sie ist keine Aussage über RAM-Verbrauch, Laufzeit,
Durchsatz, Skalierbarkeit oder den Bedarf einer späteren Parallelisierung.

## Messaufbau

Der Lauf nutzt die kanonische Spec `measurement` mit folgenden Nodes:

1. `a`: `fixture-source-v1`, Request `C:left`;
2. `b`: `fixture-source-v1`, Request `C:right`;
3. `c`: `fixture-join-v1`, Vorgänger `a`, `b`, leerer Request.

Die Limits sind `max_started_nodes=3`, `max_receipt_bytes=4096` und
`max_trace_entries=64`. Gemessen wurden die durch
`pack_phase_dag_run_v1()` erzeugten Längenpräfixfelder, einschließlich ihrer
dezimalen Längenpräfixe und Trennzeichen.

| Bestandteil | Gemessene Bytes |
|---|---:|
| Spec | 359 |
| Requests (`a`, `b`, `c`) | 6, 7, 0 |
| InputRefs von `c` (`a`, `b`) | 171, 171 |
| OutputRefs (`a`, `b`, `c`) | 219, 221, 228 |
| Receipts (`a`, `b`, `c`) | 328, 330, 677 |
| Trace-Einträge (8 Einträge) | 13, 12, 10, 12, 10, 12, 10, 13 |
| Innerer Bundle-Payload | 1.792 |
| Vollständiges Bundle inkl. Header und äußerem SHA-256-Digest | 1.880 |

## Reproduzierbarkeit

Die Messung lief am 12. Oktober 2026 auf Linux 6.12.111+deb13-amd64 (`x86_64`)
mit `g++ (Debian 14.2.0-19) 14.2.0` und C++20. Der fokussierte Lauf baut den
Phase-DAG-Test samt echter OpenSSL-SHA-256-Implementierung mit
`-Wall -Wextra -Werror` und führt ihn aus. Die Referenz liegt in
`tests/unit/phase_dag_tests.cpp`; Implementierung und Codec befinden sich in
`src/phase_dag/phase_dag.cpp`.

Die Werte sind bei Änderungen an Feldern, Codec, Compiler oder Plattform neu
zu messen. Ein Vergleich mit anderen Beispielen ist nur zulässig, wenn deren
Spec, Requests, Limits und Kodierungsumfang ebenfalls angegeben werden.
