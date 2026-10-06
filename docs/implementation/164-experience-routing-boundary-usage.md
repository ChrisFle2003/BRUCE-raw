# Core 0.84: Erfahrungsrouting in der Anwendungsschicht

Die vorhandene Funktion `rank_experience` liegt jetzt in `application/experience_routing.cpp` und wird über `application/experience_routing.hpp` deklariert. Die Portfolioplanung verwendet diesen Einstieg. Der Storeheader enthält keine Rankingdeklaration oder Referenzen auf Forschungs-/Planungstypen mehr.

Die bestehenden Funktionen für Trainingsfingerprint und Routingkontext wurden unverändert als `experience_training_key` und `experience_context_key` in denselben Anwendungsbaustein verlagert. Quellenprüfung und Ranking verwenden damit weiterhin eine gemeinsame Schlüsselableitung. Versionstexte, Hashinhalte, Reihenfolge bei Gleichstand, Stimmenzählung und Budgetschritte bleiben erhalten.

Die Storeimplementierung benötigt wegen des Quellenaudits weiterhin Forschungs-, Replay- und Planungskomponenten. Es handelt sich um eine Trennung der Routingzuständigkeit und öffentlichen Schnittstellen, noch nicht um eine vollständig gerichtete, unabhängige Persistenzschicht.

Externe C++-Aufrufer von `rank_experience` müssen nun `bruce/application/experience_routing.hpp` einbinden. CLI-Aufrufe, Snapshotformate, Replayverträge und Berichtschemas bleiben unverändert.

Tests prüfen direkte Stimmenauswertung mit stabiler Reihenfolge bei Gleichstand, exakte Prüfkosten dieses Beispiels und Ausschluss derselben Trainingseingabe. Die vorhandenen Experience-/Portfolio-/Replaytests prüfen die historisch unterstützten Kontexte und Familien. Die Headerprüfung bleibt erhalten.

Keine Regel, kein Gewicht und keine Freigabe wurde geändert. Die vorhandene Routingfunktion wird nur anders zugeordnet; sie aktiviert keine neuen Regeln. Die menschliche Regelhoheit nach ADR-013 bleibt verbindlich, ihre geschützte technische Freigabeverwaltung weiterhin offen.

Die weitere Bereinigung betrifft Quellenaudit-Abhängigkeiten sowie fachliche Regeln und ihre Darstellungshelfer. Blueprint-Paket B ist damit weitergeführt, aber noch nicht insgesamt abgeschlossen.

## Prüfnachweis am 25.09.2026

Lokal und remote bestanden 109/109 Testgruppen. Der Nutzerpfad `./build/bruce` auf Bruce-AI wurde als 0.84.0 gebaut und geprüft. Installation und TGZ-Paketprüfung bestanden; die CLI-Prüfung bestand lokal und remote mit 92 Beispielaufrufen und gültigen internen Links. Quellen und Dokumentation wurden per SCP übertragen.
