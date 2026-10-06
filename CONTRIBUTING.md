# Mitwirken an BRUCE_raw

Danke für dein Interesse an BRUCE_raw. Der Kern ist ein C++20-
Forschungsprojekt: Code, Tests und reproduzierbare Läufe sind Belege;
Roadmaps, Konzepte und ADR-Entwürfe sind keine Implementierungszusagen.
Beiträge Dritter setzen eine vorherige schriftliche Vereinbarung mit
Christopher Flegel voraus; ein Pull Request überträgt keine Rechte und erteilt
keine Lizenz.

## Vor dem ersten Beitrag

1. Kläre vor dem Erstellen eines Pull Requests die schriftliche
   Beitragsvereinbarung mit Christopher Flegel.
2. Lies danach [README.md](README.md), den
   [Dokumentationsindex](docs/README.md) und das [Projekthandbuch](AGENTS.md).
3. Prüfe, ob dein Thema bereits in den [Konzepten](docs/concepts/README.md),
   den [offenen Entscheidungen](docs/implementation/03-open-decisions.md)
   oder bestehenden Issues behandelt wird.
4. Öffne für Fehler einen strukturierten Fehlerbericht und für neue Ideen
   einen Funktionsvorschlag. Sicherheitslücken gehören nicht in öffentliche
   Issues; siehe [SECURITY.md](SECURITY.md).

## Lokale Umgebung

Installiere CMake 3.20 oder neuer, Ninja, einen C++20-Compiler, `pkg-config`,
GMP/GMPXX-, OpenSSL-Crypto- und Python-3-Entwicklungswerkzeuge. Der
[README-Buildpfad](README.md#build-test-und-installation) ist die Referenz für
eine normale lokale Umgebung. In der bereitgestellten Bruce-AI-Umgebung setzt
`scripts/with-host-deps.sh` die lokalen Paketpfade für seinen Kindprozess.

Aus einem nativen Projektpfad ohne `:`:

```sh
scripts/with-host-deps.sh cmake -G Ninja -S . -B /tmp/bruce-build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
scripts/with-host-deps.sh cmake --build /tmp/bruce-build
scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-build --output-on-failure
```

Der Wrapper verweigert absichtlich GVFS-SFTP-Pfade. Verwende dort einen
nativen SSH-Pfad oder eine vollständige temporäre Kopie außerhalb von GVFS.

## Erwartete Prüfungen

Führe vor einem Pull Request mindestens die betroffenen Tests aus. Bei
Änderungen an Build, C++-Code oder Tests ist der vollständige Prüfpfad oben
erwartet. Zusätzlich:

```sh
git diff --check
```

Die CI prüft außerdem lokale Markdown-Links, reproduziert die generierten
CLI-Artefakte und führt einen Secret-Scan aus. Die generierten Dateien
`docs/cli/source-index.md`, `docs/cli/examples.json` und
`docs/cli/validation.md` dürfen ausschließlich über
`docs/cli/verify_examples.py` in einer temporären Kopie aktualisiert oder
geprüft werden, niemals von Hand.

## Änderungen einreichen

- Halte Pull Requests klein und sachlich zusammenhängend.
- Füge bei Verhaltensänderungen passende Tests hinzu und nenne die tatsächlich
  ausgeführten Befehle im Pull Request.
- Aktualisiere Dokumentation, Beispiele und Verträge mit der Implementierung.
- Kennzeichne Inhalte präzise als **IMPLEMENTIERT UND BELEGT**,
  **IMPLEMENTIERT, ABER NICHT AKTUELL VERIFIZIERT**,
  **DOKUMENTIERT / KONZEPT**, **VORSCHLAG** oder **OFFENE ENTSCHEIDUNG**.
- Lege keine Build-Ausgaben, Rohbenchmarkdaten, lokalen Konfigurationsdateien,
  Geheimnisse oder Zugangstokens ab.
- Ändere keine fremden, bereits vorhandenen Arbeitskopie-Änderungen.
