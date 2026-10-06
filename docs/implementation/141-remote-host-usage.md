# Weiterarbeit am neuen SSH-Speicherort / Core 0.67

Aktiver Projektordner: `/home/chris/Desktop/Bruce_raw` auf `192.168.10.102`, Benutzer `chris`. Der SFTP-Pfad bezeichnet denselben Ordner. Mitkopierte Buildverzeichnisse enthalten alte Pfade und sind kein Nachweis für einen Build auf diesem Rechner.

## Benutzerlokale Buildabhängigkeiten

Auf dem neuen Debian-13-Rechner waren CMake, Ninja, GNU C++ und Python vorhanden, aber pkg-config sowie GMP-/OpenSSL-Entwicklungsdateien fehlten. Die erforderlichen Debian-Pakete wurden aus den konfigurierten Paketquellen heruntergeladen und nur unter `build-host-deps/root` entpackt. Es erfolgte keine Installation in Systemverzeichnisse und keine sudo-Konfigurationsänderung.

Für eine erneute Einrichtung im Projektordner:

```bash
mkdir -p build-host-deps/packages build-host-deps/root
cd build-host-deps/packages
apt download pkgconf pkgconf-bin libpkgconf3 libgmp-dev libgmp10 libgmpxx4ldbl libssl-dev libssl3t64
for package in ./*.deb; do dpkg-deb -x "$package" ../root; done
cd ../..
```

Der Paketnamenumfang gilt für die geprüfte Debian-13-Umgebung. Auf anderen Distributionen ist er kein allgemeiner Installer. Die tatsächlich geladenen Pakete bleiben zur Prüfung im genannten Verzeichnis; neuere Repositoryversionen können andere Dateien liefern.

`scripts/with-host-deps.sh` setzt Suchpfade nur für den gestarteten Prozess und seine Kinder: Programme, Bibliotheken, pkg-config und Debian-Multiarch-Header. Es verändert keine Shell-Startdateien. Der Helfer benötigt das vorhandene GNU-C++-Werkzeug zur Ermittlung des Multiarch-Namens und vergibt bei fehlendem Befehl einen Fehlerstatus.

## Bauen und testen

```bash
scripts/with-host-deps.sh cmake -G Ninja -S . -B build-remote-release \
  -DCMAKE_BUILD_TYPE=Release -DOPENSSL_ROOT_DIR="$PWD/build-host-deps/root/usr"
scripts/with-host-deps.sh cmake --build build-remote-release -j 4
scripts/with-host-deps.sh ctest --test-dir build-remote-release --output-on-failure -j 2
scripts/with-host-deps.sh ./build-remote-release/bruce --version
```

Die erste Einrichtung von `build-remote-release` erfolgte hier frisch. Alte Build-Caches wurden nicht wiederverwendet. Das Quellenverzeichnis kann nach einem weiteren Umzug erneut ein neues Buildverzeichnis benötigen. Der Herkunftsindex berücksichtigt inzwischen auch Shellskripte unter `scripts/`.

## Installierte CLI und Archiv

Auch installierte beziehungsweise entpackte Programme benötigen auf diesem Rechner die lokalen Bibliotheken. Daher den Aufrufhelfer verwenden, solange die Bibliotheken nicht regulär im System vorhanden sind:

```bash
scripts/with-host-deps.sh cmake --install build-remote-release --prefix "$PWD/runs/remote-install"
scripts/with-host-deps.sh ./runs/remote-install/bin/bruce --version
```

Der vorhandene [Installationsprüfer](138-installation-usage.md) kann ebenfalls über den Helfer laufen; die zur CLI passende Version ist `0.67.0`. Er prüft Installationsbaum, Export/Replay, Budgetfehler und historische Fixtures. Bibliotheken werden weiterhin nicht in das CLI-Archiv gebündelt.

## Aussagegrenze

Dies ist eine zusätzliche reale Zielmaschine mit eigenem frischem Build und lokalen Laufzeitabhängigkeiten. Es ist kein hermetischer Container, kein Betrieb ohne Bibliotheken und keine öffentliche Releasefreigabe. Quellbaum und lokale Abhängigkeiten bleiben während der Prüfung verfügbar. Mathematik und native Bundleformate wurden für den Umzug nicht geändert; Core bleibt 0.67.

[Remote-Abnahme](acceptance/remote-host-2026-09-23.md) · [M0–M2-Matrix](136-m0-m2-acceptance-matrix.md)
