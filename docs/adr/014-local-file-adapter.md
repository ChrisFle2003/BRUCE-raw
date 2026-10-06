# ADR-014: Gemeinsamer lokaler Dateiadapter

Status: umgesetzt mit Core 0.102. Kontext: lokale Linux-CLI; bestätigt durch die Entscheidung zur Vereinheitlichung der Dateiverantwortung.

## Entscheidung

`LocalFiles` in `bruce/io/local_files.hpp` und `src/io/local_files.cpp` besitzt begrenztes Lesen, exklusiven Export, Pfadbindung und Laden des Regelspeichers sowie den gesperrten bestätigten Wechsel. Der Baustein ist zustandslos; Callbacks werden nur synchron weitergereicht.

Die fünf Transferbefehle verwenden ihn direkt, einschließlich kontrolliertem `experience add`. `read_bounded` und `export_exclusive` bleiben kompatible Bibliothekseinstiege und delegieren an ihn. Bestehende Benutzer dieser Funktionen werden damit nicht zu neuen Steuerungsverträgen gezwungen.

Der Regel-CLI-Adapter behält Argumente, Anzeige, Terminalprüfung und Einlesen der konkreten Bestätigung. Erst danach ruft er den Dateiadapter mit derselben kanonischen Zielbindung auf. Der Adapter sperrt, lädt neu, prüft Basis/Generation und Bestätigung im bestehenden Anwendungsmodell und ersetzt aktive Regeln samt Journal gemeinsam. Es gibt keine öffentliche generische Überschreibfunktion; die interne Commitfunktion bleibt privat in der Implementierungsdatei.

## Unveränderte Semantik

Prüfpunktpositionen, Zähler, Fehlertexte, Dateiformate und native Ergebnisse sollen unverändert bleiben. Exklusiver Export verwendet weiterhin temporäre Datei und `link`; Regelspeicherwechsel weiterhin Sperre, temporäre Datei, `rename` und Verzeichnissynchronisation. Diese unterschiedlichen Veröffentlichungsverträge werden nicht vereinheitlicht.

Eine Bestätigungszeichenfolge ist keine Authentifizierung. Die CLI fordert weiterhin menschliche Interaktion; Bibliotheksaufrufer sind im vereinbarten lokalen Vertrauensmodell selbst für diese Interaktion verantwortlich. Kein separater Signierdienst wird eingeführt.

## Abwägung und Grenzen

Eine rein neue Wrapperklasse ohne Verlagerung der Dateiarbeit würde die Zuständigkeit nicht klären. Hier liegt die tatsächliche Implementierung im Infrastrukturbaustein. Virtuelle Dateisysteme, dynamische Backends und ein allgemeiner Speichertransaktionsdienst sind für diesen lokalen Schritt nicht erforderlich.

Der Adapter hängt für Regelspeicheroperationen vom Anwendungsmodell `RuleHistory` ab. SHA-256 bleibt im Evidenzbaustein. Einzelne Systemaufrufe können blockieren; Crash-Dauerhaftigkeit und Fehler nach erfolgtem `rename` behalten ihre bisherigen Grenzen. Regelaktivierung erhält keine neue Abbruchoption.

## Nachweise

[Umfang und Abnahme](../implementation/183-local-file-adapter-usage.md), [CLI-Dateivertrag](../cli/local-files.md), bestehende Block-/Export-/Regeldialogtests sowie neue echte Schreibfehlerfälle mit begrenzter Dateigröße im Kindprozess.
