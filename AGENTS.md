# BRUCE_raw – Projekthandbuch

## Auftrag und Grenzen

BRUCE_raw ist ein C++20-Forschungskern. Behandle Quellcode, Tests und
reproduzierbare Läufe als Beleg; Roadmaps, Konzepte und ADR-Entwürfe sind keine
Implementierungsbehauptung. Kennzeichne Ergebnisse immer als:

- **IMPLEMENTIERT UND BELEGT**
- **IMPLEMENTIERT, ABER NICHT AKTUELL VERIFIZIERT**
- **DOKUMENTIERT / KONZEPT**
- **VORSCHLAG** oder **OFFENE ENTSCHEIDUNG**

## Orientierung

- Einstieg: [README.md](README.md), dann [docs/README.md](docs/README.md).
- Aktuelle Grenzen: [Blueprint](docs/architecture/03-blueprint-consolidation.md).
- Bedienung und Dateiformate: [CLI-Handbuch](docs/cli/README.md).
- Verträge: [docs/contracts/README.md](docs/contracts/README.md).
- Historische Pläne und Stände: [HISTORY.md](HISTORY.md) und
  [docs/history/README.md](docs/history/README.md).
- Nicht implementierte Ideen: [docs/concepts/README.md](docs/concepts/README.md).

## Arbeiten im Repository

- Bestehende CMake-, CTest-, CI- und Dokumentationsabläufe haben Vorrang.
- Verwende C++20 und ändere Build- oder CI-Konfiguration nur mit belegtem
  Bedarf. Keine Architektur aus Konzepten als fertig darstellen.
- Die generierten CLI-Artefakte `docs/cli/source-index.md`,
  `docs/cli/examples.json` und `docs/cli/validation.md` nur über
  `docs/cli/verify_examples.py` in einer temporären Kopie prüfen bzw.
  aktualisieren; nicht von Hand ändern.
- `scripts/with-host-deps.sh` nur aus einem nativen Projektpfad ohne `:` oder
  Leerraum ausführen. Eine GVFS-SFTP-Ansicht oder Leerraum im Pfad kann die
  Suchpfade des gebündelten `pkg-config` zerlegen; der Wrapper weist solche
  Pfade deshalb mit Exitcode 2 ab. Für die Abnahme einen nativen sicheren
  Pfad oder eine vollständige temporäre Kopie ohne diese Zeichen verwenden.
- `runs/`, `build*/` und `.worktrees/` sind lokale, ignorierte Arbeitsbereiche.
  Das semantische Python-Index-Experiment ist reversibel unter
  `runs/retired-semantic-index/` archiviert und kein aktiver Buildbestand.
- Vor Verschieben oder Umbenennen alle Includes, CMake-Einträge, Skripte,
  Markdown-Links und CI-Pfade prüfen. Nichts endgültig löschen ohne explizite
  Freigabe und genau benannte Zielmenge.

## Prüfpfad

```sh
scripts/with-host-deps.sh cmake -G Ninja -S . -B /tmp/bruce-build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
scripts/with-host-deps.sh cmake --build /tmp/bruce-build
scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-build --output-on-failure
```

Vor Abschluss: `git diff --check`, Git-Status, lokale Markdown-Links und die
für die Änderung relevanten vorhandenen Prüfungen kontrollieren. Unabhängige
bereits vorhandene Änderungen anderer Personen nicht verändern oder bündeln.
