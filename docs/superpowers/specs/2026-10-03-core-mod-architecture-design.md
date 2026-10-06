# BRUCE Core-Mod-System V1 – Architekturdesign

**Status:** TEILWEISE IMPLEMENTIERT UND BELEGT. Die statische Mod-Runtime, der feste Katalog sowie die optionalen Language- und NanoCell-Mods sind nach ihren CMake-, Replay- und Deaktivierungstests implementiert. Die übrigen beschriebenen Mods und weitergehenden Ziele bleiben DOKUMENTIERT / KONZEPT.

**Entscheidung:** BRUCE erhält in V1 mit CMake gebaute, optional aktivierbare Mods. Es gibt weder dynamisches Laden fremder `.so`-Dateien noch eine Laufzeit-Plugin-Suche. Die vier vorgesehenen Mods sind freie Sprache, NanoCell, Cockpit und Phase-DAG.

## Zweck und Erfolgskriterium

Der bewährte mathematische Kern bleibt klein, nachvollziehbar und vollständig replaybar. Erweiterungen sollen wie Minecraft-Mods getrennt gebaut, getestet, dokumentiert und bei Bedarf nicht eingebaut werden können. Ein fehlerhafter oder ausgeschalteter Mod darf keine Core-Invariante, keinen vorhandenen CLI-Lauf und keinen gespeicherten Replayvertrag verändern.

Der Erfolg von V1 ist keine Anzahl von Mods, sondern diese nachweisbare Eigenschaft: Ein Core-Lauf erzielt mit allen ausgeschalteten Mods exakt seine bisherige fachliche Ausgabe; ein aktivierter Mod arbeitet nur über eine versionierte Core-Schnittstelle und hinterlässt einen nachvollziehbaren Herkunftseintrag.

## Bestand und Ziel sauber getrennt

| Baustein | Status am 04.10.2026 | Einordnung |
| --- | --- | --- |
| `bruce_core` und die bestehende CLI | IMPLEMENTIERT UND BELEGT | Der Core enthält Mathematik und Anwendungsschicht; er kennt keine konkrete Mod und verlinkt keine NanoCell-Implementierung. |
| Begrenzter Sprachpfad | IMPLEMENTIERT UND BELEGT | Optionale statische Language-Mod mit zwei festen Satzmustern, Umschlag-Export und direktem V1-Replay. |
| NanoCell V1 | IMPLEMENTIERT UND BELEGT | Optionale statische Mod mit Acht-Byte-Kern, äußerem Herkunftsumschlag für neue Exporte und direktem V1-Read-only-Replay; kein allgemeiner Auftrag. |
| Mod-API, Mod-Katalog und Mod-CMake-Ziele | IMPLEMENTIERT UND BELEGT | `bruce_mod_runtime`, fester CMake-Katalog sowie `BRUCE_BUILD_MOD_LANGUAGE` und `BRUCE_BUILD_MOD_NANOCELL`; keine dynamischen Plugins. |
| Cockpit | DOKUMENTIERT / KONZEPT | Es existiert kein Cockpit in diesem Repository. |
| Phase-DAG | DOKUMENTIERT / KONZEPT | Ein begrenzter V1-Entwurf existiert, aber keine Ausführung. |

Die [konsolidierte Architektur](../../architecture/03-blueprint-consolidation.md) bleibt für den tatsächlichen Core-Stand maßgeblich. Diese Datei ergänzt ihr Zielbild; sie ersetzt keine Implementierungs- oder Testnachweise.

## Zielstruktur

```text
include/bruce/
  core/                         # unveränderter fachlicher Kern
  mod/                          # kleine, stabile Mod-API V1
  mods/
    language/                   # öffentliche Sprachmod-Typen
    nanocell/                   # öffentliche NanoCell-Mod-Typen
    cockpit/                    # öffentliche Cockpit-Anfragen/-Ansichten
    phase_dag/                  # öffentliche Phase-DAG-Mod-Typen
src/
  core/                         # bestehende Core-Implementierungen
  mod/                          # Katalog und Core-Adapter
mods/
  CMakeLists.txt                # registriert nur eingebaute Mods
  language/{src,tests,docs}/
  nanocell/{src,tests,docs}/
  cockpit/{src,tests,docs}/
  phase_dag/{src,tests,docs}/
apps/
  bruce_cli.cpp                 # Core- und Mod-Einstieg
tests/
  mods/{language,nanocell,cockpit,phase_dag}/
```

Die Struktur ist ein Zielbaum. Bestehende Dateien werden nicht allein aus optischen Gründen verschoben. Eine Quelle wechselt erst beim jeweiligen Mod-Schritt zusammen mit ihren Includes, CMake-Referenzen, Tests und Dokumentationslinks.

## Abhängigkeitsrichtung

```text
Apps (CLI, später Cockpit)
        ↓
Mod-Runtime: Katalog, Manifestprüfung, Core-Adapter
        ↓                         ↑
Mod-API V1  ←──────────────  eingebaute Mods
        ↓
BRUCE Core: Mathematik, Evidenz, Replay, Gates, Budgets, Regelhoheit
```

`bruce_core` kennt keinen konkreten Mod. Die Mod-Runtime kennt den Core und die API. Ein Mod darf nur die öffentliche Mod-API verwenden und darf nie Privatheader des Cores, interne Dateipfade oder CMake-Interna voraussetzen. Der Build verknüpft keine Mod-Abhängigkeit zurück in den Core.

## Vertrag der Mod-API V1

Die API ist absichtlich klein und versionsgebunden:

1. **Manifest:** Jeder Mod benennt eine eindeutige ID, Mod-Version, benötigte `mod-api-v1`-Version, Fähigkeiten und seinen Einstieg.
2. **Anfrage:** Ein Mod übergibt entweder eine bestehende Core-Auftragsart samt Eingabeherkunft und expliziten Budgets oder einen eigenen begrenzten technischen Vertrag wie NanoCell. Neue fachliche Auftragstypen ändern nicht still die API, sondern benötigen einen eigenen Core-Vertrag.
3. **Ausführung:** Ausschließlich der Core-Adapter startet Core-Sitzungen, Core-Bundles, deren Replay und Gates. Ein technischer Mod kann einen eigenen kanonischen Run/Replayer besitzen, darf daraus aber keinen Core-Status, kein Gate und keine Regelentscheidung setzen.
4. **Antwort:** Für Core-Aufträge liefert der Core einen versionierten Laufbericht mit Bundle, Replay-/Gatebefund und klarer fachlicher Bedeutung zurück. Ein technischer Mod liefert nur seinen eigenen klar abgegrenzten technischen Befund; weder darf er den Core-Befund umdeuten noch einen solchen vortäuschen.
5. **Herkunft:** Jeder durch einen Mod veranlasste Lauf enthält Mod-ID, Mod-Version und kanonische Mod-Eingabe. Das bindet Herkunft, ist aber keine Identitäts- oder Berechtigungsbehauptung.

`VERIFIED_ON_SAMPLES`, ein lokaler NanoCell-Treffer, eine Sprachantwort und eine graphische Cockpit-Anzeige bleiben unterschiedliche Aussagen. Nur die bestehenden Core-Gates bestimmen den fachlichen Status. Der [Regelhoheitsvertrag](../../contracts/05-human-rule-approval.md) bleibt unverändert: Ein Mod darf keine Regeln aktivieren oder eine menschliche Bestätigung ersetzen.

## Bau- und Aktivierungsmodell

Die Top-Level-CMake-Konfiguration besitzt einen Schalter pro tatsächlich implementiertem Mod: `BRUCE_BUILD_MOD_LANGUAGE` und `BRUCE_BUILD_MOD_NANOCELL`. Ein ausgeschalteter Mod wird weder kompiliert noch registriert; ein aktivierter Mod wird als eigener statischer CMake-Target gebaut und beim Build in den festen Katalog aufgenommen. `BRUCE_BUILD_MOD_COCKPIT` und `BRUCE_BUILD_MOD_PHASE_DAG` entstehen erst mit dem jeweiligen eigenen Target.

V1 liest keine Mod-Binärdateien aus einem Ordner und lädt keine fremden Programme nach. Damit gibt es keine unbelegte ABI-, Signatur-, Sandbox- oder Versionskompatibilitätsbehauptung. Ein späteres dynamisches Nachladen ist ein eigenes Architekturpaket und keine Abkürzung in diesem Entwurf.

## Verantwortlichkeiten der vier Mods

| Mod | Darf | Darf nicht |
| --- | --- | --- |
| Freie Sprache | Natürlichsprachliche Eingaben in überprüfbare Vorschläge überführen und Core-Befunde erklären | Wahrheitsstatus vergeben, Gates umgehen oder Budgets ändern |
| NanoCell | Begrenzte Byte-/Kontexttechnik ausführen und ihren eigenen Run/Trace replaybar exportieren | Aus `MATCHED` einen Beweis, eine Core-Bewertung oder eine Regelentscheidung ableiten |
| Cockpit | Läufe über dieselbe Mod-Runtime starten, Fortschritt, Bundles und Befunde sichtbar machen | eine zweite fachliche Ausführungslogik oder verdeckte automatische Läufe besitzen |
| Phase-DAG | Begrenzte, versionierte Phasen und Provenienztraces berechnen und replaybar exportieren | ungebundene Zyklen, GPU-Ausführung oder Graphbehauptungen ohne eigenen Vertrag einführen |

## Umsetzungsreihenfolge

1. **Mod-Grenze:** `mod-api-v1`, fester Katalog, CMake-Schalter und ein Test, dass ein ausgeschalteter leerer Katalog den bisherigen Core nicht verändert.
2. **Language-Mod:** Den vorhandenen begrenzten Sprachpfad als erste echte Mod hinter den Adapter legen. Bestehende CLI- und Replaytests müssen unverändert weiterlaufen; der neue Mod-Test belegt Herkunft und Deaktivierung.
3. **NanoCell-Mod – IMPLEMENTIERT UND BELEGT:** Der bestehende V1-Kern ist über den optionalen statischen Mod angebunden. Seine lokalen Treffer bleiben von Gate- und Beweisstatus getrennt.
4. **Cockpit-Mod – DOKUMENTIERT / KONZEPT:** Erst auf dem bewiesenen Katalog aufbauen. Die Oberfläche soll denselben Laufweg verwenden und an Run, Abbruch, Wiederverbinden und Replay getestet werden.
5. **Phase-DAG-Mod – DOKUMENTIERT / KONZEPT:** Erst den bestehenden begrenzten V1-Vertrag als eigenen Durchstich implementieren, dann über die bewiesene Mod-Grenze verbinden.

Jeder Schritt ist ein separater, rücknehmbarer Commit. Fehlschläge werden durch Ausschalten des betreffenden CMake-Schalters beziehungsweise durch Zurücknehmen des einzelnen Commits begrenzt; der Core wird nicht als Fallback-Mod dupliziert.

## Abnahme pro Mod

Ein Mod gilt erst als **IMPLEMENTIERT UND BELEGT**, wenn alle Punkte erfüllt sind:

- eigener Unit-Test für Manifest und Core-API-Kompatibilität;
- Integrationstest für Aktivierung und Deaktivierung;
- ein vollständiger Eingabe → Ergebnis → Bundle → Replay-Lauf;
- Negativfall für inkompatible API-Version, ungültige Mod-Eingabe und abgelehnten/erschöpften Core-Lauf;
- der vollständige Core-Testlauf bleibt mit ausgeschaltetem Mod grün;
- die Mod-Dokumentation beschreibt Grenzen und macht keine stärkere Evidenzbehauptung als der Core.

## Bewusst nicht enthalten

- dynamisch nachladbare Binärplugins, Sandboxing oder ein Marktplatz;
- automatische Regelpromotion, autonome Auftragsplanung oder verdeckte Cockpit-Läufe;
- GPU-, Threadpool- oder verteilte Ausführung;
- eine Behauptung, dass freie Sprache fachliche Entscheidungen trifft;
- eine Verschiebung des gesamten bestehenden Quellbaums in einem kosmetischen Großumbau.

## Festgelegte Entscheidungen für spätere Mod-Schritte

1. Die vorhandenen acht `ApplicationTask`-Varianten sind als erste öffentliche Mod-Anfragen zugelassen. Ein Mod führt keine zusätzliche fachliche Auftragsart ohne eigenen Core-Vertrag ein.
2. Das Cockpit wird eine austauschbare Frontend-Schicht. Seine konkrete UI-Technik ist kein Bestandteil der Core- oder Mod-API V1.
3. Phase-DAG startet mit dem begrenzten V1-Durchstich: seriell, deterministisch, azyklisch und fixturebasiert. Weitergehende Zyklen-, GPU- oder Graphfunktionen bleiben eigenständige spätere Entscheidungen.

Damit blockiert nur noch die jeweilige Implementierung einen Mod, nicht seine fachliche Einordnung oder die erste Mod-Grenze.
