# Phase-DAG V2 mit NanoCell – Größenmessung und Abnahme

## Status und Aussagegrenze

**IMPLEMENTIERT UND BELEGT.** Am 12. Oktober 2026 wurden ein frischer
Release-Build aus einer vollständigen temporären Quellkopie, die fünf
fokussierten Verträge (5/5) und die vollständige verfügbare CTest-Suite
(151/151) mit Exitcode 0 abgeschlossen. Die vier NanoCell-Terminalstatus
wurden mit dem [V2-NanoCell-Vertragstest](../../tests/mods/nanocell/phase_dag_nanocell_tests.cpp)
als serialisierte V2-Bundles gemessen.

Die Zahlen gelten nur für die hier beschriebenen Ein-Node-Beispiele und den
aktuellen kanonischen Codec. Sie sind keine Aussage über RAM-Verbrauch,
Laufzeit, Durchsatz oder Skalierbarkeit. Die
[V2-Spezifikation](../superpowers/specs/2026-10-12-phase-dag-v2-nanocell-adapter-design.md)
enthält Ziel und Grenzen; Implementierungsbelege sind der
[V2-Kern](../../src/phase_dag/phase_dag_v2.cpp), der
[NanoCell-Adapter](../../mods/nanocell/src/phase_dag_adapter_v2.cpp) und die
genannten Tests.

## Reproduzierbarer Prüflauf

Die Quelle wurde vollständig nach `/tmp/bruce-phase-dag-v2-source` kopiert,
einschließlich `build-host-deps/`, das `scripts/with-host-deps.sh` benötigt.
Die bereits vorhandenen gleichnamigen temporären Verzeichnisse wurden vorher
unter `*-prior-task4` gesichert. Der Aufbau dieses Laufs erfolgte aus dem
gelieferten Arbeitsverzeichnis mit diesen Befehlen:

```sh
cd '/home/chrisi/Downloads/My-Bruce-AI-main-beta-002 (2)'
mv /tmp/bruce-phase-dag-v2-source /tmp/bruce-phase-dag-v2-source-prior-task4
mv /tmp/bruce-phase-dag-v2-build /tmp/bruce-phase-dag-v2-build-prior-task4
mkdir -p /tmp/bruce-phase-dag-v2-source
cp -a . /tmp/bruce-phase-dag-v2-source/
cd /tmp/bruce-phase-dag-v2-source
```

Von dort liefen genau diese Prüfungen, alle mit Exitcode 0:

```sh
scripts/with-host-deps.sh cmake -G Ninja -S . -B /tmp/bruce-phase-dag-v2-build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
scripts/with-host-deps.sh cmake --build /tmp/bruce-phase-dag-v2-build
scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-phase-dag-v2-build -R '^(phase_dag_contracts|phase_dag_v2_contracts|phase_dag_nanocell_contracts|nanocell_contracts|nanocell_mod_contracts)$' --output-on-failure
scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-phase-dag-v2-build --output-on-failure --parallel 8
```

| Prüfung | Bestanden | Fehlgeschlagen | Übersprungen | Exitcode |
|---|---:|---:|---:|---:|
| Fokussierter CTest | 5 | 0 | 0 | 0 |
| Vollständiger CTest | 151 | 0 | 0 | 0 |

Der Lauf erfolgte auf Linux 6.12.111+deb13-amd64 (`x86_64`) mit
`g++ (Debian 14.2.0-19) 14.2.0` und C++20. Es gab keine beobachtete
Umgebungsbeschränkung im Prüflauf.

## Gemessene Bundlegrößen

Nach der vollständigen CTest-Abnahme wurde **nur in der temporären Kopie**
eine Ausgabe von `bundle.size()` unmittelbar nach
`pack_phase_dag_run_v2(run)` im NanoCell-Vertragstest ergänzt. Danach wurden
`bruce_phase_dag_nanocell_tests` neu gebaut und das Testprogramm über
`scripts/with-host-deps.sh` ausgeführt; beide Befehle endeten mit Exitcode 0.
Die Instrumentierung gibt ausschließlich Status, Technik-ID, Operations- und
Traceanzahl sowie Bundlelänge aus. Das Bundle enthält den V2-Header, den
äußeren Digest und den gesamten kanonischen Payload einschließlich Spec,
Receipt, Output, NanoCell-Mod-Evidenz und Trace.

| NanoCell-Status | Technik | Operationen | Traceeinträge | V2-Bundlebytes |
|---|---|---:|---:|---:|
| `MATCHED` | `masked-match-v1` | 1 | 3 | 2.609 |
| `NO_MATCH` | `masked-match-v1` | 1 | 3 | 2.610 |
| `INCOMPLETE` (Operationslimit) | `rotate-match-v1` | 1 | 3 | 2.605 |
| `INCOMPLETE` (Tracelimit, zusätzlicher Testpfad) | `rotate-match-v1` | 0 | 2 | 2.600 |
| `CONTEXT_REJECTED` | `context-conflict-v1` | 0 | 2 | 2.625 |

Alle Fälle verwenden `dag_id=nanocell-adapter-test`, einen Node `a`,
`max_receipt_bytes=65536`, `max_trace_entries=32`, den statischen Adapter
`nanocell-phase-v1` in Version `v1`, acht Corebytes, Kontext-ID
`reference.context_1` und Kontextbytes `nanocell-reference-context-v1`.
Die Requests und abweichenden Limits stehen im verlinkten Vertragstest.

Der [V2-Packer](../../src/phase_dag/phase_dag_v2.cpp) setzt das Bundle aus
`BRUCE_PHASE_DAG_RUN_V2`, einem **SHA-256-Digest des Payloads**, einem
Zeilenumbruch und dem Payload zusammen. Replay prüft denselben äußeren Digest
und verlangt byteidentisches erneutes Packen.

## V1-Grenze

Der fokussierte `phase_dag_contracts`-Test und die volle Suite bestanden.
Zusätzlich waren V1-Header, V1-Implementierung und V1-Vertragstest bytegleich
mit der vor diesem Prüflauf gesicherten temporären Quellkopie (`cmp`, jeweils
Exitcode 0). Damit ist V1 gegenüber dieser Kopie unverändert und sein
aktueller Vertrag geprüft. Ein historischer Bytevergleich eines vollständigen
V1-Bundles gegen einen früheren Release wurde in diesem Lauf nicht gemessen;
hieraus wird keine solche Vergleichsaussage abgeleitet.
