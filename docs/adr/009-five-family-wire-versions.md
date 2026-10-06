# ADR-009: Gemeinsame Versionsauswahl des Fünferportfolios

## Status

Akzeptiert und implementiert mit Core 0.35 im freigegebenen Integrations-/Konsolidierungspaket.

## Kontext

Header, Engine und Planernamen des Fünferportfolios waren in mehreren langen Fallunterscheidungen für Darstellung, Export und Replay hinterlegt. Neue Generationen erhöhten das Risiko einer widersprüchlichen Zuordnung. Historische Ergebnisse und Kosten dürfen sich durch die Konsolidierung nicht ändern.

## Entscheidung

Eine gemeinsame `PortfolioVersion`-Auswahl enthält die neun bisherigen Fähigkeitsgenerationen und ihre drei Reihenfolgen. Sie bestimmt Header, Engine und Planer für V8 bis V34. Die Fähigkeitspriorität bleibt: rationale Zeugen, Bereichscache, modulare Q-Gleichheit, Q-Gleichheit, Teilbaumcache, literale Potenzen, Operatorauswahl, Grammatikindex, ursprüngliches Fünferportfolio.

Ausgabe und Export wählen über den validierten Auftrag. Replay erkennt ausschließlich exakte bekannte Header einschließlich Zeilenende und verwendet dieselbe Metadatenzuordnung. Fachliche Policy-Prüfungen, historische Rechenwege und kanonischer Inhaltsvergleich bleiben unabhängig davon erhalten.

## Konsequenzen und Prüfung

Weniger doppelte Wire-Metadaten; keine Änderung der logischen Arbeit oder alter Bundles. Alle bisherigen 24 Fünferportfolio-Versionen V8–V31 wurden unabhängig mit dem vorherigen Binary verglichen: JSON und Exportbytes unverändert, Replay identisch. Neue V32–V34 werden durch die Portfolio-/Experience-Tests geprüft.

Die Entscheidung führt keinen allgemeinen Plugin-/Strategie-Port ein. Andere Bundlefamilien und fachliche Versionsunterschiede bleiben eigene Konsolidierungsarbeit.
