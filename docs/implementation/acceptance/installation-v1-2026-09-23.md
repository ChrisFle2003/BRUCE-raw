# Lokale Installations-/Archivabnahme / Core 0.66 – 23.09.2026

## Lieferumfang

CMake-basierte Versionsauskunft (`bruce-version-v1`), installierte Buildinformationen und Quellenindex, Abhängigkeits-/Lizenzhinweise sowie lokale CPack-TGZ-Erzeugung. Wiederholbarer Installationstest mit verschobenem Installationsbaum und separatem Arbeitsverzeichnis. Ein aufbewahrtes Archiv ist exakt das zuvor entpackte und geprüfte Archiv.

## Nachweise

Vor Umsetzung scheiterte der Versionstest an `UnknownCommand: --version`; der Installationstest scheiterte an den fehlenden Buildinformationen. Anschließend bestanden beide gezielten Tests.

Der Release-Build wurde in einem neu angelegten, leeren Verzeichnis mit Ninja konfiguriert und vollständig gebaut. Die erste Gesamtsuite bestand mit 103/103 Gruppen. Nach der Korrektur aus dem Abschlussreview und dem letzten Build bestand die vollständige Release-Suite erneut mit **103/103 Gruppen** (46,65 Sekunden). Die beiden gezielten Debug-Prüfungen sowie vier gezielte UBSan-Gruppen bestanden ebenfalls. Die abschließenden Buildlogs enthalten keine Compilerwarnungen oder -fehler. 227 Markdown-Dateien wurden auf lokale Links geprüft; keine ungültigen Ziele.

Das unabhängige Review fand eine Lücke bei Mehrkonfigurationsgeneratoren. Ein vollständiger Ninja-Multi-Config-Release-Build reproduzierte sie: der Installationstest suchte die CLI im falschen Verzeichnis. Nach Korrektur werden tatsächlicher Zielpfad und konfigurationsbezogene Buildinformationen verwendet. Die echte Mehrkonfigurations-Installation und ihr Archiv bestanden ebenfalls; das unabhängige Nachreview bestätigte die Korrektur und die exakte Prüfsumme des aufbewahrten Archivs.

Die installierte sowie die entpackte CLI prüfen jeweils:

- Version und Identität des installierten Quellenindexes;
- affine Forschung mit unabhängigen Prüfpaaren, vollständigem Sitzungsergebnis und identischem Export/Replay;
- Sitzungsbudget 0 mit Fehler in EXECUTING, ohne Ergebnisexport und ohne Überschreiben einer vorhandenen Datei;
- alle 26 historischen Fixturedateien, identisch zur gebauten CLI.

Vier relevante UBSan-Gruppen (`cli_version`, `installed_cli_workflows`, `cli_workflows`, `execution_session_workflows`) bestehen. Eine erneute vollständige UBSan-Suite gehört nicht zu diesem Liefernachweis; die mathematischen Algorithmen wurden nicht geändert.

## Lokale Artefakte

- `runs/releases/core-0.66/bruce-core-0.66.0-Linux-x86_64.tar.gz`
- `runs/releases/core-0.66/installation-report.json`

Der Bericht enthält die konkreten SHA-256-Werte für Binärdatei, Archiv, Quellenindex und historische Eingaben. Installation und Archiv liefern dieselbe Binärprüfsumme. Die Laufzeitabhängigkeiten sind nicht im Archiv gebündelt. Die Artefakte liegen im ignorierten lokalen Ausgabeverzeichnis; dieser Nachweis verlangt keine Aufnahme der Binärdatei in die Quellverwaltung.

## Grenzen

Lokale Linux-x86_64-Abnahme, kein isolierter Fremdrechner. Quell-/Buildverzeichnisse bleiben während der Prüfung vorhanden. Keine Behauptung bitidentischer Builds, keine Signaturen, kein C++-SDK und keine Veröffentlichung. Eine Projektlizenz wurde nicht hinzugefügt. M0-Build/Auslieferung ist damit lokal weiter konkretisiert; vollständige M0–M2-Verträge bleiben offen.

[Vertrag](../137-installation-contract.md) · [Benutzung](../138-installation-usage.md) · [Matrix](../136-m0-m2-acceptance-matrix.md)
