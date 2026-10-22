# Phase-DAG V2 – statischer NanoCell-Adapter

## Status, Ziel und Belege

**VORSCHLAG.** Diese Spezifikation beschreibt einen neuen,
versionsgetrennten Phase-DAG-V2-Vertrag. Sie verändert den implementierten
Phase-DAG-V1-Referenzkern nicht und behauptet keine vorhandene V2-Umsetzung.

**IMPLEMENTIERT UND BELEGT:**

- `include/bruce/phase_dag/phase_dag.hpp` und `src/phase_dag/phase_dag.cpp` enthalten
  V1 mit geschlossenen Fixture-Adaptern, kanonischem Bundle und Replay.
- `bruce_mod_nanocell` enthält einen festen NanoCell-Executor, dessen
  kanonischen Run- und Mod-Envelope-Replay sowie die drei Referenztechniken.
- Die vollständige vorhandene CTest-Suite bestand nach V1 mit 149/149 Gruppen.

**Ziel:** V2 führt einen endlichen, beim Build fest zusammengestellten
Adapterkatalog ein. Ein separates statisches Modul stellt den einzigen neuen
Adapter `nanocell-phase-v1` bereit. Der Adapter führt einen bestehenden,
lokalen NanoCell-Run aus und hält dessen Run und Mod-Envelope als Evidenz fest.

Ein NanoCell-Status bleibt ein lokaler Byte- und Kontextbefund. Er begründet
keine mathematische Wahrheit, keinen Gate-Entscheid, kein Lernen, keine
Veröffentlichung und keine Berechtigung.

## Problem und Architekturgrenze

NanoCell wird als `bruce_mod_nanocell` gebaut und hängt über
`bruce_mod_runtime` von `bruce_core` ab. Der V1-Scheduler liegt dagegen in
`bruce_core`. Würde V1 NanoCell direkt aufrufen, entstünde eine zyklische
Bibliotheksabhängigkeit. Zugleich garantiert V1 bewusst nur seine zwei
privaten Fixture-Adapter.

V2 trennt deshalb den generischen Schedulerkern von statisch gelinkten
Adapterimplementierungen:

```text
bruce_core
├── Phase-DAG V1 (unverändert)
└── Phase-DAG V2 (Spec, Scheduler, Receipt, Bundle, Replay)

bruce_mod_nanocell
└── vorhandener NanoCell-Executor und Mod-Envelope

bruce_phase_dag_nanocell
├── hängt von Phase-DAG V2 und bruce_mod_nanocell ab
└── implementiert ausschließlich nanocell-phase-v1
```

Der V2-Katalog wird zur Buildzeit aus festen Adapterbeschreibungen erzeugt.
Er lädt keine Dateien, Shared Libraries oder Netzwerkressourcen und akzeptiert
keine Laufzeitregistrierung. Eine V2-Spec nennt jeweils Adapter-ID und
Adapter-Version; Replay verlangt, dass diese Kombination im identischen
statischen Katalog vorhanden ist.

## V2-Datenvertrag

`PhaseDagSpecV2` bleibt auf 1 bis 16 byteweise sortierte Nodes mit höchstens
15 Vorgängern und 64 Kanten begrenzt. IDs folgen `[A-Za-z0-9_.-]{1,64}`.
Die Knoten behalten Requestbytes und deren SHA-256-Digest. V2 erhält eine
eigene Bundlekennung `BRUCE_PHASE_DAG_RUN_V2`; V1-Bundles werden nicht als V2
interpretiert.

Ein Adapter besitzt mindestens:

```text
adapter_id
adapter_version
execute(canonical_request_bytes, ordered_input_refs) -> adapter result
```

Der Scheduler hält weiter einen eigenen Receipt-Outcome `READY`, `FAILED`,
`BLOCKED` oder `CANCELLED`. Ein fachlicher Adapterstatus darf diesen Outcome
nicht ersetzen.

### NanoCell-Adapterrequest

Der Request für `nanocell-phase-v1` ist eine ASCII-Length-Prefix-Kodierung
ohne führende Nullen. Seine Felder erscheinen genau in dieser Reihenfolge:

1. Codec-Version `nanocell-phase-request-v1`;
2. `core_layout_version`;
3. acht rohe Corebytes;
4. `active_index` als kanonische Dezimalzahl;
5. `context_id`;
6. rohe Kontextbytes;
7. Kontextdigest;
8. Referenztechnik-ID;
9. `max_operations`;
10. `max_trace_entries`.

Die Technik-ID ist ausschließlich `masked-match-v1`, `rotate-match-v1` oder
`context-conflict-v1`. Der Adapter erzeugt die Technikdefinition ausschließlich
über `nanocell_reference_technique_v1`; vom Request gelieferte Programme,
Masken oder Techniken werden nicht akzeptiert.

Alle NanoCell-Grenzen bleiben die vorhandenen Grenzen: acht Corebytes,
`active_index` 0..7, Kontext höchstens 4096 Bytes, höchstens 16 Operationen
und 2..18 Traceeinträge. Der Adapter prüft Request-, Kontext- und
Spec-Digests vor der Ausführung.

### NanoCell-Adapteroutput

Ein erfolgreich ausgeführter NanoCell-Adapter erzeugt einen V2-Output mit:

- `output_status=COMPLETE` für `MATCHED` oder `NO_MATCH`;
- `output_status=INCOMPLETE` für NanoCell `INCOMPLETE`;
- `output_status=REJECTED` für NanoCell `CONTEXT_REJECTED`;
- kanonischen Metadatenbytes mit NanoCell-Status, IDs, Digests,
  Operationszahl und Traceanzahl;
- SHA-256 der Metadatenbytes;
- dem unveränderten kanonischen NanoCell-Mod-Envelope als Evidenzbytes;
- SHA-256 der Evidenzbytes;
- Claim-Scope `nanocell-phase-v1`.

Für jeden dieser vier fachlichen NanoCell-Status lautet der Scheduler-Receipt
`READY`: Der Adapter hat einen kontrollierten, replaybaren Befund geliefert.
Ein ungültiger Request, NanoCell-Fehler, Evidenzfehler oder Überschreitung
des Phase-Receipt-Limits erzeugt `FAILED` und legt keinen Output offen.
Ohne globalen Stopp werden nur Nachfolger dieses Nodes anschließend `BLOCKED`.
Eine Spec mit einer Adapter-ID oder -Version, die im übergebenen statischen
Katalog fehlt, wird bereits bei der Validierung vor der Ausführung abgelehnt;
dafür entsteht kein node-lokaler `FAILED`-Receipt.

## Ausführung, Control und Replay

V2 arbeitet seriell und wählt den lexikographisch kleinsten entscheidungsfähigen
Node. Vor jedem Start und vor dem Blockieren eines noch offenen Nodes haben
Phase-Control und das globale Startbudget Vorrang.
Ein globaler Stopp macht alle noch offenen Nodes `CANCELLED`; laufende Adapter
werden nicht unterbrochen. Der Adapter erhält keine neue NanoCell-Control-
Callback-Schnittstelle. Dadurch sind abgespeicherte V2-Läufe ohne äußeren
Control vollständig reproduzierbar.

V2 packt Spec, Receipts, Outputs und Trace in kanonische Längenfelder und
sichert den Payload mit einem äußeren SHA-256-Digest. Replay führt folgende
Schritte aus:

1. Header, äußeren Digest, Feldlängen und Kanonizität prüfen.
2. V2-Spec einschließlich Adapter-ID und Version validieren.
3. Alle verwendeten Adapter im statischen Katalog nachschlagen.
4. Spec ohne äußeren Control erneut ausführen.
5. Receipts, NanoCell-Metadaten, Evidenzbytes, Digests und Trace bytegenau
   vergleichen.
6. Byteidentisches Repacken verlangen.

Replay führt die im Bundle enthaltene Spec erneut aus. Auch mit neu
berechneter äußerer Prüfsumme werden fehlerhafte, nichtkanonische oder
inkonsistente Daten sowie vom erneuten Lauf abweichende Outputs,
NanoCell-Metadaten und -Evidenz, Receipts, Outcomes, InputRefs und Trace
abgelehnt. Eine gültige, konsistent neu gehashte Änderung eines in sich
geschlossenen Spec-Felds wie `dag_id` kann Replay dagegen ohne eine extern
vertrauenswürdig vorgegebene erwartete Spec-Identität nicht authentifizieren.
Der äußere Digest und das selbstenthaltene Replay liefern diese externe
Bindung nicht.

## Nicht-Ziele

Nicht Teil dieses Pakets sind Änderungen an Phase-DAG V1, dynamische Plugins,
Datei-/Netzwerkzugriffe, CLI-Befehle, `ApplicationTask`, `ExecutionSession`,
Gates, Experience, Publish/Learn-Freigaben, Cockpit, GPU/Parallelisierung,
allgemeine NanoCell-Techniken und allgemeine semantische DAG-Zyklen.

## Abnahmekriterien

Die Tests müssen mindestens belegen:

1. V1 bleibt byte- und verhaltenskompatibel.
2. Ein V2-NanoCell-Node liefert für `MATCHED`, `NO_MATCH`, `INCOMPLETE` und
   `CONTEXT_REJECTED` einen `READY`-Receipt mit dem passenden unabhängigen
   Outputstatus.
3. Fehlerhafte Adapterrequestfelder, Kontextdigests und NanoCell-Grenzen
   werden im Decoder vor einem NanoCell-Aufruf abgelehnt und führen beim
   V2-Lauf zu `FAILED` ohne Output. Fehlerhafte Spec-Digests oder im statischen
   Katalog fehlende Adapter-IDs/-Versionen werden bei der Spec-Validierung
   vor der Ausführung abgelehnt, ohne node-lokalen `FAILED`-Receipt.
4. Ein NanoCell-Fehler blockiert nur abhängige Nodes; unabhängige Nodes laufen
   weiter in kanonischer Reihenfolge.
5. Jede gespeicherte NanoCell-Evidenz lässt sich über den vorhandenen
   Mod-Envelope-Replay und über den V2-Replay nachvollziehen.
6. Rehashed fehlerhafte, nichtkanonische oder inkonsistente Daten und
   Abweichungen der gespeicherten Outputs, Evidenz, Receipts und Trace vom
   erneuten Lauf werden abgelehnt. Gültige, konsistent rehashed Änderungen
   selbstenthaltener Spec-Felder wie `dag_id` sind ohne extern vertraute
   erwartete Spec-Identität nicht authentifizierbar.
7. Ein frischer Release-Build sowie fokussierte und vollständige CTests laufen
   über `scripts/with-host-deps.sh` aus einer temporären Kopie ohne
   problematische Pfadzeichen.

## Offene Folgeentscheidungen

- Ob ein zweiter statischer Adapter neben NanoCell später zugelassen wird;
- ob Phase-DAG V2 einen eigenen CLI-Vertrag erhalten soll;
- wie Cockpit später V2-Evidenz anzeigen kann;
- welche getrennten Verträge für Parallelität, GPU und semantische Zyklen
  erforderlich sind.

## Quellen

- `docs/concepts/affine-phase-dag.md`
- `docs/adr/012-checked-execution-phases.md`
- `docs/superpowers/specs/2026-10-12-phase-dag-v1-design.md`
- `include/bruce/phase_dag/phase_dag.hpp`
- `src/phase_dag/phase_dag.cpp`
- `include/bruce/mods/nanocell/nanocell.hpp`
- `mods/nanocell/src/nanocell.cpp`
- `mods/nanocell/src/mod.cpp`
