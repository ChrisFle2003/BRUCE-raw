# BRUCE_raw

BRUCE_raw ist ein begrenzter C++20-Forschungskern für überprüfbare mathematische
Kandidaten, Erklärungen und Replay. Der aktuelle, dokumentierte Kernstand ist
Core 0.102; dessen tatsächliche Aussagegrenzen stehen im
[Blueprint](docs/architecture/03-blueprint-consolidation.md).

Der originale BRUCE-Quellcode ist proprietär und steht unter der
[BRUCE Proprietary License](LICENSE). Alle Rechte liegen bei Christopher
Flegel; Verkauf oder Weiterverkauf erfordern eine vorherige schriftliche
Vereinbarung. Es ist eine lokale Linux-CLI und kein Netzwerkdienst.
Konzeptpapiere und Roadmaps beschreiben keine zugesicherten Runtime-Funktionen.

## Einstieg

- [Dokumentationsindex](docs/README.md) – Architektur, Verträge, Umsetzung und CLI.
- [CLI-Handbuch](docs/cli/README.md) – Befehle, Datenformate und Beispiele.
- [Projekthandbuch für Mitwirkende und Agenten](AGENTS.md) – Arbeitsregeln und Prüfungen.
- [Konzepte](docs/concepts/README.md) – dokumentierte Ideen, nicht als Implementierung lesen.
- [Historischer Root-Einstieg](HISTORY.md) – bisherige Entwicklung und frühere Stände.

## Voraussetzungen

Der dokumentierte und in CI geprüfte Pfad ist Ubuntu 24.04 mit:

- einem C++20-Compiler (CI: `g++`), CMake 3.20 oder neuer und Ninja;
- `pkg-config`, GMP/GMPXX und OpenSSL Crypto einschließlich
  Entwicklungsdateien;
- Python 3 für Integrationstests und das CLI-Tutorial.

Auf Debian/Ubuntu installiert der folgende Befehl die benötigten Pakete:

```sh
sudo apt-get update
sudo apt-get install -y cmake ninja-build g++ pkg-config libgmp-dev libssl-dev python3 python3-dev
```

## Build, Test und Installation

Mit den oben installierten Abhängigkeiten erzeugt dieser Ablauf einen
Release-Build und führt die vollständige Testsuite aus:

```sh
cmake -G Ninja -S . -B build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Für eine lokale Installation ohne Administratorrechte:

```sh
cmake --install build --prefix "$HOME/.local"
"$HOME/.local/bin/bruce" --version
```

Der installierte Befehl ist `bruce`. Details zu installierten Dateien,
Abhängigkeiten und Drittanbieterhinweisen beschreibt das
[CLI-Handbuch](docs/cli/README.md).

## Lokale Nutzung und Konfiguration

```sh
./build/bruce --version
./build/bruce --help
bash docs/cli/tutorial.sh ./build/bruce
```

Die Runtime verwendet keine versteckte Konfigurationsdatei und keine
Umgebungsvariable zur Strategiewahl. Eingaben erfolgen über CLI-Argumente und
ausdrücklich benannte Dateien. Die zwei eingebauten, statischen Mods lassen
sich beim Konfigurieren gezielt ein- oder ausschalten:

```sh
cmake -G Ninja -S . -B build \
  -DBRUCE_BUILD_MOD_LANGUAGE=ON \
  -DBRUCE_BUILD_MOD_NANOCELL=ON
```

Mit `=OFF` wird der jeweilige Mod nicht gebaut und sein CLI-Befehl nicht
registriert. Formate, Grenzen und vollständige Beispiele stehen im
[CLI-Handbuch](docs/cli/README.md).

## Bereitgestellte Bruce-AI-Abhängigkeiten

Die vorhandene Abhängigkeitsumgebung wird nicht verändert. Für einen frischen
Release-Build außerhalb des Repositorys:

```sh
scripts/with-host-deps.sh cmake -G Ninja -S . -B /tmp/bruce-build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
scripts/with-host-deps.sh cmake --build /tmp/bruce-build
scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-build --output-on-failure
```

Der Wrapper erfordert einen nativen Projektpfad ohne `:`. Er verweigert daher
bewusst den Start aus einer GVFS-SFTP-Ansicht wie `sftp:host=…`: Dort würden
Suchpfade in Fragmente zerfallen und unbemerkt auf Host-Abhängigkeiten
zurückfallen. Nutze in diesem Fall den nativen SSH-Pfad auf dem Zielrechner
oder eine vollständige temporäre Kopie außerhalb von GVFS.

`BRUCE_BUILD_MOD_LANGUAGE=ON` und `BRUCE_BUILD_MOD_NANOCELL=ON` aktivieren die
standardmäßig gebauten statischen Mods. Mit dem jeweiligen `=OFF` baut der Core
ohne dessen Quellen; der zugehörige CLI-Befehl ist dann nicht vorhanden. Es gibt
keine dynamische Plugin-Suche oder nachgeladenen Fremdbinärdateien.

## Mitwirken

Vor einem Pull Request bitte mindestens die für die Änderung relevanten Tests
sowie `git diff --check` ausführen. Das vollständige Vorgehen, die
Dokumentationsregeln und den Umgang mit generierten CLI-Artefakten erläutert
[CONTRIBUTING.md](CONTRIBUTING.md). Sicherheitslücken gehören nicht in
öffentliche Issues; siehe [SECURITY.md](SECURITY.md).

## Mod-Stand

- **IMPLEMENTIERT UND BELEGT:** `bruce_mod_runtime`, fester Build-Katalog,
  die optionale deterministische Language-Mod und die optionale NanoCell-Mod.
  Neue Sprach- und NanoCell-Exporte verwenden `BRUCE_MOD_RUN_V1`; direkte
  historische `BRUCE_LANGUAGE_RUN_V1`- beziehungsweise
  `BRUCE_NANOCELL_RUN_V1`-Dateien bleiben read-only replaybar.
- **DOKUMENTIERT / KONZEPT:** freie Sprache, NanoCell als allgemeiner
  `ApplicationTask`-Lauf, Cockpit und Phase-DAG.

## Einordung des Bestands

- **IMPLEMENTIERT UND BELEGT:** nur Aussagen mit aktuellem Quell-, Test- oder
  reproduzierbarem Laufbeleg.
- **IMPLEMENTIERT, ABER NICHT AKTUELL VERIFIZIERT:** vorhandener Code ohne
  frische Prüfung in der jeweiligen Umgebung.
- **DOKUMENTIERT / KONZEPT:** Entwürfe, Roadmaps und Architekturziele.
- **VORSCHLAG / OFFENE ENTSCHEIDUNG:** noch nicht beschlossene oder nicht
  umgesetzte Arbeit.

Lokale Ausgaben gehören nach `runs/`; Build-Verzeichnisse und lokale
Arbeitsbäume bleiben ignoriert. Kleine Benchmark-Zusammenfassungen sind unter
`benchmarks/` versioniert, Rohdaten bleiben lokal.
