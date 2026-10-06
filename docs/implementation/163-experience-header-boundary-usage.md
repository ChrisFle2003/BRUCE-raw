# Core 0.83: begrenzte Headerentkopplung im Experience-Modul

`experience/store.hpp` bindet nicht länger `application/portfolio_order.hpp` und damit die vollständigen Forschungs-/Planungstypen ein. Der Header benötigt nur die vorhandenen Core-Grundtypen und Replaycallbacks. `ExactResearchTask` und `PortfolioOrder` werden für die bestehende Rankingdeklaration vorwärts deklariert.

Die Implementierung `experience/store.cpp` bindet den benötigten Planungsheader nun ausdrücklich selbst ein. Aufrufer, die nur Snapshotdaten oder die Storefunktionen benötigen, bekommen keine vollständigen Planungstypen über diesen Header. Wer `ExactResearchTask` oder `PortfolioOrder` instanziiert, muss deren jeweiligen Header ausdrücklich einbinden.

Das ist eine begrenzte Headerentkopplung, keine vollständige Beseitigung der Abhängigkeit Experience → Application: Die Rankingfunktion referenziert weiterhin deren Typen und die Implementierung verwendet die bestehende Portfolioplanung und Quellenprüfung. Diese verbleibende Grenze wird nicht als bereits gelöst ausgegeben.

Der neue Übersetzungstest `experience_header_boundary` bindet ausschließlich den Storeheader ein, verwendet `ExperienceStore` und stellt sicher, dass Forschungs- und Planungstypen dort unvollständig bleiben. Vor der Änderung schlugen beide statischen Prüfungen fehl. Der Test benötigt keine Verknüpfung mit `bruce_core`.

Es werden keine Regeln, Gewichte, Rankings, Snapshotformate oder Freigaben verändert. Die menschliche Regelhoheit aus ADR-013 bleibt unverändert. CLI-Aufrufe und Berichte bleiben erhalten; externe C++-Aufrufer müssen gegebenenfalls die nun ausdrücklich benötigten Header ergänzen.

Weitere Schritte sind die fachliche Trennung der Routing-Schnittstelle und die Bereinigung von Darstellungsabhängigkeiten. Dieses Paket schließt Blueprint-Paket B noch nicht vollständig ab.

## Prüfnachweis am 25.09.2026

Lokal und remote bestanden 109/109 Testgruppen. Der Nutzerpfad `./build/bruce` auf Bruce-AI wurde als 0.83.0 gebaut und geprüft. Installation und TGZ-Paketprüfung bestanden; die CLI-Prüfung bestand lokal und remote mit 92 Beispielaufrufen und gültigen internen Links. Quellen und Dokumentation wurden per SCP übertragen.
