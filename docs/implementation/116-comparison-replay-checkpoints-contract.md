# Unterbrechbarer Bruchvergleichsreplay – Vertrag V1

Core 0.56 ergänzt interne Replay-Prüfpunkte für den exakten Polynombruchvergleich. Unterstützt sind die nativen Formate V1/V2/V3 mit den Verfahren `quadratic`, `modular` und `modular-witness`. Mathematische Aussagen, Definitionsbereiche, Zertifikate und Operationszähler bleiben erhalten.

## Prüfablauf

`replay_fraction_comparison` nimmt einen optionalen `ReplayCheckpoint` an; `replay_application` reicht ihn bei Vergleichsbundles weiter. Prüfpunkte liegen nach erfolgreicher Digestprüfung, vor der Vergleichsausführung, an deren bestehenden budgetierten Rechenschritten und nach der Ausführung vor dem abschließenden Evidenzvergleich.

Bei normaler Callback-Rückkehr liefert der Observer an den nativen Algorithmus stets false. Er fordert damit keinen nativen Abbruch an und setzt keine neue `cancel_after_operations`-Grenze. Gespeicherte Grenzen bleiben wirksam und können weiterhin identische INCOMPLETE-Befunde reproduzieren. Ohne Prüfcallback bleibt auch der Observer leer.

Exceptions aus dem Observer werden durch die bestehende interne Weitergabehülle vor der numerischen Fehlerbehandlung geschützt. So können insbesondere `NumericLimit` und `ExpressionNormalizationLimit` aus dem Prüfcallback nicht als mathematischer Grenzfall verschluckt werden. Am Vergleichsausgang wird die ursprüngliche Exception wieder geworfen. Ein unterbrochener Replay gibt keinen Befund zurück und behauptet keine bestandene Evidenzprüfung.

## Sitzung und CLI

`VerificationControl.comparison_replay_checkpoints=true` aktiviert die internen Punkte und verlangt einen Prüfcallback sowie einen `FractionComparisonTask`. Mehrere innere Replaymodi gleichzeitig sind unzulässig. Die Typ- und Modusprüfung erfolgt vor der nativen Ausführung.

In der CLI gilt:

`--verification replay --verification-cancellation comparison-checkpoints`

SIGINT/SIGTERM werden an den Prüfpunkten als Abbruchanforderung ausgewertet. Ein erkannter Abbruch innerhalb des Replay führt zu `VerificationCancelled`, Phase REPLAYING und terminalem FAILED. Es gibt keine Freigabe und keinen Export. Bereits bestehende Zieldateien bleiben erhalten.

Erfolgreiche Sitzungen verwenden `application-execution-v6` und `verification_cancellation: "comparison-checkpoints"`. Die übrigen Erfolgsfelder und ein optionales Replay-Bytebudget bleiben bestehen. Native V1–V3-Bundlebytes und die früheren Sitzungsmodi V1–V5 bleiben unverändert. Die Einstellung wird nicht im nativen Bundle persistiert.

## Aussagegrenzen

FAILED bezeichnet hier die unterbrochene Prüfsitzung, nicht eine Widerlegung der verglichenen Brüche. Ein regulär budgetbegrenzter oder nativ abgebrochener Vergleich kann ohne aktuelle Prüfungsunterbrechung weiterhin als identischer Teilbefund geprüft werden.

Parsing der Koeffizienten, einzelne Zahlenoperationen, Hashing, Kopien, Payloadaufbau und Serialisierung bleiben atomar. Die vorhandenen Rechencheckpoints garantieren keine maximale Abbruchlatenz. Der eigenständige CLI-Befehl `replay` erhält keine neue Option. Z-Kompatibilität, Erfahrungs-Quellaudits und allgemeine Operationsbudgets bleiben weitere Arbeit; kein vollständiger Meilensteinabschluss.

[Benutzung](117-comparison-replay-checkpoints-usage.md) · [Abnahme](acceptance/comparison-replay-checkpoints-v1-2026-09-23.md)
