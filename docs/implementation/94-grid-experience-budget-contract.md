# Gemeinsamer Grid-/Experience-Importrahmen – Vertrag V1

Core 0.45 erweitert den optionalen Grid-Gesamtrahmen für `experience-order`. Bei `research` mit Gridquellen, Erfahrungsspeicher und `--max-total-operations` wird automatisch GridResearch V4 erzeugt. Ohne Erfahrung bleibt V3, ohne Gesamtrahmen bleibt das jeweilige bisherige X-/Y-Format.

## Logische Abrechnung

Die feste Policy heißt `grid-and-experience-bytes-plus-research-v1`.

- `source_operations`: vollständige Bytes von Trainings- und Holdout-Gridquelle, einschließlich eingebetteter Vorgeschichte.
- `experience_import_operations`: vollständige Bytes des Erfahrungssnapshots einschließlich inaktiver Datensätze und Korrekturketten.
- `research_limit`: Minimum aus angefordertem Suchbudget und Gesamtrahmen minus beide Importkosten.
- `used`: beide Importkosten plus tatsächlich gezählte neue Forschungsoperationen.

Gesamt- und angefordertes Suchbudget müssen zwischen 1 und 10 Millionen liegen. Der Gesamtrahmen muss größer als die Summe beider Importkosten sein, damit mindestens eine Suchoperation bleibt. Andernfalls wird vor dem Quellenreplay abgewiesen; kein Export entsteht. Die bisherigen Quelldateigrenzen und höchstens 1 MiB Experience bleiben wirksam.

Die Byteprüfung ist eine deterministische Zulassung, keine Vorhersage der tatsächlichen Prüfkosten. `experience_audit_operations` bleibt zusätzliche Information und wird nicht nochmals abgezogen. Hashing, Quellenreplay, Cacheaufbau, Snapshotprüfung, Dateiverarbeitung und Export werden nicht als gemessene CPU-Arbeit abgerechnet. Quellen- und Snapshotprüfung bleiben atomar. Dieses Paket schafft keinen allgemeinen CPU-/Zeit-/RAM-Gesamtrahmen.

## Export und Replay

`BRUCE_GRID_RESEARCH_BUNDLE_V4` speichert Policy, Gesamtbudget, ursprüngliches Suchbudget, Experience-Bytezahl und das bisherige X-/Y-Herkunftsbundle. Die äußere Bytezahl wird begrenzt und zur Berechnung des verbleibenden Suchrahmens verwendet. Vor Ausführung der inneren Forschung prüft der Decoder am geparsten Task, dass der Planer `experience-order` ist und die tatsächliche Snapshotlänge exakt übereinstimmt. Das deklarierte innere Operationslimit darf den zugelassenen Rahmen nicht überschreiten. Anschließend werden der kanonische innere und äußere Inhalt vollständig verglichen.

Replay erhält einen durch den Rest begrenzten Teilbefund einschließlich Abbruchposition. Vollständige V4-Läufe können an die numerische Z-Auswahl übergeben werden; Teilbefunde bleiben dafür ausgeschlossen. Der gesamte Export bleibt ohne Originaldateien replaybar.

V4 ist bereits erfahrungsgeleitet und deshalb keine neue unabhängige Experiencequelle. Ein Import über `experience add` oder als Ersatzquelle bleibt abgewiesen.

## Kompatibilität

Historische gelernte V3-Läufe zählen beim Replay weiterhin nur Gridquellbytes und neue Suche. Sie werden nicht nachträglich auf V4 umgedeutet. Neue gelernte Läufe mit Gesamtbudget verwenden V4. Neue Läufe ohne Erfahrung bzw. ohne Gesamtbudget behalten ihre bisherigen Formate und Ausgaben.

[Benutzung](95-grid-experience-budget-usage.md) · [Abnahme](acceptance/grid-experience-budget-v1-2026-09-22.md)
