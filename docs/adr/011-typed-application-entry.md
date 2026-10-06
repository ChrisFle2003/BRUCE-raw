# ADR-011: Typisierter gemeinsamer Auftragseinstieg

## Status

Akzeptiert und implementiert mit Core 0.47 im freigegebenen Architektur-Konsolidierungsschwerpunkt.

## Kontext

Die CLI rief acht verschiedene Executor-/Darstellungs-/Exportkombinationen direkt auf. Gemeinsame Aufrufstellen für künftige Oberflächen fehlten. Zugleich unterscheiden sich Parameter, Budgeteinheiten, Status und Abbruchpositionen fachlich; ein untypisiertes Universalobjekt würde diese Grenzen verdecken.

## Entscheidung

`ApplicationTask` ist eine geschlossene C++-Variante aus den bestehenden Int64- und Exact-Forschungsaufträgen, Grid, Grid-Stapel, Bruchvergleich und Kompatibilität sowie kleinen Requesttypen für Grid-Fortsetzung und Grid-Forschung. `ApplicationRun` hält den jeweils fachlich passenden Ergebniswert. `execute_application`, `render_application` und `pack_application` bieten gemeinsame Aufrufpunkte.

Die Adapter delegieren an bestehende Fachfunktionen. Sie führen keine zweite Suche aus und fügen weder Felder noch Hüllen zu bestehenden Exporten hinzu. Ein unbekannter Auftragstyp ist kein gültiger Variantenwert; eine neue Alternative benötigt kompilierbare Executor- und Codeczuordnung. Die bestehenden fachlichen Validatoren bleiben verpflichtend.

`ExecutionControl` enthält ein Abbruchsignal ohne Arbeitszähler. Jeder Executor prüft es an seinen bisherigen lokalen Checkpoints und speichert die fachliche Abbruchposition wie zuvor. Ein fehlender Callback bleibt wirklich fehlend, da Executorverhalten diesen Unterschied berücksichtigen kann. Grid-Einzelauswertung bleibt atomar. Quellenprüfung/Cacheaufbau werden durch den gemeinsamen Einstieg nicht unterbrechbar.

Alle acht CLI-Laufpfade verwenden diese Schnittstelle. Dateilesen und exklusiver Export bleiben Verantwortung der CLI; die Schnittstelle serialisiert nur. Bestehende Replaydecoder bleiben zuständig. Experience-Verwaltung, Koordinatenkonvertierung und Taschenrechnerfunktionen bleiben außerhalb dieses Laufvertrags.

## Folgen und Grenzen

Künftige Oberflächen können typisierte Laufaufträge ohne acht eigene Executor-/Codecverknüpfungen ausführen. Native Ergebnistypen und ihre Aussagegrenzen bleiben sichtbar. Das ist kein generischer JSON-Auftrag, kein Netzwerkdienst, keine dynamische Pluginregistrierung und keine vereinheitlichte Budget-/Gate-Zustandsmaschine. Der gemeinsame Einstieg ist sequenziell und besitzt keine Hintergrundjobs.

[Plan](../implementation/98-application-task-plan.md) · [Benutzung und Abgleich](../implementation/99-application-task-usage.md)
