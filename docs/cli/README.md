# BRUCE Core 0.102.0 – CLI-Handbuch

Stand: 04. Oktober 2026. Diese Referenz beschreibt die implementierte Linux-CLI aus `apps/bruce_cli.cpp`, `apps/nanocell_cli.hpp`, `apps/rule_review.hpp` und ihren aufgerufenen Parsern und Ausführern. Architekturentwürfe und geplante Funktionen sind keine zugesicherten CLI-Funktionen. Rechteinhaber: Christopher Flegel.

## Inhalt

1. [Installation, Aufruf und Konventionen](#installation-und-start)
2. [Alle Befehle, Optionen, Standardwerte und Grenzen](commands.md)
3. [Eingabedateien, Zahlen und Exporte](formats.md)
4. [JSON-Ergebnisse, Budgets, Abbruch und Fehler](results-and-control.md)
5. [Ausführbares Tutorial](tutorial.sh)
6. [Vollständiger Optionsindex und Diagnosekatalog](source-index.md)
7. [Gemessene Beispielaufrufe mit vollständigen JSON-Ausgaben](examples.json)
8. [Validierungsnachweis](validation.md)
9. [Benchmark-Suite V1: Messverfahren und Profile](benchmark.md)
10. [Steuerung im Überblick und durchgängiger Abnahmeablauf](control-overview.md)
11. [Gemeinsame Ergebnisbewertung](../implementation/153-result-assessment-usage.md)
12. [Zulassungsgründe und Regelvertrag](../implementation/154-research-gate-reasons-usage.md)
13. [Zulassung eines konkreten Laufs prüfen](../implementation/155-run-gates-usage.md)
14. [Lokale Regelprüfung](rule-review.md)
15. [Aktive Regeln und Entscheidungsjournal](active-rules.md)
16. [Explizite Übergabe: Forschung → Erfahrung](experience-transfer.md)
17. [Explizite Übergabe: Forschung → Z-Auswahl](compatibility-transfer.md)
18. [Gemeinsame Budget- und Abbruchsteuerung der Übergaben](transfer-control.md)

19. [Lokale Dateien, Schreibfehler und Veröffentlichung](local-files.md)

## Installation und Start

Aus dem Projektstamm, mit installiertem C++20-Compiler, CMake ab 3.20, Ninja, pkg-config und Entwicklungsbibliotheken für GMP/GMPXX und OpenSSL Crypto:

```bash
cmake -G Ninja -S . -B build-cli -DCMAKE_BUILD_TYPE=Release
cmake --build build-cli --target bruce -j2
./build-cli/bruce --version
./build-cli/bruce --help
```

Optionaler vollständiger Testlauf: `cmake --build build-cli -j2` und danach `ctest --test-dir build-cli --output-on-failure`. Python 3 ist für Tests und das Tutorial erforderlich. Auf Bruce-AI kann der vorhandene `scripts/with-host-deps.sh`-Wrapper vor den Buildbefehlen die projektspezifisch bereitgestellten Systemabhängigkeiten aktivieren; er gehört nicht zur CLI selbst.

Installation ohne Administratorrechte:

```bash
cmake --install build-cli --prefix "$HOME/.local"
"$HOME/.local/bin/bruce" --version
```

Das installierte Programm ist `bin/bruce`; unter `share/bruce` liegen Buildinformationen, Quellenprüfsummen, `LICENSE`, `NOTICE`, `DEPENDENCIES.md` und `THIRD_PARTY_NOTICES.md`. Dieses Handbuch liegt im Quellbaum unter `docs/cli`; es wird durch den bestehenden Installationsvertrag nicht automatisch mitinstalliert. Das Archiv enthält keine fremden Laufzeitbibliotheken. Details: [Abhängigkeiten](../../DEPENDENCIES.md).

Im Handbuch bedeutet `bruce` entweder das installierte Programm im Suchpfad oder den vollständigen Pfad zur gebauten Datei. In allen Beispielen kann es durch `./build/bruce` ersetzt werden.

## Aufrufregeln

- Ohne Argument, mit `help` oder mit `--help` erscheint Text-Hilfe. Diese globalen Hilfeformen ignorieren nachfolgende Argumente. Befehle wie `research --help` besitzen keine eigene Hilfe und melden `UnknownOption`.
- Jede Option hat genau einen nachfolgenden Wert: `--max-bits 4096`. `--max-bits=4096`, Kurzoptionen, kombinierte Schalter und ein allgemeiner `--`-Trenner werden nicht unterstützt.
- Optionsreihenfolge ist beliebig, Wiederholungen sind Fehler. Unbekannte Optionen werden zurückgewiesen. Optionsnamen und Werte sind groß-/kleinschreibungssensitiv.
- Negative Werte bleiben eigene Argumente: `--min -8`. Zeichenketten mit Leerzeichen, Semikolon oder Shell-Metazeichen gehören in Anführungszeichen, z. B. `--paths '2,6;4,3'` und `--operators '+-*/^'`.
- `rules-approve` und `rules-activate` besitzen einen verpflichtenden Terminaldialog. Die übrigen Befehle besitzen keinen interaktiven Dialog. Es gibt keine Konfigurationsdatei oder Umgebungsvariable zur Auswahl einer Strategie. Eingaben kommen aus Argumenten oder ausdrücklich genannten Dateien. `-` ist kein implementierter Standard-Eingabeplatzhalter.
- `stdout` enthält bei erfolgreicher fachlicher Verarbeitung ein JSON-Objekt mit abschließendem Zeilenumbruch. Hilfe ist Klartext; `rules-approve` und `rules-activate` schreiben einen Klartextdialog und danach bei Erfolg JSON. Fehler stehen als JSON auf `stderr` und liefern Exitcode 2.
- Exitcode 0 bedeutet erfolgreiche Verarbeitung. Er bedeutet weder vollständige Suche noch mathematischen Beweis. `INCOMPLETE`, `NO_CANDIDATE`, `REFUTED`, `UNDECIDED` und Mehrdeutigkeit müssen im JSON ausgewertet werden. Auch NanoCell-`NO_MATCH` und `CONTEXT_REJECTED` sind reguläre Antworten.
- Ohne `--output` gibt es keinen mathematischen Bundleexport. Regelvorschläge und Einzeldatei-Freigaben verwenden Positionsparameter für ihre Ausgabedatei; `rules-activate` aktualisiert den benannten Speicher. Eine Umleitung wie `> ergebnis.json` speichert nur die JSON-Ausgabe. Sie erzeugt keine Replay-Datei.
- Exporte dürfen noch nicht existieren. Auch `experience replace` überschreibt keine Datei. Zielverzeichnisse müssen bereits existieren. Ausnahme: `rules-activate` ersetzt nach konkreter Zustimmung den benannten Regelspeicher atomar samt Journal.

## Erster vollständiger Forschungsdurchlauf

Das [Tutorial](tutorial.sh) erzeugt eigene Beispieldaten in einem neuen Verzeichnis, führt Forschung, Vergleich, Grid, Fortsetzung, Auswahl und Erfahrungskorrektur aus und prüft wesentliche Resultate. Vom Projektstamm:

```bash
bash docs/cli/tutorial.sh ./build/bruce
```

Es löscht keine vorhandenen Dateien. Der am Ende ausgegebene Arbeitsordner enthält JSON-Ergebnisse, Eingaben und replaybare Bundles. Eine gezielt verursachte Budgetüberschreitung demonstriert den Unterschied zwischen Fehlercode und fachlichem Teilbefund.

## Grenzen der Anwendung

BRUCE untersucht begrenzte, eingebaute mathematische Familien. Bei aktiviertem CMake-Schalter `BRUCE_BUILD_MOD_LANGUAGE` bildet `language interpret/select/execute/replay` eine implementierte, deterministische Language-Mod für genau zwei Satzmuster; ausgeschaltet ist der Befehl nicht registriert. Eine freie natürliche Sprachschnittstelle und ein allgemeiner Formelbeweiser sind nicht implementiert. Der optionale statische [NanoCell-Mod](commands.md#nanocell--optionaler-statischer-nanocell-mod) führt bei `BRUCE_BUILD_MOD_NANOCELL=ON` drei feste Byte-Techniken mit gebundenem Kontext aus, schreibt neue herkunftsgebundene Mod-Exporte und kann historische direkte V1-Dateien nur lesen. Er ist nicht Teil der allgemeinen Forschungssitzung. Ein Holdout bestätigt zusätzliche konkrete Zahlenpaare. `compare` kann dagegen Aussagen über zwei ausdrücklich übergebene Polynombrüche über den rationalen Zahlen beweisen; der jeweilige Definitionsbereich gehört zu dieser Aussage.

Die CLI bietet kein Netzwerkprotokoll, keine dynamischen Strategieplugins, keine GPU-Steuerung und kein universelles Zeit-/RAM-Limit. Grid und numerische Z-Kompatibilität verwenden weiterhin begrenzte Ganzzahlen. Exakte BigInt-/Bruchforschung ist ein eigener Pfad.

## Pflege der Referenz

`commands.md`, `formats.md` und `results-and-control.md` erklären das Verhalten. `source-index.md` wird direkt aus dem CLI-Parser und den Fehlerliteralen abgeleitet; `examples.json` enthält echte Aufrufe und ihre vollständigen Antworten. Bei einer neuen Core-Version müssen sowohl Texte als auch die ausgeführten Beispiele erneut geprüft werden. Die [historischen Implementierungspakete](../README.md) bleiben für Herkunft und Detailverträge erhalten.
