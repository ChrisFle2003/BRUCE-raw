# Blockweise Elternkopie bei Fortsetzungen / Core 0.67

## Vertrag

Beim Payloadaufbau von `pack_grid_resume` wird das vorhandene Elternbundle in Abschnitten von höchstens 65.536 Bytes angehängt. Der bisherige Records-Callback vor Header und Elterninhalt bleibt bestehen und gibt den ersten Abschnitt frei. Vor jedem weiteren Abschnitt wird derselbe Callback erneut aufgerufen. Es gibt keinen zusätzlichen Endcallback.

Die zusätzlichen Punkte betragen bei Elternlänge B: 0 für B=0, sonst `floor((B-1)/65536)`. Leere Elterninhalte sind nur ein Serializer-Grenztest, keine gültige Forschungs-/Fortsetzungsquelle. Bis einschließlich 65.536 Bytes bleibt die Callbackanzahl unverändert. Byteinhalte einschließlich NUL werden unverändert transportiert.

Vor dem Kopieren wird Speicher für Header und Elterninhalt reserviert; die Längensumme wird vor Addition geprüft. Die einzelnen Elternabschnitte benötigen damit während dieser Schleife keine erneute Kapazitätsvergrößerung. Speicherreservierung selbst bleibt atomar.

Eine geworfene Callbackexception verlässt den Packer unverändert. Ein Abbruch in der Kopierschleife erreicht weder die äußere Prüfsumme noch eine erfolgreiche Bundlerückgabe. Der lokale Teilaufbau wird verworfen. Native Bundleversion, Mathematik, Abbruchpositionen, Cache-/Auditkosten und Exportbytes ändern sich nicht.

## Gemeinsame Steuerung

Die vorhandene Option `--serialization-cancellation grid-record-checkpoints` erreicht die zusätzlichen Punkte automatisch. Das seit Core 0.65 optionale `--max-session-checkpoints` zählt sie ebenfalls automatisch in SERIALIZING, ohne einen zusätzlichen Signalmodus zu benötigen. Überschreitung führt zu FAILED ohne Ergebnisfreigabe/Export.

Sitzungsberichte behalten V9 beziehungsweise V14 und das Modell `session-checkpoints-v1`. Dieses Modell zählt tatsächlich erreichte Kontrollstellen, keine versionsübergreifend konstante Operationszahl. Bei großen Elternbundles kann ein in Core 0.66 ausreichendes Prüfpunktlimit deshalb in 0.67 zu klein sein. Ausführungs- und Replayanteile bleiben durch diese Änderung unverändert. Die Budgets gehören weiterhin nicht zum mathematischen Bundle.

## Grenzen

Nur diese konkrete Elternkopie im Fortsetzungs-Payload wird erweitert. Andere Payloadfamilien, einzelne große Pfade/Trace-/Ergebniszeilen, weitere Elternkopien bei anderen Abläufen, JSON-Aufbau und abschließender Payload-/Bundlezusammenbau bleiben teilweise atomar. Auch die callbackfreie Payloadrekonstruktion beim Replay erhält dadurch keine zusätzlichen Replaykontrollpunkte. Keine maximale Signallatenz, keine Prozess-RAM-Grenze und keine allgemeine Forschungsfortsetzung.

[Benutzung](140-resume-copy-blocks-usage.md) · [Abnahme](acceptance/resume-copy-blocks-v1-2026-09-23.md)
