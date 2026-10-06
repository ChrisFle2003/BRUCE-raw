# Versionierter Grid-Einzelauftrag – Core 0.36

## Umfang

Der bestehende CLI-Aufruf `grid` wird durch `GridTask` und `GridRun` in `application/grid` ausgeführt. `--output` exportiert den abgeschlossenen Einzelauftrag; `replay` rekonstruiert ihn. Die CLI übernimmt Eingabe, Dateiexport und Darstellung. Gültige bisherige Abfragen behalten ihre JSON-Ausgabe.

Das ist eine einzelne lokale Blattauswertung, kein neuer Suchraumgenerator, kein Grid-Cache, keine BigInt-Erweiterung und keine automatische Einbindung als Portfoliofamilie.

## Auftrag und Mathematik

Der Auftrag enthält den ursprünglichen Pfadtext, Modus, Bitgrenze, optionalen Modulus und die `0^0`-Politik. Der validierte Pfad besitzt 1–64 Schritte, jeder mit lokalen Koordinaten 0–7. Die Werte entstehen ausschließlich aus dem **letzten** Paar `x,y`: `LEAF_LOCAL_POWER_V1` wertet `x^y` aus. Vorherige Schritte bleiben Adressidentität, beeinflussen aber nicht den Blattwert. Gleiche Werte sind keine gleichen Adressen.

- `exact`: geprüfte UInt64-Potenz mit `max_bits` von 1 bis 64.
- `modular`: bestehende modulare Potenz mit ausdrücklich angegebenem positivem UInt64-Modulus. Ein Modulus außerhalb dieses Modus ist unzulässig. `max_bits` wird auf 1–64 validiert, begrenzt hier aber nicht den Restwert.
- `metric`: exakte Bitlänge des kleinen lokalen Potenzwerts; `metric_exact=true`. `max_bits` wird auf 1–64 validiert, die Wertberechnung verwendet wie bisher 64 Bits. Keine allgemeine Großzahlmetrik.
- `zero_zero=one`: `0^0=1`, im modularen Modus entsprechend reduziert.
- `zero_zero=stop`: Nur das letzte Paar `(0,0)` erzeugt `STOPPED`, `stop_scope=BRANCH`, `GridSentinel` und null Auswertungen. Ein früheres `(0,0)` stoppt diese Einzelabfrage nicht.

Für jeden anderen gültigen Auftrag wird genau ein Blatt ausgewertet. `theoretical_space=64^depth` beschreibt die Adressebene; es werden nicht entsprechend viele Kandidaten erzeugt oder berechnet.

## Grenzen und Fehler

Eingabepfad und Bundle sind auf die bestehende Dateibytegrenze begrenzt. Ungültige Pfade, Tiefe über 64, Koordinaten außerhalb 0–7, ungültige Modi/Politiken/Bitgrenzen und fehlender oder nullwertiger Modulus werden zurückgewiesen. Modulusprüfung findet auch vor einem Sentinel-Stopp statt.

Eine unzureichende exakte Bitgrenze bleibt wie bisher ein Fehler, kein exportierbarer Teilrun. Erst nach erfolgreicher Auswertung oder gültigem Sentinel-Stopp entsteht ein Run. Fehler erzeugen keine Ausgabedatei. Der vorhandene exklusive Export überschreibt keine vorhandenen Dateien.

Die Einzelabfrage hat keine zusätzliche Operations-/Kandidaten-/Abbruchsteuerung: Pfadtiefe und lokale Potenz sind bereits fest begrenzt. Ein späterer expandierender Grid-Executor benötigt eigene Budget-/Stoppverträge. Vier feste Trace-Ereignisse ersetzen keinen solchen Executor.

## Export und Prüfschritte

Format `BRUCE_GRID_BUNDLE_V1`, Engine `bruce-grid-v1`, Regel `LEAF_LOCAL_POWER_V1`, Politik `local-u64-single-leaf-v1`. Nach Header und SHA-256-Digest enthält die Payload ausdrücklich:

```text
engine bruce-grid-v1
rule LEAF_LOCAL_POWER_V1
policy local-u64-single-leaf-v1
path 2,6/4,3
mode exact
max_bits 64
modulus none
zero_zero one
TRACE
PATH_VALIDATED depth=2
LEAF x=4 y=3
CONFIG mode=exact max_bits=64 modulus=none zero_zero=one
EVALUATED evaluations=1
RESULT
...bisherige JSON-Ausgabe...
```

Beim Sentinel steht statt des letzten Ereignisses `STOP GridSentinel scope=BRANCH evaluations=0`. Die ursprüngliche Pfadschreibweise bleibt erhalten; andere numerische Parameter werden kanonisch geschrieben. Die feste Konfigurationszeile macht auch eine einzelne geänderte Policyzeile sichtbar, deren Änderung für diesen Blattwert zufällig keinen Unterschied hätte.

Replay akzeptiert nur die bekannten Metadaten, liest alle Auftragsparameter aus der Datei und rechnet den Auftrag erneut. Danach wird die gesamte kanonische Payload einschließlich Trace und Ergebnis bytegenau verglichen. Verkürzte, zusätzliche oder widersprüchliche Inhalte werden auch nach neu berechnetem Digest abgelehnt.

Der Digest ist ein Integritätsmerkmal, keine Urhebersignatur: Ein vollständig konsistent neu berechneter anderer Auftrag ist eine andere gültige Datei. Replay bestätigt die reproduzierbare Berechnung, keine externe Herkunftsbehauptung.

## Architektur und nächste Arbeit

Die bisher direkt im CLI-Zweig stehende Grid-Auswertung besitzt jetzt eine Anwendungs-API und einen eigenen Codec. Die Ausgabe bleibt kompatibel; vollständige GridSpec-/Adress-/Modus-Cacheidentität, Bytebudget, Forschungseinbindung, BigInt und allgemeine Aststeuerung bleiben offen. Die aktualisierte [Architekturmatrix](75-current-architecture-and-milestones.md) führt diese Grenze ausdrücklich weiter. Kein pauschaler M2-Abschluss.

[Benutzung](77-grid-replay-usage.md) · [Abnahme](acceptance/grid-replay-v1-2026-09-22.md)
