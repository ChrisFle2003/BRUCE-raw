# GridSpec, Identität und begrenzter Grid-Cache – Core 0.37

## Auftrag und Umfang

`grid-batch` führt eine geordnete Liste vorhandener Grid-Einzelabfragen mit einer gemeinsamen `GridSpec` aus. `GridCache` ist die wiederverwendbare Anwendungs-API dahinter; sie kann auch Aufträge mit unterschiedlichen Spezifikationen sicher unterscheiden. Jeder Stapel startet mit einem leeren Cache. Ohne ausdrücklichen Export wird nichts dauerhaft gespeichert.

Die Lieferung ergänzt den bestehenden Einzelblatt-Ablauf um Identität, Wiederverwendung und reproduzierbare Cacheentscheidungen. Die Anbindung an die Formelforschung und allgemeine Grid-Expansion bleiben offen.

## Versionierte GridSpec und Schlüssel

`grid-spec-v1` beschreibt ausschließlich die derzeit unterstützte Grid-Form:

| Bestandteil | Festlegung |
| --- | --- |
| Engine | `bruce-grid-v1` |
| Adressregel | `local-8y-plus-x-path-v1`, lokale Koordinaten 0–7, bis zu 64 Pfadschritte |
| Wertregel | `LEAF_LOCAL_POWER_V1`: nur das letzte Paar bestimmt `x^y` |
| Zahlenbereich | `uint64` |
| Auswertungspolitik | `local-u64-single-leaf-v1` |
| Auftragsparameter | `exact`/`modular`/`metric`, Bitgrenze, optionaler Modulus und `0^0`-Regel |

Der vollständige Cache-Schlüssel enthält `BRUCE_GRID_KEY_V1`, die kanonische GridSpec-Darstellung einschließlich aller Versionen und Parameter sowie den vollständigen normalisierten Pfad. `02,06` und `2,6` bezeichnen dieselbe Adresse. `2,6`, `4,3` und `1,1/2,6` sind verschiedene Adressen, obwohl sie alle den Wert 64 liefern. Auch Tiefe und Reihenfolge der Pfadschritte bleiben Teil der Identität.

Für Cachetreffer werden vollständige Schlüssel verglichen. Der SHA-256-Schlüssel in Ausgabe und Trace dient der kompakten Anzeige und wird nicht allein zur Trefferentscheidung benutzt. Bitgrenze und `0^0`-Regel gehören auch dann zum Schlüssel, wenn eine Änderung für den konkreten Wert keinen Unterschied macht.

Die ursprüngliche Pfadschreibweise bleibt in der jeweiligen Einzelausgabe erhalten. Ein Treffer auf `02,06` übernimmt daher nicht den Pfadtext einer vorherigen Anfrage `2,6`. Unbekannte Spec-/Regel-/Cacheversionen im Export werden abgewiesen. Andere Grid-Formen sind keine frei konfigurierbare Funktion dieser Version.

## Cache, Grenzen und Isolation

Der Cache ist lokal zum Executor und speichert nur erfolgreich ausgewertete Ergebnisse. Bis zu 64 Einträge werden in FIFO-Reihenfolge gehalten; ein Treffer verschiebt keinen Eintrag. Bei einer Einfügung werden die ältesten Einträge entfernt, bis sowohl Anzahl als auch Bytebudget eingehalten werden.

- Standardbudget: 65536 Bytes; zulässig: 0 bis 1048576. Null deaktiviert die Speicherung.
- Eintragskosten: UTF-8-Bytes des vollständigen kanonischen Schlüssels plus Bytes des kanonischen Grid-V1-Ergebnisbundles. Der gespeicherte Run verwendet den normalisierten Pfadtext.
- Das ist eine **versionierte logische Byteabrechnung**, keine Messung oder Obergrenze für gesamten Prozessspeicher, Container-/Allocator-Overhead oder die Ausgabeliste. Diese bleibt zusätzlich über die Anfragezahl und Eingabebytes begrenzt.
- Passt ein einzelner Eintrag nicht ins Budget, wird er ohne Verdrängung bestehender Einträge nur berechnet und nicht gespeichert.
- Ein Stapel enthält 1–1024 Abfragen mit zusammen höchstens 262144 Pfadtextbytes. Die CLI begrenzt zusätzlich die gesamte `--paths`-Zeichenfolge einschließlich Trennzeichen auf diesen Wert.
- Jede Abfrage besitzt weiterhin höchstens 64 Schritte; es werden keine Unterräume expandiert.

`STOPPED/BRANCH` gilt nur für das aktuell abgefragte letzte `(0,0)` bei `zero_zero=stop`. Der Stopp wird nie gespeichert und verhindert keine nachfolgende Abfrage. Fehler wie ungültige Pfade oder unzureichende exakte Bitgrenzen scheitern als gesamter Stapel ohne Export; sie werden nicht als Ergebnisse zwischengespeichert. Bereits vorhandene Exportdateien werden nicht überschrieben.

## Ausgabe und Zählung

Die Stapelausgabe enthält Spec, Cachepolitik, Bytebudget, Statistiken und die geordneten Einzelergebnisse. Ein vollständig verarbeiteter Stapel meldet `COMPLETED`, auch wenn einzelne Abfragen lokal gestoppt wurden. Er behauptet damit keine vollständige Forschung oder allgemeine Grid-Erkundung.

| Angabe | Bedeutung |
| --- | --- |
| `evaluations` im Stapel | Tatsächlich neu berechnete Blattwerte; Treffer und Stopps zählen nicht |
| `evaluations` im eingebetteten bisherigen Grid-Ergebnis | Logische Einzelblatt-Auswertung: 1 für einen Wert, 0 für einen Stopp; unverändertes V1-Format |
| `hits` | Wiederverwendete Ergebnisse |
| `misses` | Neu berechnete Werte ohne Treffer |
| `bypasses` | Stopps und wegen Bytebudget nicht gespeicherte Werte |
| `inserts`, `evictions`, `entries` | Einfügungen, Verdrängungen und aktuell gespeicherte Einträge |
| `bytes`, `peak_bytes` | Aktuelle und größte gleichzeitig abgerechnete Cachebelegung |

Je Abfrage entsteht ein Trace-Ereignis mit Index, Schlüsseldigest, Entscheidung (`HIT`, `MISS_INSERT`, `MISS_BYPASS`, `STOP_BYPASS`), aktueller Bytebelegung und kumulierten Verdrängungen. Ein Treffer spart die Potenzberechnung; Schlüsselbildung, Validierung und Darstellung bleiben erforderlich. Es wird kein allgemeiner Laufzeitvorteil behauptet.

## Export und Replay

`BRUCE_GRID_BATCH_BUNDLE_V1` enthält Digest, alle Spec-/Cacheversionen, alle Parameter, Bytebudget, geordnete ursprüngliche Pfade, Trace und Ergebnis. Cachepolitik: `fifo-64-canonical-evidence-bytes-v1`.

Replay baut einen leeren Cache auf und führt die komplette Liste erneut aus. Danach wird die gesamte kanonische Payload bytegenau verglichen. Geänderte Entscheidungen, Werte, Zähler, Metadaten, gekürzte oder zusätzliche Inhalte werden auch bei neu berechnetem Digest abgewiesen, wenn sie nicht vollständig mit dem rekonstruierten Auftrag übereinstimmen. Ein vollständig konsistent geänderter Auftrag ist ein anderer gültiger Auftrag; der Digest ist keine Urhebersignatur.

Alte `BRUCE_GRID_BUNDLE_V1`-Exporte und die Einzelabfrage `grid` behalten ihr Format. Der neue Stapel ist noch keine Portfoliofamilie, kein Experience-Eintrag und keine Quelle für Z-Auswahl. Die vorhandenen mathematischen Ausdrucks-/Bereichscaches bleiben eigenständige Komponenten.

[Benutzung](79-grid-cache-usage.md) · [Abnahme](acceptance/grid-cache-v1-2026-09-22.md)
