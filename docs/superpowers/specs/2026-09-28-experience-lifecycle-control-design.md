# Design: kontrolliertes `experience revoke` und `experience replace`

## Status

Vorgeschlagene Architekturentscheidung, 28.09.2026. Diese Spezifikation autorisiert noch keine Implementierung. Ein separater Umsetzungsplan und dessen Freigabe bleiben erforderlich.

## Ziel

Die bestehenden mutierenden Befehle

```text
bruce experience revoke  [--store STORE] --id ID --reason TEXT --output NEW
bruce experience replace [--store STORE] --id ID --reason TEXT --from-run RUN --output NEW
```

erhalten opt-in dieselbe begrenzte Ausführungssteuerung wie `experience add`. So können optionales Lesen der Basis, fachliche Snapshotverarbeitung und die exklusive Veröffentlichung gemeinsam abgebrochen oder begrenzt werden, ohne einen bisherigen Aufruf zu verändern.

## Nachweisbarer Ausgangspunkt

- Die CLI akzeptiert die Steueroptionen bereits für `experience add`, `experience plan-add`, `experience apply-add`, `compatibility-plan` und `compatibility-apply`.
- `experience revoke` und `experience replace` lesen die optionale Basis bisher über den unkontrollierten Leseweg, rufen `execute_experience` ohne Callback auf und exportieren unkontrolliert.
- `execute_experience` akzeptiert bereits einen optionalen `ReplayCheckpoint`; seine Snapshot-Inspektion verwendet ihn. `LocalFiles::read` und `LocalFiles::publish_new` unterstützen denselben Callback am gesamten Datei- beziehungsweise Exportweg.
- `TransferControl` prüft erst einen angeforderten Signalabbruch, dann das Budget, und zählt anschließend den Kontrollpunkt. Der vorhandene opt-in Erfolgsmantel heißt `controlled-transfer-v1`.

Quellen: `apps/bruce_cli.cpp`, `include/bruce/application/experience_task.hpp`, `src/application/experience_task.cpp`, `include/bruce/application/transfer_control.hpp`, `src/io/local_files.cpp`, `docs/cli/transfer-control.md`.

## Entscheidung

### Öffentliche Erweiterung

Für genau diese beiden Aktionen werden die vorhandenen Optionen zugelassen:

```text
--max-transfer-checkpoints N
--transfer-cancellation none|checkpoints
```

Sie verwenden unverändert das Modell `transfer-checkpoints-v1`; das Konto beginnt pro CLI-Aufruf bei null. Bei Angabe mindestens einer der beiden Optionen erfolgt eine erfolgreiche Antwort im bestehenden Mantel:

```json
{
  "contract": "controlled-transfer-v1",
  "result": { "...": "bestehender Experience-Bericht" },
  "usage": {
    "model": "transfer-checkpoints-v1",
    "max_checkpoints": 123,
    "checkpoints": 42
  }
}
```

`result` enthält den bisherigen nativen Bericht unverändert. Ohne die neuen Optionen bleiben Optionsmenge, stdout-Erfolgsausgabe, Fehlerhülle, Exitcode und erzeugte Snapshotbytes unverändert. Ein explizites `--transfer-cancellation none` aktiviert wie bei `experience add` trotzdem die Hülle und den kontrollierten Pfad.

### Kontrollierter Bereich

Bei aktivierter Steuerung wird **dasselbe** `TransferControl`-Konto an diese Schritte gebunden:

1. das Lesen von `--store`, falls es angegeben ist;
2. bei `replace` das Lesen des Forschungsbundles aus `--from-run`;
3. `execute_experience` einschließlich des bestehenden Checkpoints vor Snapshotarbeit, der fachlichen Revoke-/Replace-Operation, der Snapshot-Inspektion und des Checkpoints danach;
4. die Vorbereitung und exklusive Veröffentlichung von `--output` über `LocalFiles::publish_new`.

Die fachliche Revoke- oder Replace-Operation erhält denselben Callback für die bestehende Snapshotprüfung, Ereignisanwendung und bei Replace die bereits vorhandene Quellenprüfung. Die bestehenden atomaren Einzeloperationen bleiben atomar; dieses Paket fügt keine neue Domainsemantik hinzu. Das eingelesene `--from-run`-Bundle wird als Teil von `ExperienceReplaceTask` der bestehenden fachlichen Quellprüfung übergeben.

Die Veröffentlichung folgt dem vorhandenen `publish_new`-Vertrag: Ein vorhandenes Ziel wird nie ersetzt. Erkennt ein Kontrollpunkt vor der Veröffentlichung einen Abbruch, wird die temporäre Datei über den vorhandenen Aufräumpfad entfernt, soweit das Dateisystem dies zulässt.

### Fehler- und Abbruchvertrag

- An jedem gemeinsamen Kontrollpunkt bleibt die Reihenfolge Signalabbruch (`TransferCancelled`) vor Budgeterschöpfung (`TransferCheckpointBudgetExceeded`) bestehen.
- Ein Steuerungsfehler beendet den Befehl mit Exit 2 und der vorhandenen einfachen Fehlerausgabe auf stderr. Es gibt keinen fachlichen Teilbericht und keinen `controlled-transfer-v1`-Mantel auf stdout.
- Ein Budget von 0 scheitert am ersten erreichten Kontrollpunkt; bei vorhandener Basis ist das vor ihrem Öffnen, andernfalls vor der fachlichen Snapshotarbeit.
- Fachliche Fehler — etwa unbekannte ID, unzulässiger Ersatzlauf, ungültiger oder zu großer Basissnapshot und ein vorhandenes Ausgabeziel — behalten ihren bisherigen Vertrag. Sie werden nicht in Steuerungsfehler umgedeutet.

## Abgrenzung

- Nur `experience revoke` und `experience replace` gehören zu diesem Paket.
- `experience inspect` bleibt ein rein lesender Befehl und akzeptiert keine Transfersteuerungsoptionen.
- `experience add`, `plan-add` und `apply-add` sowie die Kompatibilitäts- und Regelverwaltungswege werden nicht verändert.
- Es gibt keine neue Version des Experience-Snapshotformats, keine persistierte Steuerungsinformation, keine automatische Regelaktivierung und keine Änderung der Domainsemantik von Revoke oder Replace.
- Das Paket vereinheitlicht Transfer-, Gate- und Sessionpunkte nicht zu einem universellen Budgetmodell.

## Kompatibilitätsregeln

1. Historische Revoke-/Replace-Aufrufe ohne die beiden Optionen bleiben byte- und formatkompatibel.
2. Vorhandene Experience-Snapshots bleiben unverändert lesbar; erfolgreiche kontrollierte und unkontrollierte Aufrufe erzeugen bei gleichen Eingaben dieselben Snapshotbytes.
3. Der neue Erfolgsmantel erscheint nur bei expliziter Steueranforderung.
4. Das Konto wird weder in einen Snapshot geschrieben noch zwischen CLI-Aufrufen geteilt.
5. Die bestehenden Steuerbefehle und ihre Reihenfolge aus Signal-, Budget- und Dateiüberprüfungen bleiben unverändert.

## Akzeptanzkriterien für die spätere Umsetzung

- Ohne Steueroptionen liefern Revoke und Replace exakt die bisherigen Berichte und Snapshotbytes.
- Mit einem ausreichend hohen Limit sind nativer Bericht und erzeugter Snapshot identisch zum unkontrollierten Weg; stdout ist zusätzlich ein valider `controlled-transfer-v1`-Mantel mit tatsächlichem Verbrauch.
- Für beide Aktionen gelingt das exakt benötigte Budget; ein Punkt weniger und Budget 0 scheitern mit Exit 2, ohne veröffentlichtes Ziel oder temporären Rest.
- Ein zugestelltes SIGINT oder SIGTERM an einer erreichbaren Kontrollstelle ergibt `TransferCancelled`, ebenfalls ohne Ziel oder temporären Rest.
- Eine Basisdatei wird bei Budget 0 nicht geöffnet; fachliche Fehler und ein schon vorhandenes Ziel behalten ohne Steuerung sowie mit hohem Budget ihren bisherigen Fehlervertrag.
- `experience inspect` und die übrigen ausgeschlossenen Aktionen lehnen die neuen Optionen weiterhin als unbekannt ab.
- Tests decken optionale und fehlende Basis, beide mutierenden Aktionen, einen fehlerhaften Ersatzlauf, ein vorhandenes Ziel sowie Ausgaben- und Fehlkanaltrennung ab. Die gesamte CTest-Suite bleibt erfolgreich.

## Rückfallstrategie

Die Erweiterung ist rein additiv. Bei einem Fehler kann die neue Optionsbehandlung entfernt oder deaktiviert werden; die bisherigen Revoke-/Replace-Aufrufe, bestehende Snapshots und alle anderen Steuerungswege bleiben unverändert. Es existiert keine Migration oder persistierter Schalter, der zurückgerollt werden müsste.

## Weiterer Ablauf

Nach fachlicher Freigabe dieser Spezifikation wird ein kleinteiliger Umsetzungsplan erstellt. Er beginnt mit fehlschlagenden CLI- und Regressionstests für beide Aktionen und zieht keine weiteren Verwaltungswege in dieses Paket.
