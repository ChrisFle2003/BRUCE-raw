# Umbenennung des Cockpit-Inspektors zu MCP

**Status:** VORSCHLAG — zur Freigabe vor der Umsetzung\
**Datum:** 2026-10-18

## Ziel

Der bisher als **Cockpit** bezeichnete lokale, browserbasierte
Inspektor-Modus wird im gesamten aktuellen Produktbestand als **MCP**
bezeichnet. Seine Funktion bleibt unverändert: Er startet weiterhin einen
nur lokal erreichbaren HTTP-Server für die Untersuchung vorhandener
BRUCE-Läufe. Der bisherige CLI-Befehl

```text
bruce cockpit [--port 1..65535]
```

wird durch

```text
bruce mcp [--port 1..65535]
```

ersetzt.

## Nicht-Ziele

- Der HTTP-Inspektor wird **nicht** zu einem JSON-RPC- oder Stdio-MCP-Server
  umgebaut.
- Die übrigen BRUCE-CLI-Befehle und ihre Optionen bleiben unverändert.
- Die Funktionalität, Routen, Antworten, Bindung an `127.0.0.1`,
  Portvalidierung und der schreibgeschützte Charakter des Inspektors bleiben
  unverändert.
- Die vorhandene Stdio-Anbindung `tools/bruce_mcp.py` wird nicht funktional
  verändert.
- Historische Dokumente, archivierte Läufe unter `runs/` und externe
  Abhängigkeitsnachweise werden nicht nachträglich umgeschrieben. Sie bleiben
  als zeitgenössische Belege erhalten.
- Die nicht eingebundenen historischen Fragmente `mods/cockpit/src/cockpit_assets.cpp`,
  `tests/mods/cockpit/cockpit_tests.cpp`, `tests/mods/cockpit/test_cockpit.py`
  und `tests/mods/cockpit/test_language_management.py` bleiben unverändert.
  Sie gehören weder zum aktuellen CMake-Target noch zur registrierten
  Testsuite und beschreiben eine frühere, nicht vorhandene Verwaltungsoberfläche.

## Begriffe und Abgrenzung

Nach der Umbenennung gibt es zwei technisch unterschiedliche Dinge mit
„MCP“ im Namen:

| Bezeichnung | Ort | Funktion |
|---|---|---|
| MCP-Inspektor-Modul | bisher `mods/cockpit/` | Lokale Browser-/HTTP-Oberfläche für vorhandene Läufe |
| BRUCE-Stdio-MCP-Adapter | `tools/bruce_mcp.py` | JSON-RPC über Standard-Ein-/Ausgabe für MCP-Clients |

Die Namen werden in der aktuellen Dokumentation ausdrücklich unterschieden.
Der Adapter behält seinen bestehenden Dateinamen, damit weder sein
Protokollvertrag noch Aufrufer unbeabsichtigt geändert werden.

## Öffentliche und interne Umbenennungen

| Bisher | Neu | Auswirkung |
|---|---|---|
| `bruce cockpit` | `bruce mcp` | Bewusste, inkompatible CLI-Umbenennung |
| `BRUCE_BUILD_MOD_COCKPIT` | `BRUCE_BUILD_MOD_MCP` | CMake-Konfigurationsschnittstelle wird umbenannt |
| `BRUCE_BUILD_MOD_COCKPIT` (C++-Definition) | `BRUCE_BUILD_MOD_MCP` | Bedingte Kompilierung bleibt erhalten |
| `bruce_mod_cockpit` | `bruce_mod_mcp` | Internes CMake-Target wird umbenannt |
| `bruce::cockpit_*` | `bruce::mcp_*` | Öffentliche C++-Symbolnamen des Moduls werden umbenannt |
| `include/bruce/mods/cockpit/` | `include/bruce/mods/mcp/` | Öffentlicher Include-Pfad wird umbenannt |
| aktive Dateien in `mods/cockpit/` | `mods/mcp/` | Der aktuelle Inspector wird in ein neues Implementierungsverzeichnis überführt |
| aktive Inspector-Tests in `tests/mods/cockpit/` | `tests/mods/mcp/` | Die registrierten C++- und Python-Tests werden überführt |
| aktive `cockpit_inspector*`-Quelldateien | `mcp_inspector*`-Quelldateien | Die aktiven Dateinamen folgen der neuen Modulbezeichnung |

Es wird keine Kompatibilitätsalias-Schicht für `bruce cockpit`, alte
CMake-Optionen oder alte C++-Symbole eingeführt. Das entspricht der
gewünschten vollständigen Umbenennung; bestehende Aufrufer müssen auf `mcp`
umgestellt werden.

Der MCP-Inspektor bleibt an NanoCell gebunden:
`BRUCE_BUILD_MOD_MCP=ON` erfordert `BRUCE_BUILD_MOD_NANOCELL=ON`.
Die umbenannte Konfigurationsablehnung lautet `McpRequiresNanoCell`.

## Unveränderter Ablauf

```text
bruce mcp [--port PORT]
        |
        v
MCP-CLI-Einstiegspunkt
        |
        v
Lokaler HTTP-Server auf 127.0.0.1:PORT
        |
        +--> statische Inspektor-Oberfläche
        +--> Health-Endpunkt
        +--> schreibgeschützte Inspektions- und Replay-Abfragen
```

Die derzeitigen Endpunkte und ihre Inhalte bleiben bestehen. Die Umbenennung
betrifft den Modul-, Datei-, Symbol- und Nutzernamen, nicht das HTTP-Protokoll.
Der Standardport bleibt 8080; zulässig sind dezimale Portwerte von 1 bis
65535. `POST /api/v1/inspect` behält die 1 MiB-Grenze, während
`POST /api/v3/inspect` und `POST /api/dag-scheduler-v4/inspect` bei 5 MiB
bleiben.

## Betroffene Bereiche

1. **Build und Katalog:** `CMakeLists.txt`,
   `mods/cockpit/CMakeLists.txt` sowie bedingte Modulregistrierungen.
2. **C++-Implementierung:** öffentlicher Header, CLI-Dispatch, Inspector,
   Server und eingebettete Assets.
3. **Tests:** C++- und Python-Tests einschließlich CTest-Namen und aller
   Testpfade.
4. **Aktuelle Dokumentation:** README, Architektur-, Implementierungs- und
   CLI-Handbücher sowie aktuelle Beispiele.
5. **Generierte CLI-Dokumentation:** ausschließlich über
   `docs/cli/verify_examples.py` in einer temporären Kopie aktualisieren;
   keine manuelle Bearbeitung der erzeugten Dateien.

Nicht umbenannt werden Versionsgeschichte, archivierte Untersuchungsstände,
historische Roadmaps, referenzierte Fremdkomponenten und die oben genannten,
nicht eingebundenen historischen Fragmente. Falls sie den alten Namen
enthalten, sind sie als historische Belege einzuordnen.

## Fehlerverhalten

Das bestehende Fehlerverhalten wird beibehalten:

- ungültige Ports werden vor dem Serverstart abgewiesen;
- die bestehende Bindung auf `127.0.0.1` bleibt bestehen;
- nicht unterstützte HTTP-Anfragen und Inspektionsfehler behalten ihre
  vorhandenen Statuscodes und Antwortformate;
- beim deaktivierten Modul erscheint der neue Befehl nicht in der CLI-Hilfe
  und ist nicht dispatchbar.

## Verifikation nach der Umsetzung

1. Suche nach verbliebenen produktiven `cockpit`-Referenzen in Quellcode,
   Build-Dateien, aktuellen Tests und aktueller Dokumentation. Ausnahmen
   werden auf historische oder Drittanbieterbelege begrenzt und begründet.
2. Konfiguration, Build und CTest im vorgegebenen temporären, pfadsicheren
   Verzeichnis nach dem Projekthandbuch ausführen.
3. Die MCP-Inspektor-Tests und die relevanten CLI-Tests ausführen.
4. Prüfen, dass `bruce mcp --help` beziehungsweise die globale CLI-Hilfe den
   neuen Befehl zeigt und `bruce cockpit` nicht mehr akzeptiert.
5. Die erzeugte CLI-Dokumentation nur über den vorgesehenen Generator prüfen.
6. Markdown-Links sowie `git diff --check` prüfen, soweit eine funktionsfähige
   Git-Metadatenbasis vorhanden ist.

## Build-Bereinigung

Die ursprüngliche Bitte umfasst das Entfernen bisheriger Build-Artefakte und
einen Neuaufbau. Das ist eine separate, destruktive Maßnahme. Der konkrete
Produkt-Buildpfad ist `build/`; er wird entfernt und frisch angelegt. Der
lokale Host-Abhängigkeitsbestand `build-host-deps/` bleibt erhalten, weil
`scripts/with-host-deps.sh` ihn für jeden unterstützten CMake-/CTest-Lauf
voraussetzt und er kein BRUCE-Kompilatbestand ist. Die von dieser Analyse
erzeugten temporären Verzeichnisse unter `/tmp/` werden ebenfalls entfernt.
Kein Quell-, Dokumentations- oder Laufdatenbestand wird dabei entfernt.

## Entscheidungspunkt

Mit dieser Spezifikation wird die bisherige lokale Weboberfläche als
**MCP-Inspektor** weitergeführt. Die bestehende Stdio-MCP-Anbindung bleibt ein
getrenntes Werkzeug. Nach Freigabe folgt ein detaillierter Umsetzungsplan;
erst danach werden Dateien umbenannt oder Build-Artefakte bereinigt.
