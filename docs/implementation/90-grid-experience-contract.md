# Grid-Forschung als Erfahrung – Vertrag V1

Core 0.43 verbindet die bestehenden Grid-Forschungsbundles mit dem begrenzten Erfahrungsspeicher. Die CLI bleibt `experience add --from-run QUELLE --output SNAPSHOT`. Zulässig sind GridResearch V1 (X), V2 (Y) und V3 (gemeinsamer logischer Rahmen), deren innerer Lauf bereits als unabhängige Portfolioquelle zulässig ist.

## Zulassung und Herkunft

Vor der inneren Forschungsausführung wird deren Bundleversion gegen die bestehende Liste unabhängiger Drei-, Vier- und Fünferportfolios geprüft. Erfahrungsgeleitete Portfolios und Einzelstrategien bleiben ausgeschlossen. Anschließend werden Gridquellen, Samplebindung, innerer Lauf und vollständiger Wrapper replaygeprüft. Maximal eine Million deklarierte Forschungsoperationen sind zulässig; der Parser prüft dieses Limit vor Ausführung der Forschung. Ein eventuell engerer V3-Forschungsrahmen bleibt wirksam.

Nur abgeschlossene Portfolios mit Holdout und mindestens einer auf Samples bestätigten Hypothese erhalten Stimmen. Unvollständige oder ungültige Quellen bleiben Fehler ohne Export. Gridquellen dürfen vollständig fortgesetzte Stapel enthalten; deren bisherige Rekursions- und Bytegrenzen bleiben erhalten.

Der gespeicherte Quellblock enthält das gesamte Grid-Forschungsbundle. `source_digest` identifiziert genau diese Quelle, nicht nur die innere Forschung. Der Snapshot bleibt daher ohne ursprüngliche Grid-/Fortsetzungs-/Forschungsdateien prüfbar.

## Identität und Stimmen

Die bestehenden Kontextschlüssel und nach Lernpaaren sortierten Fingerprints bleiben erhalten. Identische Paare werden auch zwischen CSV- und Gridherkunft dedupliziert. Andere Gridpfade oder eine andere Zeilenreihenfolge erzeugen keine zusätzliche Stimme für dieselben Lernpaare. Der Herkunftstyp beeinflusst die Rangfolge nicht; die bisherige Übereinstimmung von Suchkonfiguration und Datenform bleibt erforderlich.

Ein Datensatz liefert höchstens eine Stimme je erfolgreicher Familie. Widerruf deaktiviert ihn für künftige Planung, erhält aber seine Herkunft. Lernen aus schon erfahrungsgeleiteten Läufen bleibt ausgeschlossen. LearnGate und PublishGate bleiben geschlossen; die Statistik bestätigt keine allgemeine mathematische Wahrheit.

## Format und Kosten

Enthält ein Snapshot mindestens einen Grid-Datensatz, verwendet er `BRUCE_EXPERIENCE_V12` mit `routing-experience-v12`. Diese Version bleibt auch nach dessen Widerruf bestehen. Die Datensatzanzeige ergänzt für jeden Eintrag `grid_origin`. Gemischte Snapshots sind erlaubt. Ohne Grid-Datensatz bleiben bisherige Versionen und Ausgaben unverändert.

`audit_operations` addiert für Grid-Datensätze die bisherige Forschungs-/Formanalysearbeit und die vollständigen Trainings-/Holdout-Quellbytes als logische Importkosten. Das sind keine gemessenen CPU-Schritte. Insbesondere werden rekursive Grid-Prüfung und Cacheaufbau nicht als einzelne Potenzberechnungen in diesen Zähler übertragen. Die spätere Forschung zeigt diese Bilanz separat als `experience_audit_operations`; sie wird nicht vom Suchbudget abgezogen. Die Snapshotprüfung erfolgt vor der eigentlichen Suchplanung. Bis zu 1 MiB Snapshot/Quelle, acht Datensätze und 16 Ereignisse bleiben gültig.

## Grenzen

Es handelt sich um erfahrungsbasierte Reihenfolgeplanung, nicht um allgemeines Lernen oder automatische Regelpromotion. Externe Evidenz, weitergehende Korrekturketten, allgemeine Strategie-/Gate-Verträge und fortsetzbare Formelforschung bleiben offen.

[Benutzung](91-grid-experience-usage.md) · [Abnahme](acceptance/grid-experience-v1-2026-09-22.md)
