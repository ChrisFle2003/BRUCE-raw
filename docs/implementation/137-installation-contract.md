# Lokale Auslieferungsabnahme / Core 0.66

## Umfang

Der nächste freigegebene Schritt aus der M0–M2-Matrix prüft die tatsächliche CLI-Installation und ein lokales Archiv. Keine Änderung mathematischer Algorithmen oder Bundleformate. Die Version kommt aus CMake; `bruce --version` liefert JSON. Installation enthält Buildinformationen und Abhängigkeits-/Lizenzhinweise. Mangels vorhandener Projektlizenz wird keine Lizenz neu vergeben und keine öffentliche Distribution behauptet.

## Abnahmekriterien

- Frischer Release-Build und vollständige Tests in einem zuvor leeren Verzeichnis.
- Installation in ein leeres temporäres Ziel, danach Verschieben des gesamten Installationsbaums. CLI-Aufruf aus einem unabhängigen leeren Arbeitsverzeichnis mit absoluten Eingabe-/Ausgabepfaden.
- Vollständige affine Forschung mit unabhängigen Prüfpaaren, Export und identischer Replayausgabe. Gemeinsames Budget 0 muss scheitern und darf keine Ergebnisdatei erzeugen oder überschreiben.
- Alle historischen `.bruce`-Fixtures werden mit Build- und installierter CLI identisch wiedergegeben. Mindestens eine Fixture ist erforderlich.
- Ein lokal mit CPack erzeugtes TGZ-Archiv wird in ein neues Ziel entpackt und mit denselben Prüfungen abgenommen.
- Bericht enthält Version, Buildinformationen, Binär-/Fixture-/Archivprüfsummen und geprüfte Fälle. Ein reproduzierbares Prüfverfahren ist keine Behauptung bitidentischer Builds.
- Installationsprüfung läuft als CTest-Gruppe und kann separat einen JSON-Bericht schreiben. Archivprüfung ist optional für die separate Abnahme.

## Grenzen

Geprüft wird die vorhandene Linux-Umgebung. Dynamische Systembibliotheken müssen am Ziel vorhanden sein. Der Quellbaum wird nicht entfernt oder versteckt; der verschobene Aufruf weist einen Betrieb ohne Arbeitsverzeichnisbindung nach, keinen isolierten Fremdrechner. Kein signiertes Release, keine plattformübergreifende Garantie, keine pauschale M0–M2-Abnahme. Das CLI-Paket enthält keine C++-SDK-Installation.
