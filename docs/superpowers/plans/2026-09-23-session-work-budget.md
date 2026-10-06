# Gemeinsames Prüfpunktbudget – Implementierungsplan

> Umsetzung direkt in dieser Sitzung mit superpowers:executing-plans; Abschlussreview separat.

**Goal:** Alle acht geprüften Auftragstypen unter ein optionales, phasenübergreifendes Prüfpunktbudget stellen.
**Architecture:** Sitzungslokale Abrechnung, additive Adapterbeobachtung, getrennte Signalcallbacks. Die nativen mathematischen Budgets und Bundles bleiben bestehen.
**Tech Stack:** C++20, CMake, Python-CLI-Tests.
**Spec:** [Vertrag](../../implementation/134-session-work-budget-contract.md).

## Bedingungen und Prüffokus

Keine neuen Abhängigkeiten. Kein Git-Repository vorhanden: keine Commits/Worktrees, Änderungen im freigegebenen Arbeitsordner. Keine Gleichsetzung von Prüfpunkten und Rechenoperationen. Kein Export nach Fehler. Keine zusätzliche Abfrage vorhandener Signalcallbacks. Beachtung numerischer Exception-Catches, nested Imports, kleiner Budgets, UINT64_MAX und nativer Teilbefunde.

## Schritte

- [x] CLI-Regression für fehlende Option, Phasensummen, Budgetgrenzen und unveränderte Exporte schreiben und rot ausführen.
- [x] Unit-Matrix über acht Auftragstypen, verschachtelte Importe, unabhängige Abbruchcallbacks und jede erreichbare Budgetgrenze ergänzen.
- [x] SessionBudget/Usage implementieren; Application-Adapter erhält optionalen Arbeitsbeobachter; Sitzung bindet diesen an vorhandene innere Punkte.
- [x] CLI-Option und V14-/Fehler-V2-Berichte ergänzen; fokussierte Tests grün ausführen.
- [x] Architekturübersicht sachlich aktualisieren, Benutzung und Meilenstein-Abnahmeschritte dokumentieren.
- [x] Debug/UBSan-Suiten, historische Fixtures, Demo und Baseline vergleichen; unabhängiges Review und Abnahme festhalten.

## Verlauf

Freigabe: Nutzer hat die vorgeschlagene Architekturkonsolidierung ausdrücklich freigegeben. Routineentscheidungen und Umsetzung erfolgen ohne erneute Genehmigung. Der Vertrag konkretisiert die Grenzen der ersten Lieferung.

Zwischenstand: Beide CLI-Tests vor Umsetzung erwartungsgemäß rot (UnknownOption), anschließend zwölf fokussierte Gruppen grün. Ein Matrix-Test enthielt irrtümlich einen Projektionspunkt für manuelle Z-Kandidaten; die native Zulassungsprüfung lehnte dies korrekt ab. Testeingabe berichtigt, Produktionscode unverändert. Unabhängiges Abschlussreview ohne relevante Befunde; zusätzlich 20 unbudgetierte Baselinevergleiche, 15 budgetierte Ergebnisvergleiche und numerische Beobachter-Fehlerinjektionen bestanden.

Abgeschlossen: Debug 101/101, UBSan 101/101, vollständige Demo, elf eigene Baselinefälle samt budgetierter Variante, 26 historische Fixtures und 223 Markdown-Dateien mit gültigen lokalen Links. Keine Produktionsänderung nach dem unabhängigen Review; abschließend wurden Demo und Dokumentation vervollständigt. Liefernachweis: docs/implementation/acceptance/session-work-budget-v1-2026-09-23.md.
