# Experience-Korrekturketten – Vertrag V1

Core 0.44 ergänzt `experience replace`. Ein aktiver Datensatz wird durch einen neu geprüften Datensatz ersetzt. Die bisherigen unabhängigen Portfolioquellen einschließlich GridResearch V1/V2/V3 bleiben zulässig; unvollständige Läufe, fehlender Holdout, fehlende bestätigte Hypothesen und erfahrungsgeleitete Quellen bleiben unzulässig.

## Ein Ereignis, zwei Zustandsänderungen

Ein `replace`-Ereignis enthält Ziel-ID, nichtleeren Grund von höchstens 256 Bytes ohne Zeilenumbruch/NUL und das vollständige neue Quellbundle. Ziel und Vorfahren müssen im bisherigen Store liegen. Das Ziel muss aktiv sein. Erst nach Quellenprüfung und Deduplizierung wird es inaktiv und der Nachfolger aktiv. Die neue Datensatz-ID ist der Digest des Ersetzungsereignisses.

Die API arbeitet auf einem lokal eingelesenen Snapshot und liefert nur bei erfolgreicher Prüfung einen neuen Snapshot zurück. Die CLI exportiert erst nach dessen erneutem Einlesen und überschreibt keine Datei. Bei einem Fehler bleibt der vorherige Snapshot unverändert; es entsteht kein teilweise korrigierter Export.

## Herkunft und Deduplizierung

Jeder Nachfolger speichert `replaces` als Vorgänger-ID. Der Korrektureintrag verknüpft `target`, `reason` und `replacement`. Ein regulärer Widerruf hat keinen Nachfolger. Alle früheren Quellbundles bleiben in der Ereigniskette enthalten und werden beim Einlesen erneut geprüft.

Gleiche Lernpaare sind bei einer expliziten Ersetzung innerhalb der Vorgängerkette erlaubt, etwa bei zusätzlichem oder geändertem Holdout. Gleiche Fingerprints außerhalb dieser Kette bleiben unzulässig, auch bei dort bereits widerrufenen Datensätzen. Neue Lernpaare sind ebenfalls erlaubt; die spätere Nutzung muss weiterhin den neuen Kontextbedingungen entsprechen. Eine byteidentische direkte Ersatzquelle wird abgewiesen. Eine Rückkehr zu einer früheren, weiterhin zulässigen Quelle ist als neues begründetes Ereignis möglich und reaktiviert keine alte Stimme.

Bei der Rangfolge zählen ausschließlich aktive Datensätze. Mehrfaches Ersetzen vervielfacht daher keine Stimmen. Ein inaktiver Vorgänger kann nicht nochmals ersetzt oder widerrufen werden. Der aktuelle Nachfolger lässt sich normal widerrufen oder weiter ersetzen.

## Format und Grenzen

Stores mit einem Ersetzungsereignis verwenden `BRUCE_EXPERIENCE_V13` und `routing-experience-v13`. Ihre Anzeige ergänzt `replaces` für Datensätze und `replacement` für Korrekturen; fehlende Verknüpfungen sind leere Strings. Vorhandene Grid-Markierungen bleiben erhalten. Ohne Ersetzung bleiben bisherige Formate und Anzeigen unverändert.

Die bisherigen Grenzen bleiben bestehen: 1 MiB, acht historische Datensätze und 16 Ereignisse. Jede Ersetzung verbraucht einen neuen historischen Datensatz und ein Ereignis; alte Datensätze werden nicht gelöscht. Auditkosten enthalten weiterhin auch die inaktiven historischen Quellen und bleiben separat vom Suchbudget. Replay prüft sämtliche Quellen, Verknüpfungen und kanonische Ereigniskette.

## Aussagegrenze

Die Korrektur dokumentiert einen begründeten Austausch von Routing-Evidenz. Sie beweist weder, dass der alte mathematische Befund falsch war, noch dass der Nachfolger allgemein wahr ist. Externe Evidenz, automatische Regelpromotion und allgemeines Lernen bleiben offen.

[Benutzung](93-experience-replacement-usage.md) · [Abnahme](acceptance/experience-replacement-v1-2026-09-22.md)
