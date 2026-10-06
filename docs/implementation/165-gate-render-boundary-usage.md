# Core 0.85: Forschungsregeln unabhängig von JSON

Die fachliche Gateauswertung und die Benennung ihrer Gründe bleiben in `research_gates.cpp`. Die JSON-Ausgabe des Katalogs liegt nun in `research_gate_render.cpp`. Damit benötigt die Entscheidungsimplementierung weder den Forschungsheader noch `json_string`.

`research_gate_completed_statuses()` liefert einen schreibgeschützten Span auf die bisherige feste Abschlussstatusliste. Auswertung und Renderer nutzen denselben Bestand. Diese Schnittstelle ist keine Registrierung und erlaubt keine Regelmutation. Der vorhandene Katalogvertrag `research-gate-policy-v1` und die Funktion `render_research_gate_contract` bleiben erhalten.

Der Test `research_gate_isolation` übersetzt und verknüpft die Gateimplementierung direkt, ohne `bruce_core`, Mathematikbibliotheken oder Renderer. Er prüft zulässigen und unvollständigen Befund sowie weiterhin gesperrte Promotionsbefugnisse. Vor der Trennung scheiterte genau dieser unabhängige Link an der JSON-Abhängigkeit. Die bestehende Regelmatrix und CLI-Katalogtests sichern die fachlichen Fälle und die Ausgabe.

Es wurden keine Zulassungsbedingungen, Prioritäten, Gewichte, Freigaben oder Regeln geändert. Die menschliche Regelhoheit aus ADR-013 bleibt unverändert; ein geschützter menschlicher Freigabekanal wird durch diese Strukturänderung nicht implementiert.

Der Renderer nutzt weiterhin die vorhandene JSON-Hilfe des Forschungsmoduls. Diese Restkopplung betrifft die Darstellung, nicht mehr die isoliert verwendbare Entscheidung. Die Abhängigkeiten des Erfahrungsquellenaudits bleiben ebenfalls offen. Blueprint-Paket B ist weitergeführt, nicht vollständig abgeschlossen.

## Prüfnachweis am 25.09.2026

Lokal und remote bestanden 110/110 Testgruppen. Der Nutzerpfad `./build/bruce` auf Bruce-AI wurde als 0.85.0 gebaut und geprüft. Die komplette Ausgabe von `gate-contracts` vor und nach dem Update war bytegleich. Installation und TGZ-Paketprüfung bestanden; die CLI-Prüfung bestand lokal und remote mit 92 Beispielaufrufen und gültigen internen Links. Quellen und Dokumentation wurden per SCP übertragen.
