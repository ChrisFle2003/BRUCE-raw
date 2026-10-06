# Core 0.75: Zulassungsinspektion konkreter Läufe

## Anwendung

```bash
bruce gate-run lauf.bruce
```

Der Befehl liest höchstens 32 MiB und rekonstruiert den Lauf mit `replay_application`. Erst nach erfolgreicher Prüfung wird die Zulassungsbewertung erstellt. Es wird kein Erfahrungsspeicher geöffnet, geschrieben oder verändert. Zusätzliche Argumente und Datei-/Replayfehler führen zu Exit 2. Fachliche Ablehnungen werden als JSON mit Exit 0 ausgegeben.

## Ausgabe und Bedeutung

Vertrag `application-gates-v1`, Policy `research-gate-policy-v1`:

| Feld | Bedeutung |
| --- | --- |
| `kind` | Anwendungstyp des wiederhergestellten Laufs |
| `replay_checked` | true nach erfolgreichem vollständigen Replay |
| `applicable` | true für Int64-, exakte und Grid-Forschung |
| `selection.allowed`, `selection.reason` | Entscheidung des bestehenden Forschungsgates aus Status und Stopgrund |
| `experience_source.allowed`, `experience_source.reason` | Ergebnis des bestehenden Erfahrungsquellenaudits; bei Erfolg Allowed, sonst dessen Fehlercode |
| `learn_promotion`, `publish`, `general_proof` | Immer false; keine zusätzlichen Befugnisse |

Eine Auswahlzulassung garantiert weder vorhandene Kandidaten noch eine erfolgreiche numerische Projektion an einer bestimmten Stelle. Diese Prüfungen bleiben in der Kompatibilitätsausführung. Vollständig abgeschlossene negative Forschung kann das Auswahlgate bestehen.

Eine Quellenzulassung garantiert keinen Import in einen konkreten Erfahrungsspeicher. Kapazität, Duplikate, Ereignisgrenzen und spätere Kontextnutzung bleiben davon getrennt. Ein normales einzelnes Forschungsbundle kann als Auswahlquelle geeignet sein und trotzdem am Erfahrungsformat scheitern, weil Erfahrung unabhängige Portfoliobefunde verlangt.

Andere Anwendungstypen liefern `applicable: false`, beide Zulassungen false und `UnsupportedApplicationKind`. Ein Bruchbeweis kann fachlich gültig sein, obwohl dieses Forschungsgate nicht auf ihn anwendbar ist.

## Gemeinsame Implementierung und Grenzen

`inspect_application_gates` verwendet den allgemeinen Replayadapter und `research_gates`. Der neue lesende Adapter `audit_experience_source` ruft denselben internen Prüfer wie der bestehende Erfahrungsimport auf; es gibt keine zweite Implementierung der Quellenregeln.

Die Erfahrungsprüfung kann das Bundle erneut replayen. Sie behält ihre 1-MiB-Quellengrenze, den bestehenden Arbeitsgrenzwert von 1.000.000 und die unterstützten unabhängigen Portfolioformate einschließlich ihrer Grid-Einbettung. Deshalb kann ein allgemeiner Replay gelingen und die anschließende Erfahrungsprüfung dennoch abgelehnt werden. Die Ablehnungsgründe stammen hier aus dem Importprüfer, nicht ausschließlich aus dem abstrakten Gate-Regelkatalog.

Es gibt bei diesem Befehl noch keine zusätzlichen Abbruch- oder Budgetoptionen. Bestehende Bundles, `assess-run`, `gate-contracts` und Importfehler bleiben unverändert. Optimierungen wie das Vermeiden des zweiten Replays sind nachrangig.

## Tests und weitere Architektur

Die Tests prüfen historische Bundles, nicht anwendbare Typen, Manipulation und zusätzliche Argumente. Für eine gültige unabhängige Portfolioquelle, fehlenden Holdout und einen Teillauf vergleichen sie die Inspektion mit dem tatsächlichen Erfahrungsimport.

Weitere Aufgaben sind typisierte anwendungsübergreifende Gateentscheidungen, zusätzliche Geltungsbereiche und die kontrollierte Weiterleitung zugelassener Ergebnisse. Dieses Paket führt keine automatische Promotion und keinen vollständigen M0–M2-Abschluss ein.

## Ausgeführte Abnahme am 25.09.2026

- Lokaler Debug-Build: 108/108 CTest-Gruppen bestanden.
- SCP-Übertragung auf Bruce-AI; dort Release-Build und 108/108 CTest-Gruppen bestanden.
- Remote Installation und TGZ-Paketprüfung für Core 0.75.0 erfolgreich, einschließlich historischer Wiederherstellung.
- CLI-Dokumentation lokal und remote: 89 erfolgreiche Beispielaufrufe, 63 dokumentierte Optionen, 324 Diagnosen und interne Links geprüft.
- Die neuen CLI-Tests scheiterten vor Implementierung erwartungsgemäß mit `UnknownCommand: gate-run`.

Die Nachweise beziehen sich auf die geprüften Linux-Umgebungen, nicht auf einen allgemeinen Architekturabschluss oder eine öffentliche Veröffentlichung.
