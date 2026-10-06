# BRUCE_raw Dokumentation

**[Aktueller Blueprint und nächste Architekturabnahmen](architecture/03-blueprint-consolidation.md)** – Konsolidierter Bestand und verbleibende Grenzen in Core 0.102.

**Aktuelle Benutzung: [CLI-Handbuch für Core 0.102.0](cli/README.md)** mit vollständiger Befehlsreferenz und geprüftem Tutorial.

**Aktuelle Dateipersistenz:** [Core 0.102 – gemeinsamer lokaler Adapter](implementation/183-local-file-adapter-usage.md).

**Steuerungsabnahme:** [Core 0.101 – durchgängiger Ablauf](implementation/182-control-consolidation-usage.md) und [gemeinsamer Bestandsvertrag](contracts/06-control-and-publication.md).

**Historische technische Abnahme:** [Lokales CLI-Profil für Core 0.93](implementation/174-local-cli-acceptance-profile.md) mit getrennt ausgewiesenen offenen Gesamtmeilensteinen.

Diese Dokumentation ist nach Verantwortung getrennt. Der aktuelle Blueprint und der Steuerungsvertrag beschreiben den geprüften Bestand; ältere Zieltexte bleiben als Entwurf gekennzeichnet zu lesen. Der [Core-V1-Benutzungsstand](implementation/06-core-v1-usage.md) trennt die erste ausführbare C++-Runtime von den noch geplanten Funktionen.

```text
architecture/     Gesamtstruktur, Invarianten und Modulgrenzen
contracts/        Daten- und Verhaltensvertraege
implementation/   Tests, Phasen und offene Entscheidungen
adr/              zukuenftige begruendete Architekturentscheidungen
```

## Bereichseinstiege

- [Architektur](architecture/README.md)
- [Verträge](contracts/README.md)
- [Umsetzung](implementation/README.md) und [Abnahmebelege](implementation/acceptance/README.md)
- [Konzepte](concepts/README.md) – dokumentierte Ideen, nicht als Umsetzung lesen
- [Audits](audit/README.md), [Arbeitsprotokolle](superpowers/README.md) und [Historie](history/README.md)

**Regelhoheit:** [Lokaler Freigabevertrag](contracts/05-human-rule-approval.md) mit bestätigtem Export und aktiver numerischer Regelverwaltung.

## Lesereihenfolge

1. [Produktidentitaet und Invarianten](architecture/01-identity-and-invariants.md)
2. [Gesamtarchitektur](architecture/02-system-architecture.md)
3. [Z-Kompatibilitaetsraum](contracts/01-z-compatibility.md)
4. [Token, Sprache und Relationen](contracts/02-token-language-relations.md)
5. [Evidenz und Experience Store](contracts/03-evidence-and-experience.md)
6. [Mathematische Forschung und Replay](contracts/04-math-evidence-replay.md)
7. [Teststrategie](implementation/01-test-strategy.md)
8. [Umsetzungsphasen](implementation/02-implementation-phases.md)
9. [Offene Entscheidungen](implementation/03-open-decisions.md)
10. [Konsolidierte Entwicklungsroadmap](implementation/04-roadmap.md) – Gesamtplanung einschließlich Lazy Grid, NanoCell und Phase-DAG; begründete Weiterentwicklung der bisherigen Phasen.
11. [Core-V1-Umsetzung](implementation/05-core-v1-plan.md)
12. [Core-V1-Benutzung und Grenzen](implementation/06-core-v1-usage.md)
13. [Z-Kompatibilität: Umsetzung](implementation/07-z-compatibility-plan.md)
14. [Core 0.2: dynamische Regeln und Filtergruppen](implementation/08-z-compatibility-usage.md)
15. [Exakte Zahlendomänen: Umsetzung](implementation/09-exact-domains-plan.md)
16. [Core 0.3: große Ganzzahlen und rationale Forschung](implementation/10-exact-domains-usage.md)
17. [Nichtlineare Forschung: Umsetzung](implementation/11-power-research-plan.md)
18. [Core 0.4: begrenzte Potenzformelsuche](implementation/12-power-research-usage.md)
19. [Adaptive Koeffizientensuche: Umsetzung](implementation/13-adaptive-search-plan.md)
20. [Core 0.5: datenabhängige Suchplanung](implementation/14-adaptive-search-usage.md)
21. [Mehrtermige Polynome: Umsetzung](implementation/15-polynomial-research-plan.md)
22. [Core 0.6: mehrtermige Polynomforschung](implementation/16-polynomial-research-usage.md)
23. [Gemeinsame Strategieplanung: Umsetzung](implementation/17-strategy-portfolio-plan.md)
24. [Core 0.7: mehrere Strategien in einem Forschungslauf](implementation/18-strategy-portfolio-usage.md)

25. [Rationale Bruchfunktionen: Umsetzung](implementation/19-rational-linear-plan.md)
26. [Core 0.8: Bruchfunktionen mit Definitionsbereich](implementation/20-rational-linear-usage.md)

27. [Portfolio mit Definitionsbereichen: Vertrag](implementation/21-portfolio-domain-contract.md)
28. [Core 0.9: drei Strategien gemeinsam untersuchen](implementation/22-rational-portfolio-usage.md)

29. [Datenabhängige Reihenfolge: Vertrag](implementation/23-adaptive-portfolio-contract.md)
30. [Core 0.10: datenabhängige Strategieplanung](implementation/24-adaptive-portfolio-usage.md)

31. [Routing-Erfahrungsspeicher: Vertrag](implementation/25-experience-routing-contract.md)
32. [Core 0.11: Erfahrungen speichern und nutzen](implementation/26-experience-routing-usage.md)
33. [Quadratische Nenner: Vertrag](implementation/27-rational-quadratic-contract.md)
34. [Core 0.12: Bruchfunktionen mit quadratischem Nenner](implementation/28-rational-quadratic-usage.md)
35. [Viererportfolio und Erfahrung: Vertrag](implementation/29-quadratic-portfolio-contract.md)
36. [Core 0.13: vier Formelfamilien mit Erfahrungsplanung](implementation/30-quadratic-portfolio-usage.md)
37. [Begrenzte Ausdruckssynthese: Vertrag](implementation/31-expression-synthesis-contract.md)
38. [Core 0.14: Ausdrücke zusammensetzen und prüfen](implementation/32-expression-synthesis-usage.md)

39. [Ausdruckszusammenführung: Vertrag](implementation/33-expression-equivalence-contract.md)
40. [Core 0.15: gleiche Formeln mit Definitionslücken zusammenführen](implementation/34-expression-equivalence-usage.md)

41. [Fünferportfolio mit Ausdruckssynthese: Vertrag](implementation/35-expression-portfolio-contract.md)
42. [Core 0.16: fünf mathematische Suchwege gemeinsam](implementation/36-expression-portfolio-usage.md)

43. [Fünferplanung und Routing-Erfahrung: Vertrag](implementation/37-five-experience-contract.md)
44. [Core 0.17: fünf Suchwege mit Erfahrung ordnen](implementation/38-five-experience-usage.md)

45. [Indizierte Ausdruckserzeugung: Vertrag](implementation/39-indexed-expression-contract.md)
46. [Core 0.18: weniger Planungsarbeit für Ausdrucksbäume](implementation/40-indexed-expression-usage.md)

47. [Grammatikindex im Portfolio und Erfahrungsspeicher: Vertrag](implementation/41-indexed-portfolio-contract.md)
48. [Core 0.19: indizierte Ausdruckssuche gemeinsam nutzen](implementation/42-indexed-portfolio-usage.md)

49. [Auswählbare Ausdrucksoperatoren: Vertrag](implementation/43-expression-operators-contract.md)
50. [Core 0.20: die Rechenarten einer Suche auswählen](implementation/44-expression-operators-usage.md)

51. [Operatorauswahl im Portfolio: Vertrag](implementation/45-portfolio-operators-contract.md)
52. [Core 0.21: ausgewählte Rechenarten mit fünf Suchwegen nutzen](implementation/46-portfolio-operators-usage.md)

53. [Begrenzte Potenzen in Ausdrucksbäumen: Vertrag](implementation/47-expression-powers-contract.md)
54. [Core 0.22: Potenzen direkt in Ausdrücken verwenden](implementation/48-expression-powers-usage.md)

55. [Potenzausdrücke im Fünferportfolio: Vertrag](implementation/49-portfolio-powers-contract.md)
56. [Core 0.23: Potenzausdrücke gemeinsam planen und Erfahrung nutzen](implementation/50-portfolio-powers-usage.md)

57. [Begrenzter Teilbaumcache: Vertrag](implementation/51-cached-expression-contract.md)
58. [Core 0.24: wiederkehrende kleine Teilbäume wiederverwenden](implementation/52-cached-expression-usage.md)

59. [Teilbaumcache im Portfolio und in der Erfahrungsplanung: Vertrag](implementation/53-cached-portfolio-contract.md)
60. [Core 0.25: Teilbäume im Portfolio wiederverwenden und Erfahrung nutzen](implementation/54-cached-portfolio-usage.md)

61. [Rationale Definitionsbereiche vergleichen: Vertrag](implementation/55-fraction-comparison-contract.md)
62. [Core 0.26: Polynombrüche mit ihren Definitionsbereichen vergleichen](implementation/56-fraction-comparison-usage.md)

63. [Rationale Definitionsbereiche in Ausdrucksgruppen: Vertrag](implementation/57-q-expression-contract.md)
64. [Core 0.27: genauere Ausdrucksgruppen über Q](implementation/58-q-expression-usage.md)

65. [Q-Gruppen im Portfolio und Erfahrungskontext: Vertrag](implementation/59-q-portfolio-contract.md)
66. [Core 0.28: Q-Gruppen über fünf Familien gemeinsam nutzen](implementation/60-q-portfolio-usage.md)

67. [Modulare Nullstellenausschlüsse: Vertrag](implementation/61-modular-comparison-contract.md)
68. [Core 0.29: höhere Nennergrade exakt vergleichen](implementation/62-modular-comparison-usage.md)

69. [Modulare Q-Gruppen in Forschung und Erfahrung: Vertrag](implementation/63-modular-research-contract.md)
70. [Core 0.30: Primzahlbeweise beim Forschen nutzen](implementation/64-modular-research-usage.md)

71. [Begrenzter Cache für Bereichsgleichheit: Vertrag](implementation/65-domain-cache-contract.md)
72. [Core 0.31: bewiesene Bereichsgleichheiten wiederverwenden](implementation/66-domain-cache-usage.md)

73. [Bereichscache im Portfolio und Erfahrungskontext: Vertrag](implementation/67-domain-cache-portfolio-contract.md)
74. [Core 0.32: Bereichsbeweise im Portfolio wiederverwenden](implementation/68-domain-cache-portfolio-usage.md)
75. [Begrenzte rationale Nullstellenzeugen: Vertrag](implementation/69-rational-witness-contract.md)
76. [Core 0.33: rationale Definitionslücken finden](implementation/70-rational-witness-usage.md)
77. [Rationale Gegenbeispiele in der Ausdruckssuche: Vertrag](implementation/71-witness-expression-contract.md)
78. [Core 0.34: Definitionslücken während der Forschung prüfen](implementation/72-witness-expression-usage.md)
79. [Gegenbeispiele im Portfolio und Erfahrung: Vertrag](implementation/73-witness-portfolio-contract.md)
80. [Core 0.35: gemeinsamer Forschungsablauf](implementation/74-witness-portfolio-usage.md)
81. [Aktuelle Architektur und M0–M2-Abnahmen](implementation/75-current-architecture-and-milestones.md)
82. [Versionierter Grid-Einzelauftrag: Vertrag](implementation/76-grid-replay-contract.md)
83. [Core 0.36: Grid-Aufträge speichern und wiederholen](implementation/77-grid-replay-usage.md)
84. [GridSpec und begrenzter Cache – Vertrag](implementation/78-grid-cache-contract.md)
85. [Core 0.37: Wiederholte Grid-Abfragen](implementation/79-grid-cache-usage.md)
86. [Grid-Quellen für exakte Forschung – Vertrag](implementation/80-grid-research-contract.md)
87. [Core 0.38: Vom Grid zur Formelforschung](implementation/81-grid-research-usage.md)
88. [Beide Grid-Koordinatenrichtungen – Vertrag](implementation/82-grid-axis-contract.md)
89. [Core 0.39: Basis oder Exponent untersuchen](implementation/83-grid-axis-usage.md)
90. [Gemeinsamer Grid-/Forschungsrahmen – Vertrag](implementation/84-grid-budget-contract.md)
91. [Core 0.40: Grid-Import und Forschung begrenzen](implementation/85-grid-budget-usage.md)
92. [Begrenzte Grid-Stapel – Vertrag](implementation/86-grid-partial-contract.md)
93. [Core 0.41: Grid-Teilbefunde behalten](implementation/87-grid-partial-usage.md)
94. [Grid-Fortsetzung – Vertrag](implementation/88-grid-resume-contract.md)
95. [Core 0.42: Grid-Stapel fortsetzen](implementation/89-grid-resume-usage.md)
96. [Grid-Forschung als Erfahrung – Vertrag](implementation/90-grid-experience-contract.md)
97. [Core 0.43: Aus Grid-Forschung Erfahrung gewinnen](implementation/91-grid-experience-usage.md)
98. [Experience-Korrekturketten – Vertrag](implementation/92-experience-replacement-contract.md)
99. [Core 0.44: Erfahrungen nachvollziehbar ersetzen](implementation/93-experience-replacement-usage.md)
100. [Gemeinsamer Grid-/Experience-Importrahmen – Vertrag](implementation/94-grid-experience-budget-contract.md)
101. [Core 0.45: Grid- und Experience-Import gemeinsam begrenzen](implementation/95-grid-experience-budget-usage.md)
102. [Architektur-Konsolidierung – Plan](implementation/96-research-contract-consolidation-plan.md)
103. [Core 0.46: Gemeinsame Forschungsverträge](implementation/97-research-contract-usage.md)
104. [Gemeinsamer Auftragseinstieg – Plan](implementation/98-application-task-plan.md)
105. [Core 0.47: Typisierte Laufaufträge](implementation/99-application-task-usage.md)
106. [Gemeinsamer Ergebnisvertrag](implementation/100-application-result-contract.md)
107. [Core 0.48: Einheitliche Laufinspektion](implementation/101-application-result-usage.md)
108. [Geprüfte Ausführungssitzung – Vertrag](implementation/102-execution-phases-contract.md)
109. [Core 0.49: Gemeinsame Ausführungsphasen](implementation/103-execution-phases-usage.md)

## Statuslegende

| Kennzeichnung | Bedeutung |
| --- | --- |
| Implementiert und aktuell verifiziert | Code plus aktueller Testnachweis vorhanden |
| Implementiert, aber nicht aktuell verifiziert | Code vorhanden, aktueller Testnachweis fehlt |
| Dokumentiert / Konzept | vereinbarter Vertrag, aber keine Runtime |
| Blueprint-Vorschlag | empfohlene spaetere Umsetzung |
| Offene Entscheidung | vor Implementierung zu klaerende Wahl |
| Nicht aus dem Repository ableitbar | keine belastbare Grundlage vorhanden |

110. [Strukturierte Sitzungsfehler – Vertrag](implementation/104-execution-failure-contract.md)
111. [Core 0.50: Sitzungsfehler mit Verlauf und Ursache](implementation/105-execution-failure-usage.md)

112. [Replay-Eingangsbudget – Vertrag](implementation/106-replay-byte-budget-contract.md)
113. [Core 0.51: Prüfdaten vor dem Replay begrenzen](implementation/107-replay-byte-budget-usage.md)

114. [Abbruch an Prüfgrenzen – Vertrag](implementation/108-verification-cancellation-contract.md)
115. [Core 0.52: Prüfung an Phasengrenzen abbrechen](implementation/109-verification-cancellation-usage.md)

116. [Interne Grid-Replaypunkte – Vertrag](implementation/110-grid-replay-checkpoints-contract.md)
117. [Core 0.53: Grid-Prüfung zwischen Abfragen abbrechen](implementation/111-grid-replay-checkpoints-usage.md)

118. [Forschungsreplay-Checkpoints – Vertrag](implementation/112-research-replay-checkpoints-contract.md)
119. [Core 0.54: Forschungsreplay an Suchcheckpoints abbrechen](implementation/113-research-replay-checkpoints-usage.md)

120. [Grid-Forschungsreplay-Checkpoints – Vertrag](implementation/114-grid-research-replay-contract.md)
121. [Core 0.55: Grid-Forschung im Replay abbrechen](implementation/115-grid-research-replay-usage.md)

122. [Bruchvergleichsreplay-Checkpoints – Vertrag](implementation/116-comparison-replay-checkpoints-contract.md)
123. [Core 0.56: Bruchvergleichsreplay abbrechen](implementation/117-comparison-replay-checkpoints-usage.md)

124. [Z-Kompatibilitätsreplay – Vertrag](implementation/118-compatibility-replay-checkpoints-contract.md)
125. [Core 0.57: Z-Kompatibilitätsreplay abbrechen](implementation/119-compatibility-replay-checkpoints-usage.md)

126. [Erfahrungs-Quellaudits mit Replayprüfpunkten – Vertrag](implementation/120-experience-replay-checkpoints-contract.md)
127. [Core 0.58: Erfahrungsnachweise im Replay abbrechen](implementation/121-experience-replay-checkpoints-usage.md)

128. [Unterbrechbare Bundle-Prüfsumme – Vertrag](implementation/122-serialization-digest-checkpoints-contract.md)
129. [Core 0.59: Prüfsummenerzeugung abbrechen](implementation/123-serialization-digest-checkpoints-usage.md)

130. [Grid-Inhaltsaufbau mit Abbruchpunkten – Vertrag](implementation/124-grid-serialization-records-contract.md)
131. [Core 0.60: Grid-Evidenz beim Aufbau abbrechen](implementation/125-grid-serialization-records-usage.md)

132. [Erfahrungsimport im Erstlauf – Vertrag](implementation/126-execution-experience-checkpoints-contract.md)
133. [Core 0.61: Erfahrungsnachweise im Erstlauf abbrechen](implementation/127-execution-experience-checkpoints-usage.md)

134. [Gridquellen im Erstlauf – Vertrag](implementation/128-execution-grid-source-checkpoints-contract.md)
135. [Core 0.62: Grid-Eingangsquellen im Erstlauf abbrechen](implementation/129-execution-grid-source-checkpoints-usage.md)

136. [Forschungsimport der Z-Auswahl – Vertrag](implementation/130-execution-compatibility-source-contract.md)
137. [Core 0.63: Forschungsquellen der Z-Auswahl abbrechen](implementation/131-execution-compatibility-source-usage.md)

138. [Native Fortsetzungsimporte – Vertrag](implementation/132-execution-resume-import-contract.md)
139. [Core 0.64: Vorgängerprüfung und Cacheaufbau abbrechen](implementation/133-execution-resume-import-usage.md)

- [Core 0.89: lokale Regelprüfung](implementation/169-local-rule-review-usage.md)

- [Core 0.90: Regelvorschläge in der Anwendungsschicht](implementation/170-rule-proposal-application-usage.md)

- [Core 0.91: aktive Regeln und Entscheidungsjournal](implementation/171-active-rule-history-usage.md)

- [Core 0.92: explizite Erfahrungsübergabe](implementation/172-experience-transfer-usage.md)

- [Core 0.93: explizite Z-Auswahlübergabe](implementation/173-compatibility-transfer-usage.md)

- [Core 0.94: gemeinsame Übergabeausführung](implementation/175-transfer-control-usage.md)

Aktuelle Fortschreibung: [Core 0.95 – Transferplanung steuern](implementation/176-transfer-planning-control-usage.md).

Aktuelle Fortschreibung: [Core 0.96 – Transferdateien kontrolliert lesen](implementation/177-transfer-input-control-usage.md).

Aktuelle Fortschreibung: [Core 0.97 – Regelspeicher im Transferkonto](implementation/178-transfer-rule-store-control-usage.md).

Aktuelle Fortschreibung: [Core 0.98 – kontrollierter Transferexport](implementation/179-transfer-export-control-usage.md).

Aktuelle Fortschreibung: [Core 0.99 – kontrollierte eingebettete Regelprüfung](implementation/180-transfer-rule-validation-control-usage.md).

Aktuelle Fortschreibung: [Core 0.100 – native Z-Regelsteuerung](implementation/181-native-z-rule-control-usage.md).
