# Lokal bauen, installieren und als Archiv prüfen / Core 0.66

Die CLI kann jetzt mit einem wiederholbaren Ablauf als installierte Anwendung geprüft werden. Die Abnahme verschiebt das Installationsverzeichnis, arbeitet aus einem unabhängigen temporären Ordner und prüft Forschung, unabhängige Prüfpaare, Export, Replay, Budgetfehler und sämtliche historischen Fixturedateien.

## Frischer Build

```bash
cmake -G Ninja -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j 4
ctest --test-dir build-release --output-on-failure -j 2
```

Für einen wirklich frischen Build ein noch nicht vorhandenes Buildverzeichnis verwenden. `installed_cli_workflows` installiert ausschließlich in ein temporäres Ziel. Die CTest-Prüfung verändert keine Systeminstallation. `cli_version` prüft die Versionsauskunft.

## Installieren und benutzen

```bash
cmake --install build-release --prefix "$PWD/runs/local-install"
./runs/local-install/bin/bruce --version
./runs/local-install/bin/bruce --help
```

Die Version wird aus CMake erzeugt. Im Installationsziel liegen außerdem `share/bruce/build-info.json`, `source-files.sha256` und `DEPENDENCIES.md`. Der Buildbericht nennt Konfiguration, Compiler, Systemarchitektur, CMake-, GMPXX- und OpenSSL-Version sowie die Prüfsumme des Quellenindexes.

`cmake --install` selbst ist der normale CMake-Installer und kann vorhandene Installationsdateien ersetzen. Für eine Abnahme ein leeres Ziel wählen. Native Ergebnisexporte bleiben weiterhin exklusiv.

## Archiv prüfen und aufbewahren

```bash
mkdir -p runs/releases/core-0.66
python3 scripts/verify_install.py \
  --build-dir build-release --fixtures tests/fixtures --version 0.66.0 \
  --config Release --package \
  --archive-out runs/releases/core-0.66/bruce-core-0.66.0.tar.gz \
  --report runs/releases/core-0.66/installation-report.json
```

Das Skript erzeugt mit CPack ein TGZ-Archiv, entpackt es in ein neues Ziel und wiederholt dieselben Funktionsprüfungen. `--archive-out` bewahrt genau das geprüfte Archiv auf. Archiv- und Berichtsziele dürfen noch nicht existieren; ohne diese Optionen bleiben nur die JSON-Ausgabe und keine dauerhaften Artefakte. Historische Eingaben werden kopiert und nicht verändert.

Der Bericht `bruce-install-audit-v1` enthält Binär-, Archiv- und Fixtureprüfsummen sowie die installierten Buildinformationen. Er bestätigt die geprüften lokalen Abläufe, keine mathematischen Allgemeinbeweise. Bei Fehler wird der Ablauf mit einem Fehlerstatus beendet.

Mehrkonfigurationsgeneratoren werden über `--config Release` oder `--config Debug` berücksichtigt. Für besondere Buildlayouts lässt sich `--baseline /absoluter/pfad/bruce` angeben. CTest übergibt automatisch den tatsächlichen Zielpfad. Die installierten Metadaten nennen die tatsächlich gewählte Konfiguration.

## Voraussetzungen und Grenzen

Die CLI benötigt kompatible dynamische Systembibliotheken. Das Archiv bündelt sie nicht. [Abhängigkeiten und Lizenzstatus](../../DEPENDENCIES.md) sind ausdrücklich dokumentiert: Im aktiven Projekt fehlt eine Projektlizenz; diese technische Abnahme erteilt keine öffentliche Weiterverteilungsfreigabe.

Geprüft ist die vorhandene Linux-Umgebung. Quell- und Buildverzeichnisse bleiben während der Prüfung vorhanden; der verschobene Aufruf beweist keine vollständig isolierte Fremdrechnerinstallation. Ein wiederholbarer Prüfablauf bedeutet nicht bitidentische Builds. Kein signiertes Release und keine vollständige M0–M2-Freigabe.

[Vertrag](137-installation-contract.md) · [Abnahmematrix](136-m0-m2-acceptance-matrix.md)
