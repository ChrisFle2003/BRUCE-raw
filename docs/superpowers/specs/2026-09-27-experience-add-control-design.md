# Design: kontrolliertes `experience add`

## Status

Akzeptierte Architekturentscheidung, 27.09.2026. Diese Spezifikation autorisiert noch keine Implementierung. Ein separater Umsetzungsplan und dessen Freigabe bleiben erforderlich.

## Ziel

Der vorhandene Befehl

```text
bruce experience add --from-run RUN [--store STORE] --output NEW
```

erhält optional dieselbe begrenzte Ausführungssteuerung wie die bereits vorhandenen vier expliziten Transferbefehle. Damit werden Dateilesen, Quellen-Audit, Snapshot-Aufbau und Exportvorbereitung beobachtbar abbrechbar, ohne die bestehende Fachsemantik oder den Regelaktivierungspfad umzubauen.

## Nachweisbarer Ausgangspunkt

- `ExperienceAddTask` enthält einen bisherigen Snapshot und einen Forschungsrun; `execute_experience` nimmt bereits einen optionalen `ReplayCheckpoint` entgegen.
- Die aktuelle CLI liest beide Dateien ohne solches Konto, ruft `execute_experience` auf und exportiert exklusiv.
- `TransferControl` prüft Signal vor Budget und zählt danach einen Kontrollpunkt. Die bestehenden Transferbefehle verwenden dessen Erfolgshülle `controlled-transfer-v1`.
- `LocalFiles` unterstützt bereits begrenztes Lesen mit Kontrollpunkt und exklusiven Export mit Kontrollpunkt.

Quellen: `include/bruce/application/experience_task.hpp`, `src/application/experience_task.cpp`, `apps/bruce_cli.cpp`, `include/bruce/application/transfer_control.hpp`, `src/io/local_files.cpp`.

## Entscheidung

### Öffentliche Erweiterung

Nur für `experience add` werden zwei optionale Paare zugelassen:

```text
--max-transfer-checkpoints N
--transfer-cancellation none|checkpoints
```

Sie verwenden das vorhandene Modell `transfer-checkpoints-v1`. Das Konto beginnt pro CLI-Aufruf bei null. Es misst ausschließlich erreichte Kontrollstellen, keine CPU-Operationen, Laufzeit, RAM oder eine maximale Signallatenz.

Bei mindestens einer dieser Optionen wird eine erfolgreiche Antwort so umschlossen:

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

Ohne die neuen Optionen bleiben Argumente, stdout-Erfolgsausgabe, Fehlerhülle, Exitcode und erzeugte Snapshot-Bytes unverändert.

### Kontrollierter Bereich

Der optionale Callback wird weitergereicht an:

1. das begrenzte Lesen von `--store`, wenn angegeben;
2. das begrenzte Lesen von `--from-run`;
3. `execute_experience` einschließlich Quellen-Audit, Snapshot-Aufbau und Snapshot-Inspektion;
4. die Vorbereitung und den exklusiven Export des neuen Snapshots bis unmittelbar vor Veröffentlichung.

Die Veröffentlichung folgt dem bestehenden `LocalFiles::publish_new`-Vertrag: vorhandene Ziele werden nicht ersetzt. Ein erkannter Callbackabbruch vor Veröffentlichung räumt die temporäre Datei auf, soweit das Dateisystem dies zulässt.

### Fehler- und Abbruchvertrag

- Bei einem gemeinsamen Kontrollpunkt gilt unverändert: Signalabbruch (`TransferCancelled`) hat Vorrang vor Budgeterschöpfung (`TransferCheckpointBudgetExceeded`).
- Ein Steuerungsfehler beendet den Aufruf mit Exit 2 und der bestehenden einfachen Fehlerausgabe auf stderr. Es gibt keinen fachlichen Teilbericht auf stdout.
- Fachliche Resultate wie eine abgelehnte oder ungeeignete Experience-Quelle bleiben fachliche Fehler nach ihrem bisherigen Vertrag; sie werden nicht als Budget- oder Signalereignis umgedeutet.
- Der neue Erfolgsmantel wird nicht für Fehler erzeugt.

## Bewusst nicht enthalten

- Keine Änderung von `experience revoke`, `experience replace` oder `experience inspect`.
- Keine Änderung von `rules-stage`, `rules-activate`, `rules-status`, `rules-propose` oder `rules-approve`.
- Keine Änderung der acht `ApplicationTask`-Varianten, ihrer Sessionphasen oder Fehlerhüllen.
- Keine Vereinheitlichung von Gate-, Session- und Transferpunkten zu einer universellen Einheit.
- Keine neue Berechtigung, keine automatische Regelaktivierung und keine Änderung von Bundleformaten.

## Kompatibilitätsregeln

1. Historische Aufrufe ohne die beiden neuen Optionen bleiben byte- und formatkompatibel.
2. Historische Snapshotformate bleiben unverändert lesbar.
3. Die neue Erfolgshülle erscheint ausschließlich bei explizit angeforderter Steuerung.
4. Bereits bestehende Transferbefehle, deren Reihenfolge von Signal-, Budget- und Dateiprüfung, bleiben unverändert.
5. Die neue Erweiterung verwendet keine implizite Weitergabe eines verbrauchten Kontos an spätere Aufrufe.

## Akzeptanzkriterien für die spätere Umsetzung

- Der unveränderte Befehl ohne Steueroptionen erzeugt zum bisherigen Weg bytegleiche Snapshot-Ausgaben und denselben Bericht.
- Ein ausreichend hohes Limit erzeugt denselben Snapshot wie der unkontrollierte Weg und zusätzlich einen korrekten `controlled-transfer-v1`-Bericht.
- Das exakt benötigte Limit gelingt; ein um eins kleineres Limit scheitert mit Exit 2, ohne Ziel oder temporären Rest.
- Ein zugestelltes SIGINT oder SIGTERM an einer erreichbaren Kontrollstelle ergibt `TransferCancelled`, ebenfalls ohne Ziel oder temporären Rest.
- Die Tests decken beide Eingabeformen (mit und ohne bisherigen Store), eine ungeeignete Quelle, ein schon vorhandenes Ausgabeziel und einen fehlerhaften Snapshot ab.
- Die gesamte vorhandene CTest-Suite bleibt erfolgreich; die zusätzlichen Tests werden als Unit- und CLI-Integrationstests registriert.

## Rückfallstrategie

Die Erweiterung ist additiv. Bei einem Fehler kann sie entfernt oder hinter den neuen Optionen deaktiviert werden; der alte `experience add`-Aufruf, bestehende Snapshots und die übrigen Steuerungswege bleiben unverändert. Es gibt keine Migration und keinen persistierten Schalter, der zurückgerollt werden müsste.

## Weiterer Ablauf

Vor Implementierungsbeginn wird ein kleinteiliger Umsetzungsplan erstellt. Er muss zuerst eine fehlschlagende Testabdeckung für die neue Steuerung festlegen und darf keine weiteren Verwaltungswege in dasselbe Paket ziehen.
