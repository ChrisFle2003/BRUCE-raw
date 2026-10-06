# Aktive lokale Regeln – Core 0.102.0

[Handbuch](README.md) · [Einzeldatei-Export](rule-review.md) · [Regelsyntax](formats.md#regeldatei-bruce_rules_v1)

## Ablauf

Ein Regelspeicher ist eine ausdrücklich angegebene lokale Datei. Sie enthält sämtliche bestätigten Versionswechsel und daraus ableitbar die aktive Fassung. Es gibt keinen automatisch ausgewählten Standardspeicher und keinen Hintergrunddienst.

```text
bruce rules-stage STORE NEW PROPOSAL
bruce rules-activate STORE PROPOSAL
bruce rules-status STORE
bruce compatibility --rule-store STORE --candidates CANDIDATES [bestehende Kompatibilitätsoptionen]
```

`rules-stage` liest NEW als gültige `BRUCE_RULES_V1`-Datei (maximal 1 MiB) und erzeugt exklusiv PROPOSAL. STORE wird gelesen und geprüft; falls noch nicht vorhanden, dient `BRUCE_RULES_V1\nsnapshot uninitialized\n` als leere Basis der Generation 0. Die Speicherdatei wird erst bei bestätigter Aktivierung angelegt. Kanonisch gleiche Regeln einschließlich Snapshotkennung werden als `RuleProposalUnchanged` abgelehnt. Erfolg: `{"staged":true,"approved":false}`, Exit 0.

`rules-activate` erwartet einen mit `rules-stage` erzeugten Vorschlag. Es prüft Ziel, Journalstand und Ausgangsfassung, zeigt alte und neue Regeln sowie den aufgelösten Speicherpfad und den Generationswechsel. stdin und stdout müssen Terminals sein. Nur die exakt angezeigte Zeile `CONFIRM <Digest>` stimmt zu; jede andere Eingabe oder Dateiende ergibt `RuleApprovalDeclined`, Exit 2.

Nach Zustimmung erhält der Prozess eine nicht wartende exklusive Schreibsperre. Er liest STORE erneut und prüft die Basis nochmals. Ein erfolgreicher Wechsel schreibt Journal und aktive Fassung gemeinsam durch atomaren Dateiersatz. Anschließend erscheint nach dem Dialog derselbe JSON-Bericht wie bei `rules-status`. Vorschlag und NEW werden nicht gelöscht. Keine Ein-/Ausgabeumleitung bei Aktivierung verwenden; die anderen Befehle liefern reines JSON.

`rules-status` verlangt eine vorhandene, nichtleere Speicherdatei und prüft das vollständige Journal. Es ist keine bloße Headerabfrage. Fehler und manipulierte Historien werden zurückgewiesen. Alle drei Befehle verwenden ausschließlich die gezeigten Positionsargumente; keine zusätzlichen Optionen, Sitzungsbudgets oder automatische Zustimmung.

## Beispiel

Vom Projektstamm im Terminal ausführen; das temporäre Verzeichnis ist ein eigener Testspeicher:

```bash
rules_dir=$(mktemp -d)
printf 'BRUCE_RULES_V1\nsnapshot erste-version\nrule positiv positive value ge 0 0 1 manual -\n' > "$rules_dir/neu.rules"
./build/bruce rules-stage "$rules_dir/aktiv.store" "$rules_dir/neu.rules" "$rules_dir/erste.stage"
./build/bruce rules-activate "$rules_dir/aktiv.store" "$rules_dir/erste.stage"
```

Prüfe beide Fassungen und bestätige nur bei Zustimmung die angezeigte Zeile. Danach:

```bash
./build/bruce rules-status "$rules_dir/aktiv.store"
printf 'id,domain,layer,x,y,value\n1,demo,0,2,6,64\n' > "$rules_dir/kandidaten.csv"
./build/bruce compatibility --rule-store "$rules_dir/aktiv.store" --candidates "$rules_dir/kandidaten.csv" --output "$rules_dir/lauf.bruce"
./build/bruce replay "$rules_dir/lauf.bruce"
```

Für weitere Änderungen NEW bearbeiten, einen neuen Vorschlag unter einem neuen Dateinamen erstellen und ausdrücklich bestätigen. Zurücksetzen funktioniert genauso: Eine ältere gewünschte Fassung wird als NEW gegen die aktuelle Basis vorgeschlagen. Das erhöht die Generation; es reaktiviert keine alte Zustimmung.

## Laufbindung und Reichweite

`compatibility` akzeptiert genau einen Regelweg: `--rules DATEI` oder `--rule-store STORE`. Beide gleichzeitig ergeben `UseRulesOrRuleStore`. Ohne beide bleibt `--rules` Pflicht. `--rule-store` funktioniert auch mit `--from-run` und den bestehenden Sitzungs-/Exportoptionen.

Der Speicher wird einmal vor Auftragsausführung geprüft. Die dabei aktive kanonische Fassung wird in den Auftrag übernommen. Ein späterer Wechsel beeinflusst diesen Lauf nicht. Ergebnis-JSON enthält die vorhandenen Felder `snapshot_id` und `snapshot_digest`; exportierte Bundles bewahren die vollständigen Regelbytes. Replay verwendet diese eingebettete Fassung ohne den aktuellen Speicher zu lesen.

Der Lauf bindet damit den **Regelinhalt**, nicht den Journalpfad oder die Aktivierungsgeneration. Bei Rückkehr zu identischen Regeln ist der Digest wieder identisch. Die Generation und die bestätigten Wechsel stehen im Journal. Eine neue Bundle-Provenienz mit eingebettetem Journal ist nicht implementiert.

Die Verwaltung gilt für die numerischen Kompatibilitäts-Snapshots. Sie verwaltet keine eingebauten Forschungs-Gates, Sprachregeln oder sämtliche zukünftigen Regelarten. Bestehende `--rules`-Aufrufe und `rules-propose`/`rules-approve` bleiben als direkte Dateiwege verfügbar. Das ist kein global erzwungener Freigabenachweis für sämtliche CLI-Aufrufe.

## Bericht `rule-history-v1`

| Feld | Bedeutung |
| --- | --- |
| `target` | Aufgelöster absoluter Speicherpfad |
| `generation` | Anzahl bestätigter Wechsel; erste Aktivierung ist 1 |
| `head` | Hash des bisherigen Journalverlaufs, einschließlich Zeitangaben |
| `active_digest` | SHA-256 der aktiven kanonischen Regelbytes |
| `active_rules` | Vollständige aktive kanonische Fassung |
| `decisions` | Bestätigte Wechsel in aufsteigender Generation |
| `decisions[].generation` | Durch den Wechsel erreichte Generation |
| `unix_time` | Lokale Systemzeit bei Bestätigung, ganze Sekunden seit Unix-Epoche; kein vertrauenswürdiger Zeitstempel |
| `decision` | Immer `approved`; abgelehnte Dialoge werden nicht gespeichert |
| `proposal` | Digest des eingebetteten alten/neuen Regelvorschlags |
| `confirmation` | Beim Wechsel verlangter Bestätigungsdigest |
| `previous_digest`, `active_digest` | Alter und neuer Inhaltsdigest des einzelnen Wechsels |
| `previous_rules`, `proposed_rules` | Beide vollständigen Fassungen zum Nachvollziehen der Änderung |

Die Generation bestimmt die Reihenfolge; eine verstellte Systemuhr kann nichtmonotone Zeiten erzeugen. Digest und Journal sind keine Signaturen oder Nachweise menschlicher Identität.

## Speicherformat und Schreibvertrag

`BRUCE_RULE_STAGE_V1` ist ein SHA-256-umschlossener Textkörper aus Versionszeile, gequotetem Zielpfad, gequotetem bisherigem Journalhash, Ausgangsgeneration und gequoteten `BRUCE_RULE_PROPOSAL_V1`-Paketbytes. Jede Komponente endet mit LF. Es gilt C++ `std::quoted`, nicht JSON-Quoting. Der Bestätigungsdigest ist SHA-256 über `BRUCE_ACTIVATE_V1` + LF + vollständige Stagebytes.

`BRUCE_RULE_HISTORY_V1` besitzt ebenfalls eine äußere SHA-256-Prüfsumme. Der Körper enthält die Versionszeile, den gequoteten Zielpfad und pro bestätigtem Wechsel eine Zeile aus gequoteten Stagebytes, einem Leerzeichen und der dezimalen Unixzeit. Eingebettete Zeilenumbrüche bleiben erhalten. Die aktive Fassung wird aus dem letzten geprüften Eintrag gewonnen; kein zweiter aktiver Zeiger kann zum Journal auseinanderlaufen.

Die initiale Journalbindung ist SHA-256 über `BRUCE_RULE_HISTORY_V1` + LF + Zielpfad. Pro Eintrag wird SHA-256 über bisherigen Hash + LF + serialisierte Eintragsbytes gebildet. Jeder Stageeintrag muss die zuvor erreichte Bindung und Generation nennen und an die vorherige Regelfassung anschließen. Damit werden überholte Vorschläge auch nach einem Rückwechsel abgewiesen.

Grenzen: maximal 128 bestätigte Wechsel und 8 MiB serialisierte Historie; Stageimporte maximal 8 MiB, darin gelten die bisherigen 3-MiB-Vorschlags- und 1-MiB-Snapshotgrenzen. Keine automatische Rotation oder Löschung. Erreichte Grenzen führen zur Ablehnung ohne Versionswechsel. Aufrufe haben kein universelles Zeit-/RAM-Budget.

Der Dateiadapter löst den Pfad vor der Verwendung auf. Das Journal ist an diesen Pfad gebunden; ein Verschieben/Kopieren an einen anderen Pfad wird nicht als Migration akzeptiert. STORE-Elternverzeichnis muss existieren. `STORE.lock` wird beim ersten bestätigten Schreibversuch angelegt und bleibt bestehen. Die Sperrdatei während des Betriebs nicht entfernen oder ersetzen.

Unter dieser Sperre wird die aktuelle Datei erneut geprüft. Die neue vollständige Datei wird im selben Verzeichnis temporär angelegt (private Rechte), geschrieben, mit `fsync` synchronisiert, per `rename` ersetzt und anschließend das Elternverzeichnis synchronisiert. Leser sehen die vorherige oder neue vollständige Datei. Die Sperre ist kooperativ; andere Programme können sie ignorieren. Prozessabbruch vor dem Ersatz kann eine temporäre Datei hinterlassen. Nach dem Ersatz ist der Wechsel erfolgt, auch wenn der Prozess vor seiner Erfolgsausgabe beendet wird: dann `rules-status` prüfen.

## Fehler

Zusätzlich zu den bisherigen Datei-, Snapshot-, Vorschlags- und Terminalfehlern:

| Fehler | Bedeutung |
| --- | --- |
| `RuleStoreTargetMismatch` | Stage oder Journal gehört zu einem anderen Speicherpfad beziehungsweise hat nicht die erwartete Historyversion |
| `RuleStoreBaseChanged` | Journalhash oder Generation ist nicht mehr aktuell; neuen Vorschlag erstellen |
| `RuleBaseChanged` | Eingebettete alte Regelfassung passt nicht zur Journalbasis |
| `RuleHistoryDigestMismatch` | Äußere Prüfsumme eines Stage-/Historypakets ungültig |
| `InvalidRuleStage`, `InvalidRuleHistory` | Struktur oder kanonische Serialisierung ungültig; auch leere vorhandene Historien sind ungültig |
| `RuleHistoryDecisionLimit` | 128 bestätigte Wechsel bereits vorhanden |
| `RuleHistoryByteLimit`, `InputByteLimit` | Paket-/Dateigröße überschritten |
| `RuleStoreBusy` | Schreibsperre nicht sofort verfügbar; Zustand prüfen und gegebenenfalls erneut starten |
| `RuleStoreFileOpenFailed` | Sperrdatei, temporäre Datei oder Elternverzeichnis nicht geöffnet |
| `RuleStoreWriteFailed`, `RuleStoreSyncFailed`, `RuleStoreReplaceFailed` | Schreiben, Dateisynchronisierung oder atomarer Ersatz fehlgeschlagen |
| `RuleStoreCommitDurabilityUnknown` | Ersatz ist bereits erfolgt, aber die Verzeichnissynchronisierung scheiterte; nicht blind wiederholen, zuerst Status prüfen |
| `RuleStoreInvalidClock` | Systemzeit vor Unix-Epoche; Uhr prüfen |
| `UseRulesOrRuleStore` | Beide Regelquellen zugleich angegeben |

Normale Fehler liefern Exit 2 und JSON auf stderr. Bei Fehlern nach dem Dialog steht auf stdout bereits Klartext. Ein Fehler nach erfolgreichem Ersatz ist kein Beleg für einen unveränderten Speicher. Ein Prozesssignal besitzt hier keinen speziellen Teilbefundvertrag.

Seit Core 0.97 bindet die optionale [Transfersteuerung](transfer-control.md) bei `compatibility-plan` und `compatibility-apply` auch das Lesen und Prüfen des aktiven Regelspeichers ein. Freigabe und Aktivierung werden dadurch nicht geändert.
