# NanoCell-Mod V1 – Architekturdesign

**Status:** TEILWEISE IMPLEMENTIERT UND BELEGT. Die hier festgelegte statische
NanoCell-Mod-Grenze, der äußere Herkunftsumschlag, die direkte V1-Read-only-
Kompatibilität und die Deaktivierung sind implementiert und getestet. Allgemeine
NanoCell-Aufträge, Cockpit-/LLM-Anbindung und weitere Techniken bleiben
DOKUMENTIERT / KONZEPT.

**Ziel:** Der vorhandene NanoCell-V1-Kern wird als optionaler, statisch gebauter
Mod in den festen BRUCE-Mod-Katalog überführt. Sein lokaler technischer Befund
bleibt ausdrücklich von mathematischer Evidenz, Core-Gates, Regelentscheidungen
und `ApplicationTask` getrennt.

**Ausgangspunkt:** `mods/nanocell/src/nanocell.cpp` liefert drei feste,
deterministische Referenztechniken, begrenzte Operationen/Traces sowie den
kanonischen `BRUCE_NANOCELL_RUN_V1`-Run und dessen Replay. Die allgemeine
Mod-API V1 liefert Manifestprüfung, Katalog und einen Core-Adapter für die acht
bestehenden `ApplicationTask`-Varianten. NanoCell ist keine dieser Varianten.

## Festgelegte Entscheidung

NanoCell wird als **Option-1-Mod** umgesetzt:

- `BRUCE_BUILD_MOD_NANOCELL` baut den Mod als eigenen statischen CMake-Target
  `bruce_mod_nanocell` oder lässt ihn vollständig aus Build, Katalog und CLI
  weg.
- Der Mod erhält ein Manifest mit ID `nanocell`, Version `nanocell-mod-v1`,
  `bruce-mod-api-v1` und der neuen Fähigkeit `nanocell_execution`.
- Der bestehende innere NanoCell-V1-Run bleibt bytegenau und direkt replaybar.
  Ein neuer Export wird außen mit `BRUCE_MOD_RUN_V1` umhüllt und bindet damit
  Mod-ID, Mod-Version, kanonische NanoCell-Anfrage und den inneren Run.
- `nanocell replay` akzeptiert weiterhin einen vorhandenen inneren V1-Run und
  zusätzlich den neuen äußeren Mod-Run. Ein neuer `nanocell run --output`
  erzeugt ausschließlich den äußeren Mod-Run.

Der Mod verwendet die vorhandenen Manifest- und Umschlagfunktionen der
Mod-API, ruft aber **nicht** `execute_mod_core_run` auf: Dieses ist bewusst nur
für bereits vorhandene `ApplicationTask`-Läufe. Ein `MATCHED` bleibt daher
`NanoCellStatusV1::matched`, nicht `VERIFIED_ON_SAMPLES`, kein Gate-Erfolg und
keine Regeländerung.

## Zielstruktur

```text
include/bruce/mods/nanocell/
  nanocell.hpp                  # öffentlicher NanoCell-V1-Kern und Mod-Run-Typen
  mod.hpp                       # NanoCell-Mod-Manifest
mods/nanocell/
  CMakeLists.txt                # statischer Target bruce_mod_nanocell
  src/nanocell.cpp              # bestehender V1-Kern und direkte V1-Replays
  src/mod.cpp                   # Manifest und äußerer Mod-Run
tests/mods/nanocell/
  nanocell_tests.cpp            # bestehender Kernvertrag nach dem Umzug
  nanocell_mod_tests.cpp        # Manifest-, Umschlag- und Herkunftsvertrag
  test_nanocell.py              # CLI, Direkt-V1- und äußerer Mod-Replay
```

`include/bruce/nanocell/nanocell.hpp` bleibt für diesen Schritt ein schmaler
Weiterleitungsheader auf den neuen öffentlichen Mod-Pfad. Das erhält interne
Quellkompatibilität, ohne NanoCell erneut in `bruce_core` zu verlinken. Seine
spätere Entfernung ist eine eigene Kompatibilitätsentscheidung.

## Mod-Runvertrag

Der Mod ergänzt folgende öffentliche Typen/Funktionen im NanoCell-Mod-Header:

```cpp
struct NanoCellModRunV1 { NanoCellRunV1 nanocell; };

const ModManifestV1& nanocell_mod_manifest_v1();
NanoCellModRunV1 execute_nanocell_mod_v1(
    const NanoCellRequestV1&, const NanoCellControlV1& = {});
std::string pack_nanocell_mod_run_v1(const NanoCellModRunV1&);
NanoCellModRunV1 replay_nanocell_mod_run_v1(std::string_view);
```

Beim Export bildet der Mod einen `ModRunEnvelopeV1` mit:

- `input_codec = "bruce-nanocell-request-v1"` und einer kanonischen,
  ausschließlich aus `NanoCellRequestV1` gewonnenen Eingabe;
- `payload_codec = "bruce-nanocell-run-v1"` und dem unveränderten inneren
  `BRUCE_NANOCELL_RUN_V1`-Bundle.

Beim Replay müssen Manifest und beide Codecs exakt passen. Der dekodierte
Eingabe-Request muss exakt mit dem Request des inneren, bereits deterministisch
replayten V1-Runs übereinstimmen. Abweichende Eingabe, Payload, Digest,
Manifest oder nichtkanonische Serialisierung werden abgelehnt.

## Verantwortlichkeiten und Nicht-Ziele

| Verantwortlich | Nicht erlaubt |
| --- | --- |
| NanoCell-Mod: begrenzte Byte-/Kontexttechnik, Trace, lokaler Status und Herkunft | Status zu mathematischer Evidenz, Beweis oder Regelentscheidung aufwerten |
| Mod-Katalog: nur aktiv gebaute Manifeste registrieren | dynamische Plugins oder Laufzeit-Suche einführen |
| CLI: äußere Exporte schreiben und beide belegten Formate replayen | einen ausgeschalteten NanoCell-Mod erreichbar halten |
| Core: unverändert bleiben | NanoCell-spezifische `ApplicationTask`-, Session- oder Gate-Semantik erhalten |

Nicht Bestandteil dieses Schritts sind ein neuer `ApplicationTask`,
`ExecutionSession`-Phasen für NanoCell, ein generischer Beobachtungslauf,
LLM-/Cockpit-Anbindung, GPU-Ausführung, zusätzliche Techniken oder Änderungen
am fachlichen Bedeutungsgehalt bestehender NanoCell-Statuswerte.

## Abnahme

Der Schritt ist erst **IMPLEMENTIERT UND BELEGT**, wenn:

1. der NanoCell-Code bei `BRUCE_BUILD_MOD_NANOCELL=OFF` nicht in `bruce_core`
   oder die CLI gelangt und `nanocell` mit `UnknownCommand` abgelehnt wird;
2. ein aktivierter Katalog die sortierten Manifeste von Language- und
   NanoCell-Mod enthält;
3. alle vorhandenen inneren V1-Bundles unverändert replaybar bleiben;
4. neue äußere Exporte Manifest, Eingabe und Payload binden und jede
   kontrolliert neu gehashte semantische Manipulation abweisen;
5. `MATCHED`, `NO_MATCH`, `CONTEXT_REJECTED` und `INCOMPLETE` unverändert
   technisch bleiben;
6. der bestehende Build/Testlauf sowie die vorhandene Dokumentationsprüfung
   grün sind.

## Rückfallstrategie

Der vollständige Mod lässt sich mit `-DBRUCE_BUILD_MOD_NANOCELL=OFF`
ausklammern. Weil der direkte innere V1-Run unverändert bleibt, sind bereits
gespeicherte NanoCell-Bundles weder umzuschreiben noch zu migrieren. Der
einzelne Mod-Commit kann ohne Änderung an Core-Gates und Anwendungstypen
zurückgenommen werden.
