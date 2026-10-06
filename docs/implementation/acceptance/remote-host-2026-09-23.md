# Zusätzliche Zielmaschine und Arbeitsortwechsel / Core 0.67 – 23.09.2026

## Arbeitsort und Umgebung

Aktiver Ordner: `/home/chris/Desktop/Bruce_raw` auf `192.168.10.102`, SSH-Benutzer `chris`. CMake-Projektstand 0.67.0 wurde direkt im verschobenen Ordner bestätigt. Debian GNU/Linux 13, x86_64, GNU C++ 14.2.0, CMake 3.31.6, Python 3.13.5.

Vorhandene Buildverzeichnisse wurden nicht als Remote-Buildnachweis verwendet. `build-remote-release` wurde frisch konfiguriert und gebaut. pkg-config und GMP-/OpenSSL-Entwicklungsdateien fehlten zunächst. Acht Debian-Pakete wurden ausschließlich unter `build-host-deps` heruntergeladen und entpackt; kein sudo und keine Installation in Systemverzeichnisse. Verwendet wurden pkgconf 1.8.1, GMP/GMPXX 6.3.0 und OpenSSL 3.5.7.

Der erste Build mit lokalen Paketen zeigte den fehlenden Multiarch-Headerpfad (`gmp.h`). Der Aufrufhelfer wurde entsprechend ergänzt. Der anschließende Build bestand. `ldd` bestätigt die Verwendung der lokalen libcrypto-/libgmp-Dateien zusammen mit vorhandenen Systemlaufzeitbibliotheken.

## Prüfungen

- Vollständige Release-Suite auf diesem Rechner: **105/105 Testgruppen bestanden**, 104,30 Sekunden.
- Frische Installation, verschobener Installationsbaum und hier erzeugtes Archiv: Versions-/Quellenindexprüfung, vollständige affine Forschung mit unabhängigen Prüfpaaren, Export und identischer Replay, Budgetfehler ohne Export/Überschreiben sowie 26 historische Fixtures bestanden.
- Das unveränderte mitgebrachte Core-0.67-Archiv wurde separat entpackt und gegen die hier frisch gebaute CLI geprüft: dieselben Funktionsfälle und alle 26 historischen Fixtures bestanden.
- Neues Archiv und zugehöriger Prüfbericht besitzen übereinstimmende SHA-256-Werte; installierte und archivierte CLI haben dieselbe Binärprüfsumme.
- Der Aufrufhelfer meldet einen fehlenden Befehl mit Exit 2 und begrenzt seine Umgebungsänderungen auf den Kindprozess. Shellskripte werden nun im CMake-Quellenindex berücksichtigt.

## Artefakte und Reproduktion

Unter `runs/host-validation/192.168.10.102/` liegen `installation-report.json`, `original-archive-report.json`, das neue CLI-Archiv und die Umgebungs-/Paketidentität. Build- und Testlogs liegen unter `build-host-deps/`. [Anleitung](../141-remote-host-usage.md).

## Grenzen

Eine zusätzliche tatsächliche Linux-Zielumgebung ist geprüft. Die lokalen Bibliotheken und der Quellbaum bleiben während dieser Abnahme verfügbar; dies ist kein hermetisch isolierter Test und kein unabhängiges Betriebssystem-/Architekturmatrix-Ergebnis. Die CLI bündelt ihre Laufzeitbibliotheken weiterhin nicht. Keine öffentliche Veröffentlichung, Lizenzentscheidung oder vollständige M0–M2-Freigabe. Die mathematische Runtime und die Bundleformate wurden nicht geändert; Version bleibt 0.67.0.
