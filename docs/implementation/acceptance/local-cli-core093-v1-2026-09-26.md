# Technischer Abnahmenachweis: local-cli-core093-v1

Datum: 26.09.2026. Zielversion Core 0.93.0. **Ergebnis: begrenztes lokales Profil technisch bestanden.** Das ist kein vollständiger M0–M2-Abschluss, keine neue menschliche Regelbestätigung und keine öffentliche Releasefreigabe.

[Profil und Kriterien](../174-local-cli-acceptance-profile.md) · [M0–M2-Matrix](../136-m0-m2-acceptance-matrix.md) · [Einzeltests und Build-/Paketdaten als JSON](local-cli-core093-v1-2026-09-26.json)

## Neu ausgeführte Prüfungen

| Nachweis | Lokal | Bruce-AI |
| --- | --- | --- |
| Version / Build | 0.93.0, Debug | 0.93.0, Release am Nutzerpfad `./build/bruce` |
| Vollständige CTest-Suite | 117 erfolgreich, 0 fehlgeschlagen, 0 übersprungen | 117 erfolgreich, 0 fehlgeschlagen, 0 übersprungen |
| Installation und TGZ-Paket | bestanden | bestanden |
| Historische Fixtureprüfungen | 26 je Installations-/Paketprüfung | 26 je Installations-/Paketprüfung |
| CLI-Beispiele | 104 erfolgreich | 104 erfolgreich |
| Dokumentationsabdeckung | 67 Optionsnamen, 366 Diagnoseliterale, Dateilinks gültig | gleicher Umfang erfolgreich |

A01–A09 werden durch die vollständige Suite einschließlich der jeweils benannten Gruppen nachgewiesen; A10 durch deren fehlerfreien und nicht übersprungenen Gesamtlauf. A11 besteht durch die zusätzlichen Installations-/Paketprüfungen, A12 durch die CLI-Referenzprüfung und die kontrollierten Dokumentverweise. Das sind begrenzte Testnachweise, keine formale Vollständigkeits- oder Sicherheitsgarantie.

Die manuelle Codeprüfung bestätigt acht allgemeine Laufvarianten, vier separate Experience-Aktionen, zwei konkrete Transferverträge, isolierte Gate-/JSON-Bausteine, die verbliebene Store→Anwendungsaudit-Abhängigkeit und die Einheit „Prüfpunkt“ des Sitzungsbudgets. Die aktuellen Übersichten wurden daran angepasst.

## Umgebung und Quellenidentität

Beide Nachweise melden Linux x86_64, GNU 14.2.0, CMake 3.31.6, OpenSSL 3.5.7 und GMPXX 6.3.0. Die vorhandenen Buildverzeichnisse wurden neu gebaut; es wurde keine frische unabhängige Betriebssystem-/Toolchaininstallation vorgenommen. Debug und Release sind verschiedene Buildkonfigurationen.

Die Quellenindex-Digests unterscheiden sich:

- Lokal: `4ba9ff6585f7d907c72cedaf8fd3e80dcafc6adc2f3ab7b8a31c2284ed55d1ff`
- Bruce-AI: `9b360110875915ddd5f063a8ff486488acafcc7316db965ac474270043005a21`

Der Dateivergleich erklärt dies durch genau eine zusätzliche Remote-Datei: `tests/unit/grid_record_blocks_tests.cpp`. Sie wird im Quellenindex mitgeführt, ist aber keinem expliziten CMake-Build-/Testziel zugeordnet. Alle gemeinsam indizierten Dateien sind bytegleich; nur lokal vorhandene Dateien oder abweichende gemeinsame Dateien wurden nicht gefunden. Die zusätzliche Datei wurde nicht gelöscht oder verändert. Vollständige Checkoutidentität wird deshalb nicht behauptet.

Die JSON-Beilage bewahrt Buildinformationen, Binary-/Archivdigests, Fixtureergebnisse und alle tatsächlich ausgeführten Testnamen und Zeiten. Unterschiedliche Binary-/Archivdigests zwischen Debug und Release sind keine Aussage über bitidentische Reproduzierbarkeit. Testlaufzeiten werden nicht als Forschungsbenchmark interpretiert.

## Weiterhin offene Grenzen

Die offenen Zielanforderungen der Matrix bleiben bestehen: allgemeine Runtime-/Strategieerweiterung, universelle Kosten- und Speichergrenzen, Sprachrollen und externe Evidenz, Expansion/BigInt im Grid/Z, weitergehende Übergabesteuerung und die späteren Blueprint-Komponenten. Der lokale Regelpfad ist eine Bedienkontrolle innerhalb derselben Benutzerumgebung; direkte Dateiwege bleiben verfügbar.

Diese Lieferung konsolidiert Dokumentation und Abnahmenachweise. Runtime und Version bleiben Core 0.93.0. Es wurden keine produktiven Regeln aktiviert.
