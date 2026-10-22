# Phase-DAG V2 Cockpit Inspector – Design

## Status und Beleggrenze

**IMPLEMENTIERT UND BELEGT:** Der optionale statische
Cockpit-Mod prüft bestehende Phase-DAG V2 NanoCell Bundles lesend. Die finale
Release-Abnahme vom 12. Oktober 2026 ist im folgenden Belegabschnitt erfasst.

**IMPLEMENTIERT UND BELEGT:** Phase-DAG V2 und der feste NanoCell-Adapter
bestehen als C++-API mit begrenzter deterministischer Ausführung, kanonischem
Packen und Replay. Belege:
[Phase-DAG V2 API](../../../include/bruce/phase_dag/phase_dag_v2.hpp),
[V2-Implementierung](../../../src/phase_dag/phase_dag_v2.cpp),
[NanoCell-Adapter](../../../mods/nanocell/src/phase_dag_adapter_v2.cpp) und
[Cockpit-Vertragstests](../../../tests/mods/mcp/mcp_inspector_tests.cpp).
Ein allgemeiner Scheduler und eine breitere Runtime-Orchestrierung bleiben
**DOKUMENTIERT / KONZEPT**.

**DOKUMENTIERT / KONZEPT:** Historische Cockpit-Job/Import/Language/Redis-
Funktionen bleiben nicht implementiert. Das alte HTML-Asset und die nicht
registrierten Tests sind unverändert erhaltene, ungebaute Referenzfragmente.
Der neue Mod besitzt eigene `cockpit_inspector_*`-Dateien und ein aktuelles
CMake-Ziel; die historischen Fragmente sind kein Teil dieses Ziels.

## Ziel

`bruce cockpit [--port 1..65535]` stellt eine ausschließlich lokale, lesende
(read-only) Browseroberfläche für gespeicherte Phase-DAG V2 Bundles bereit. Ein
Nutzer wählt ein Bundle aus; der Cockpit-Mod prüft es durch kanonisches Replay
mit dem statischen NanoCell-V2-Adapterkatalog und zeigt die belastbaren,
serialisierten Ergebnisse an.

Das Cockpit erzeugt keine neue DAG-Ausführung, startet keine Arbeitsprozesse,
verwaltet keine Jobs und nimmt keine dauerhaften Änderungen an Nutzerdaten vor.

## Nicht-Ziele

Folgende historische oder weitergehende Funktionen gehören nicht zu diesem
Paket:

- freie Sprache, Language-Entwürfe oder CORE-Ausführung,
- Job-, Import-, Log- oder Redis-Verwaltung,
- Kommandoausführung oder Übergabe beliebiger Argumente,
- Durchsuchen lokaler Verzeichnisse durch den Server,
- persistente Bundle-Ablage,
- Ausführung neuer Phase-DAG-Spezifikationen,
- externe oder nicht-lokale Netzwerkbindung,
- Adapter-Discovery oder dynamische Plugins,
- generischer Beweiser, GPU oder freie Sprache.

## Architektur

```text
Browser
  │ wählt Bundle aus
  ▼
Cockpit auf 127.0.0.1
  │ begrenzt Request, prüft Header und Replay
  ▼
Phase-DAG-V2-Replay + NanoCell-Adapterkatalog
  │ erzeugt nur ein Anzeige-Modell
  ▼
JSON-Antwort / eingebettete HTML-Oberfläche
```

Die Fachgrenze ist `replay_phase_dag_run_v2(bytes,
phase_dag_nanocell_catalog_v2())`. Ein erfolgreiches Replay ist Voraussetzung
für einen Status `VALID`. Der Inspektor darf die Modellfelder ausschließlich
von der rehydrierten `PhaseDagRunV2`-Instanz ableiten. Replay berechnet intern den deterministischen erwarteten Lauf zur Prüfung
des bestehenden Bundles. Das ist keine Schnittstelle für neue Nutzeraufträge.
Replay/Digest belegt Bundlekonsistenz; ein Digest authentifiziert keine Person
oder Herkunft und erbringt keinen allgemeinen mathematischen Beweis.

## Lokale Schnittstelle

### Prozessschnittstelle

```text
bruce cockpit [--port 1..65535]
```

Ohne `--port` gilt Standardport 8080; zulässig sind ausschließlich dezimale
Ports von 1 bis 65535. Der Dienst bindet
nur an die numerische Loopback-Adresse `127.0.0.1`. Eine Portkollision endet
kontrolliert; es gibt keinen Fallback auf eine andere Adresse oder einen
abweichenden Port.

### HTTP-Schnittstelle

| Methode | Pfad | Zweck |
| --- | --- | --- |
| `GET` | `/` | eingebettete, statische Cockpit-Oberfläche |
| `GET` | `/api/v1/health` | lokaler, nicht fachlicher Dienststatus |
| `POST` | `/api/v1/inspect` | begrenztes V2-Bundle prüfen und Anzeige-Modell liefern |

`POST /api/v1/inspect` akzeptiert nur rohe Bundlebytes bis 1 MiB
(1048576 Bytes), ausschließlich im Request-Speicher, ohne Persistenz. Die
Antwort verwendet ausschließlich `VALID`, `REJECTED` oder `ERROR`. Sie enthält
bei `VALID` DAG-Version/-ID, Nodes und Abhängigkeiten, Receipts samt Adapter,
Digests, Outcome und Fehlercode sowie die Trace-Einträge. Bei `REJECTED` liefert
sie nur `{"status":"REJECTED","code":"REJECTED"}` ohne Bundlebytes,
Dateipfade oder interne Details. `VALID` erhält HTTP 200, fachliches `REJECTED`
422; zu große Requests erhalten 413 vor Replay mit demselben festen
Ablehnungs-JSON. Unerwartete Dienstfehler erhalten HTTP 500 und ausschließlich
`{"status":"ERROR","code":"ERROR"}`. Andere Pfade/Methoden erhalten 404.
Ein nicht verfügbarer angeforderter Port beendet den Dienst kontrolliert mit
Exit ungleich 0, ohne Ersatzadresse oder Ersatzport.

Es gibt weder eine allgemeine Importroute noch Routen für Jobs,
Kommandoausführung, Dateisystemzugriff oder neue DAG-Ausführung.

## Darstellung

Die eingebettete HTML-/JavaScript-Oberfläche besitzt nur eine Dateiauswahl,
eine Prüfschaltfläche und eine Zurücksetzen-Schaltfläche. Sie zeigt fünf
Bereiche:

1. Übersicht: Version, DAG-ID, Knotenzahl, Prüfergebnis.
2. Graph: Knoten und deklarierte Abhängigkeiten.
3. Receipts: Adapterkennung, Requests, Outputs, Evidenz-Digests, Outcome und
   Fehlercode.
4. Trace: validiert, gestartet, bereit, blockiert, fehlgeschlagen, abgebrochen
   und abgeschlossen.
5. Prüfung: Ergebnis oder kontrollierter Ablehnungsgrund.

Bundlewerte werden mit `textContent` oder äquivalentem Text-Rendering angezeigt;
sie dürfen nicht als HTML, JavaScript oder URL interpretiert werden.

## Einbindung

Der Cockpit-Mod wird wie Language und NanoCell als optionaler statischer Mod
konfiguriert. `BRUCE_BUILD_MOD_COCKPIT=ON` (Standard ON) setzt
`BRUCE_BUILD_MOD_NANOCELL=ON` voraus; andernfalls meldet CMake
`CockpitRequiresNanoCell`. Implementierte Struktur:

```text
BRUCE_BUILD_MOD_COCKPIT
└── bruce_mod_cockpit
    ├── Manifest und CLI-Einstieg
    ├── lokaler HTTP-Adapter
    ├── Phase-DAG-V2-Inspektionsservice
    └── eingebettete Oberfläche
```

Der CMake-Schalter steuert Ziel, statischen Mod-Katalog und CLI-Dispatch
konsistent. Ist er aus, gibt es weder Cockpit-Manifest noch `cockpit`-Befehl.
Die alten, nicht baubaren Cockpit-Quellen werden nicht als Implementierung
reaktiviert; nur geeignete UI-Formulierungen oder Testideen dürfen nach
fachlicher Prüfung übernommen werden.

## Sicherheits- und Fehlervertrag

- Bindung ausschließlich an `127.0.0.1`.
- Feste maximale Requestgröße 1 MiB, vor Replay begrenzt; nur Request-Speicher.
- Keine dauerhafte Speicherung der erhaltenen Bytes.
- Keine Ausführung von Requestinhalt.
- Keine Pfadangaben oder Bundlebytes in Fehlerantworten.
- Deterministische Replay-Prüfung gegen den festen NanoCell-Katalog.
- Ungültiges Format, unbekannter Adapter, Digestfehler und nichtkanonische
  Serialisierung sind fachliche Ablehnungen (`REJECTED`).
- Server- oder Ressourcenfehler bleiben von fachlichen Ablehnungen getrennt
  (`ERROR`).

## Akzeptanzkriterien

1. Ein Standard-Build mit aktiviertem Cockpit registriert Manifest, CLI-Befehl
   und einen CTest-Vertrag; ein deaktivierter Build enthält keines davon.
2. Der Dienst akzeptiert ausschließlich Loopback und meldet einen belegten Port
   kontrolliert.
3. Ein kanonisches NanoCell-V2-Bundle ergibt `VALID` und eine vollständige,
   text-sichere Ansicht.
4. Manipulierte, zu große, fremde oder nichtkanonische Daten ergeben
   `REJECTED`, ohne eine Datei zu erzeugen oder zu verändern.
5. Der Cockpit-Code besitzt keine Routen und keine Bibliotheksaufrufe für
   Prozessstarts, Jobs, Importe, Redis oder Dateisystem-Browsing.
6. Bestehende Phase-DAG-V2- und NanoCell-Verträge bleiben unverändert grün.
7. Nutzer- und Architektur-Dokumentation behaupten erst nach erfolgreicher
   Abnahme eine buildbare Cockpit-Funktion.

## Umgesetzte HTTP-Entscheidung

Der lokale HTTP-Adapter verwendet die unveränderte, mit MIT-Lizenz vendorte
Abhängigkeit `cpp-httplib` v0.58.0. Herkunft, Release-Commit und SHA-256 stehen
in [DEPENDENCIES.md](../../../DEPENDENCIES.md) und
[THIRD_PARTY_NOTICES.md](../../../THIRD_PARTY_NOTICES.md). CMake lädt nichts
nach. Der Adapter aktiviert weder Verzeichnisbereitstellung noch Prozess- oder
Dateisystemnavigation, externe Bindung, CORS, Adapter-Discovery oder generische
DAG-Ausführung.

Belege: [Mod/CLI](../../../mods/mcp/src/mod.cpp),
[Replay-Service](../../../mods/mcp/src/mcp_inspector.cpp),
[Loopback-HTTP](../../../mods/mcp/src/mcp_inspector_server.cpp),
[textsichere Oberfläche](../../../mods/mcp/src/mcp_inspector_assets.cpp)
und [HTTP-/CLI-/Dokumentationsprüfungen](../../../tests/mods/mcp/test_mcp_inspector.py).

## Finale Release-Abnahme — 12. Oktober 2026

**IMPLEMENTIERT UND BELEGT:** Eine frische vollständige native Quellkopie unter
`/tmp/bruce-cockpit-final-source` wurde mit `CMAKE_BUILD_TYPE=Release` gebaut.
Die vollständige vorgeschriebene CTest-Suite bestand **153/153** Tests,
**0 Fehler, 0 übersprungene CTest-Registrierungen**, Exit 0 (107,08 Sekunden).
Nach Aktualisierung des Quellen-Prüfsummenindex bestätigte der abschließende
parallele Lauf ebenfalls **153/153**, Exit 0 (25,49 Sekunden).

Befehle aus der vollständigen temporären Quellkopie:

```sh
scripts/with-host-deps.sh cmake -G Ninja -S . -B /tmp/bruce-cockpit-final -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
scripts/with-host-deps.sh cmake --build /tmp/bruce-cockpit-final -j4
scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-cockpit-final --output-on-failure
scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-cockpit-final --output-on-failure -j4
python3 docs/cli/verify_examples.py /tmp/bruce-cockpit-final/bruce
python3 scripts/check_local_doc_links.py
```

Der CLI-Verifizierer unterstützt einen Programmpfad als Positionsargument,
kein `--check`. Er erzeugte ausschließlich in der vollständigen temporären
Kopie **152 Beispiele, 85 dokumentierte Optionen und 686 Diagnoseeinträge**;
die drei generierten Artefakte wurden gemeinsam als Verifizierer-Ergebnis
übernommen. Cockpit-Beispiele prüfen Parserablehnungen, die HTTP-/Replay-Grenze
wird durch die separate Cockpit-Vertragssuite geprüft. Der lokale Linkprüfer
verwendet die etablierten CI-Dokumentationseingaben; er prüft Dateiziele,
keine externen URLs oder Fragmentanker.

Die [HTTP-/CLI-/Dokumentationsprüfung](../../../tests/mods/mcp/test_mcp_inspector.py)
und [C++-Verträge](../../../tests/mods/mcp/mcp_inspector_tests.cpp) gehören
zur bestehenden CTest-Registrierung. Diese Abnahme belegt den lokalen lesenden
Inspector; sie behauptet weder eine allgemeine mathematische Beweisfunktion
noch eine manuelle Browser-Abnahme oder einen allgemeinen Scheduler.
