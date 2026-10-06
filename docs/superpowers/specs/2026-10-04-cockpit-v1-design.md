# Cockpit-Mod V1 – Architekturdesign

**Status:** DOKUMENTIERT / KONZEPT. In diesem Repository existieren aktuell
weder ein Cockpit-Mod, ein HTTP-Server noch eine Browser-Oberfläche. Die
vorhandene CLI, die statische Mod-Runtime, die Language- und NanoCell-Mods
sowie die bestehenden Bundle- und Replaywege sind die Grundlage, nicht Teil
dieser neuen Implementierung.

**Ziel:** Cockpit V1 ist eine austauschbare, ausschließlich lokale
Browser-Oberfläche für vorhandene BRUCE-Forschungs- und Mod-Läufe. Sie macht
Jobs, Konsolenausgabe, Bundles, Replays und Gates sichtbar und startet nur
bestehende `bruce`-Befehle als isolierte Kindprozesse. Sie führt keine
Mathematik, Gates, Bundlecodecs oder Modlogik selbst aus.

Die [konsolidierte Architektur](../../architecture/03-blueprint-consolidation.md)
und das [Core-Mod-System](2026-10-03-core-mod-architecture-design.md) bleiben
für die Grenzen von Core und Mods maßgeblich. Dieses Dokument ist eine
verbindliche Entwurfsgrenze, keine Implementierungsbehauptung.

## Festgelegte Entscheidungen

1. Cockpit ist ein optionaler, statisch gebauter Mod; es gibt kein dynamisches
   Plugin, keine externe Cockpit-App und keine zweite fachliche Laufzeit.
2. Der Dienst bindet ausschließlich an die IPv4-Loopback-Adresse `127.0.0.1`.
   Er veröffentlicht weder einen LAN- noch einen Internetdienst, verwendet
   keine externe API und startet keinen Browser selbst.
3. `bruce cockpit` gibt nach erfolgreichem Start die vollständige lokale URL
   aus. Ohne `--port` wählt das Betriebssystem einen freien Loopback-Port; ein
   optionaler `--port` fordert einen festen lokalen Port. Bindefehler führen
   zum Fehler, nie zu einem Fallback auf eine andere Adresse.
4. Neue Cockpit-Artefakte entstehen ausschließlich unter `runs/cockpit/`.
   Bereits vorhandene `.bruce`-Dateien dürfen nur nach ausdrücklicher Auswahl
   lesend geöffnet werden. Cockpit ändert, verschiebt oder löscht sie nicht.
5. Cockpit startet den bestehenden `bruce`-Prozess als Kindprozess mit einer
   festen Argumentliste; es ruft keine private Core-Implementierung direkt
   auf. Der auslösende `bruce`-Pfad wird fest gebunden, nicht über `PATH`
   gesucht.
6. Die Oberfläche bietet geführte Masken und einen Expertenmodus. Beide zeigen
   den vollständigen, tatsächlich gestarteten BRUCE-Aufruf vor einer expliziten
   Benutzerbestätigung. Der Expertenmodus akzeptiert nur BRUCE-Argumente,
   keine Shell-Syntax und keine beliebigen Programme.
7. Die Parallelitätsgrenze ist standardmäßig `1`. Eine lokale Person erhöht
   sie vor einem weiteren Start bewusst; jeder Job bleibt ein separater
   Kindprozess in einem eigenen Jobordner. Cockpit macht aus dem sequenziellen
   Core keinen Scheduler und verspricht keine gemeinsame Parallelsemantik.
8. Replay und Gate starten nur nach einem sichtbaren Klick oder durch die vor
   dem Lauf ausdrücklich gewählte vorhandene Option `--verification replay`.
   Cockpit führt keine verdeckten Anschlussläufe aus.

## Bestand und Grenzen

| Baustein | Einordnung | Bedeutung für Cockpit V1 |
| --- | --- | --- |
| `bruce`-CLI sowie Core-Bundles, Replay und Gates | IMPLEMENTIERT, ABER NICHT AKTUELL VERIFIZIERT | Einzige fachliche Ausführungs- und Bewertungsquelle. |
| Statische Language- und NanoCell-Mods | IMPLEMENTIERT, ABER NICHT AKTUELL VERIFIZIERT | Aktiv gebaute Befehle können über Cockpit gestartet werden. |
| `runs/` in `.gitignore` | IMPLEMENTIERT UND BELEGT | Lokaler, nicht versionierter Zielraum für Cockpit-Jobs. |
| Cockpit-Mod, Webtransport und Browser-Assets | DOKUMENTIERT / KONZEPT | Gegenstand eines späteren, separat freizugebenden Plans. |
| Freie Sprachschnittstelle über die bestehenden begrenzten Language-Befehle hinaus | DOKUMENTIERT / KONZEPT | Nicht Teil von Cockpit V1. |
| NanoCell als `ApplicationTask` oder Phase-DAG-Ausführung | DOKUMENTIERT / KONZEPT | Cockpit bildet keinen Ersatz für diese separaten Pakete. |

Ein Browser-Text, eine Fortschrittsanzeige oder ein lokaler NanoCell-Befund
wertet kein Ergebnis auf. Fachliche Aussagen kommen weiterhin ausschließlich
aus dem ausgeführten Core-/Mod-Befehl, dessen Bundle, Replay und bestehenden
Gates.

## Zielstruktur

```text
include/bruce/mods/cockpit/
  mod.hpp                         # öffentliches Cockpit-Manifest und CLI-Einstieg
mods/cockpit/
  CMakeLists.txt                  # statischer Target bruce_mod_cockpit
  src/mod.cpp                     # Manifest und CLI-Einstieg
  src/server.cpp                  # ausschließlich lokaler HTTP-Transport
  src/jobs.cpp                    # Jobordner, Warteschlange und Kindprozessgrenze
  assets/                         # statische HTML-, CSS- und JavaScript-Oberfläche
tests/mods/cockpit/
  cockpit_tests.cpp               # Job-, Pfad-, Argument- und Statusverträge
  test_cockpit.py                 # Loopback-, CLI- und Browser-Integrationsfälle
```

Der Baum ist ein Zielbaum. Keine vorhandene Core- oder CLI-Datei wird nur aus
optischen Gründen verschoben. `BRUCE_BUILD_MOD_COCKPIT` und der Target
`bruce_mod_cockpit` entstehen erst zusammen mit dieser Implementierung; bei
deaktiviertem Mod bleibt `cockpit` wie jeder andere nicht gebaute Befehl
unerreichbar.

## Lokaler Prozess- und Browservertrag

```text
Terminal
  └─ bruce cockpit [--port N]
       └─ 127.0.0.1:PORT  ─── statische Browser-Oberfläche
            ├─ Job anlegen / Eingabe prüfen / Aufruf anzeigen
            ├─ bruce <bestehender Befehl> ...  (Kindprozess je Job)
            └─ Jobprotokoll, Ausgabe, Bundle-Verweis, explizites Replay/Gate
```

Der Cockpit-Prozess und sein Browser-Frontend dürfen ausschließlich über einen
kleinen lokalen Transport miteinander sprechen. Die konkrete HTTP-Bibliothek
ist bewusst noch nicht ausgewählt: Vor einer Implementierung wird eine
gepflegte, permissiv lizenzierte Einbettung mit fest gepinnter Version,
Lizenztext und CMake-Integration gegen den bestehenden Build geprüft. Der in
der Vorprüfung betrachtete CivetWeb ist nur ein Kandidat, keine eingeführte
Abhängigkeit.

Der Browser kann sich während der Laufzeit des Cockpit-Prozesses neu verbinden
und erhält dann den gespeicherten Jobzustand sowie die bisherige Ausgabe. Ein
Neustart des Cockpit-Prozesses verspricht dagegen kein transparentes
Wiederanbinden an einen zuvor laufenden Kindprozess: abgeschlossene Jobs werden
aus ihren lokalen Daten wieder sichtbar; ein beim ungeplanten Dienstende noch
als laufend markierter Job wird als `UNRESOLVED_AFTER_SERVER_EXIT` angezeigt.
Das vermeidet eine unbelegte Wiederaufnahmebehauptung.

## Job- und Dateivertrag

Jeder bestätigte Start erhält einen neuen, nicht wiederverwendeten Ordner:

```text
runs/cockpit/<job-id>/
  job.json                        # bruce-cockpit-job-v1, Anfrage und Status
  command.argv.json               # tokenisierte, bestätigte Argumentliste
  stdout.log                      # unveränderte Standardausgabe des Kindes
  stderr.log                      # unveränderte Fehlerausgabe des Kindes
  artifacts/                      # ausschließlich vom Cockpit erzeugte Ausgaben
```

`job.json` trennt mindestens `QUEUED`, `RUNNING`, `SUCCEEDED`, `FAILED`,
`CANCEL_REQUESTED`, `CANCELLED` und `UNRESOLVED_AFTER_SERVER_EXIT`. Der
Rückkehrcode, Start-/Endzeit, ausführbare Datei, Mod-Katalog und Verweise auf
lokale Artefakte werden protokolliert. Diese Metadaten sind Bedienprovenienz,
kein neues wissenschaftliches Bundle und keine fachliche Evidenz.

Ausgabeoptionen wie `--output` werden vom Cockpit in `artifacts/` erzeugt;
Schreibziele außerhalb des jeweiligen Jobordners werden abgelehnt. Explizit
gewählte Eingaben dürfen außerhalb von `runs/cockpit/` liegen, werden aber nur
gelesen. Befehle, die bestehende Regel- oder Experience-Speicher außerhalb des
Jobordners verändern würden, gehören nicht zu Cockpit V1. Der Expertenmodus
unterliegt derselben Pfadregel und zerlegt niemals Text durch eine Shell.

Das Öffnen einer externen `.bruce`-Datei erzeugt höchstens einen neuen lokalen
Ansichtsjob mit einem Pfadverweis und Ergebnislogs. Die Quelldatei wird weder
kopiert noch verändert. `inspect-run`, `assess-run`, `replay` und `gate-run`
bleiben dabei eigenständige, sichtbar ausgelöste bestehende CLI-Aufrufe.

## Bedien- und Ausführungsgrenze

Geführte Masken decken die vorhandenen Forschungs- und aktiv gebauten
Mod-Laufwege ab. Der Expertenmodus schafft den vollständigen Zugang zu
zulässigen vorhandenen BRUCE-Laufargumenten, ohne dass Cockpit eine eigene
Semantik für deren Optionen erfindet. Administrative Befehle sowie Befehle mit
externen veränderbaren Speichern bleiben außerhalb des V1-Katalogs.

Vor dem Start validiert Cockpit nur seine eigene Bediengrenze:

- erster Prozessname ist der fest gebundene `bruce`-Pfad;
- die Argumentliste enthält keine NUL-Bytes und wird direkt als Argumentvektor
  übergeben;
- der Befehl gehört zum aktiv gebauten Cockpit-Katalog;
- alle Cockpit-erzeugten Schreibpfade liegen kanonisch im Jobordner;
- die Benutzerperson bestätigt die endgültige Argumentliste sichtbar.

Der gestartete Prozess validiert anschließend wie bisher alle fachlichen
Argumente, Budgets, Bundleformate, Replays und Gates. Cockpit zeigt rohe
Standard-/Fehlerausgabe stets zugänglich an. Eine grafische Zusammenfassung
darf nur bereits vorhandene strukturierte Felder abbilden und muss den
zugrunde liegenden Aufruf und die Rohdaten verlinken.

Ein Abbruch ist ebenfalls eine sichtbare Benutzeraktion. Cockpit sendet dem
betroffenen Kindprozess die vorhandene lokale Unterbrechung, wartet auf dessen
Rückkehr und protokolliert Ergebnis oder Fehler. Es wandelt weder einen
Abbruch noch einen technischen Fehler in einen fachlichen Befund um.

## Bau-, Manifest- und CLI-Grenze

Die spätere Implementierung ergänzt einen CMake-Schalter
`BRUCE_BUILD_MOD_COCKPIT` und den statischen Target `bruce_mod_cockpit` nach
dem vorhandenen Language-/NanoCell-Muster. Der feste Mod-Katalog erhält ein
Cockpit-Manifest nur bei aktiviertem Target. Der CLI-Einstieg lautet dann:

```text
bruce cockpit [--port N]
```

Der Mod darf `bruce_mod_runtime` verwenden, aber keine privaten Core-Header
oder umgekehrte Abhängigkeit vom Core erzwingen. Er ruft für Jobs die
ausführbare CLI auf, nicht `execute_mod_core_run` oder einzelne
Core-Implementierungen. Damit bleiben CLI, Core, Mod-Runtime und Cockpit klar
voneinander getrennt.

## Abnahme

Cockpit V1 ist erst **IMPLEMENTIERT UND BELEGT**, wenn mindestens Folgendes
durch vorhandene automatische Prüfungen nachgewiesen ist:

1. Mit `BRUCE_BUILD_MOD_COCKPIT=OFF` existieren weder Cockpit-Target,
   Manifest noch CLI-Befehl; bestehende Core-, Language- und NanoCell-Tests
   laufen unverändert.
2. Mit aktiviertem Mod bindet der Dienst ausschließlich an `127.0.0.1`, gibt
   die tatsächlich erreichbare lokale URL aus und öffnet keinen Browser.
3. Ein geführter und ein Experten-Job starten ausschließlich den gebundenen
   `bruce`-Prozess mit der vorab angezeigten Argumentliste; Shell-Eingaben,
   fremde Programme und Schreibpfade außerhalb des Jobordners werden
   abgelehnt.
4. Ein Lauf protokolliert Ausgabe, Rückkehrcode und lokale Artefaktverweise;
   der Browser kann sich während desselben Serverlaufs neu verbinden.
5. Abbruch, Fehlerrückkehr, Warteschlange und eine bewusst erhöhte
   Parallelitätsgrenze sind mit deterministischen Testkindern belegt.
6. Replay und Gate erfolgen nur nach sichtbarer Benutzeraktion oder einer
   vorher gewählten bestehenden `--verification replay`-Option.
7. Eine extern gewählte `.bruce`-Datei wird in Inspect-, Assess-, Replay- und
   Gatefällen nicht verändert; lokale Markdown-Links, Build, Tests und die
   bestehende CLI-Dokumentationsprüfung bleiben grün.

## Bewusst nicht enthalten

- LAN-/Internetbetrieb, Konten, Authentisierung, TLS, Telemetrie oder externe
  Datendienste;
- automatisches Browser-Öffnen, automatische Replay-/Gate-Läufe oder autonome
  Auftragsplanung;
- eine allgemeine freie Sprach- oder LLM-Schnittstelle;
- eine Umdeutung von Bundle-, Replay-, Gate- oder NanoCell-Status;
- automatische Änderung externer Regel- oder Experience-Speicher;
- Core-Scheduler, GPU-, Threadpool- oder verteilte Ausführung;
- eine Zusage, dass ein nach Prozessabbruch noch laufender Job fortgesetzt
  oder transparent wiederverbunden werden kann.

## Offene Entscheidung vor dem Umsetzungsplan

Für den lokalen HTTP-Transport ist vor der Umsetzung genau eine gepinnte,
permissiv lizenzierte C/C++-Abhängigkeit oder eine bereits vorhandene
Projektkomponente auszuwählen. Die Auswahl muss CMake-, Lizenz-, Offline- und
Deaktivierungstests enthalten. Sie darf weder eine unpinned Netzwerkabfrage
beim Build noch ein neues Laufzeit-Pluginmodell einführen.

Nach dieser Entscheidung folgt ein kleiner, rücknehmbarer Implementierungsplan:
zuerst Mod-/CMake-Deaktivierung, dann Loopback-Start und statische Ansicht,
danach Jobprotokoll und einziger Kindprozess, zuletzt kontrollierte Parallelität,
Import, Replay/Gate und die vollständigen Integrationsprüfungen. Kein Schritt
ändert vorhandene Bundleformate oder Core-Contracts.
