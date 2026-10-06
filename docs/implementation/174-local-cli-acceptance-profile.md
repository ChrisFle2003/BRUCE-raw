# Technisches Abnahmeprofil local-cli-core093-v1

Stand: 26.09.2026. Zielversion: Core 0.93.0. Dieses Profil prüft den implementierten lokalen Linux-CLI-Vertrag. Es ersetzt weder das ursprüngliche M0–M2-Ziel noch eine neue menschliche Regelbestätigung. [Aktuelle Matrix](136-m0-m2-acceptance-matrix.md), [Ergebnisprotokoll](acceptance/local-cli-core093-v1-2026-09-26.md).

## Geltungsbereich

- Vorhandene C++20-CLI und Bibliothek; lokaler Debug-Build und Release-Build am Nutzerpfad auf Bruce-AI.
- Acht allgemeine Auftragstypen und ihre bestehenden nativen Bundle-/Replayverträge.
- Eingebaute mathematische Strategien und ihre konkreten Zahlen-/Suchbereiche gemäß [CLI-Handbuch](../cli/README.md).
- Vorhandene lokale Gridverträge, numerische Auswahl, Erfahrungssnapshots, lineare Sitzung, native Budgets und Prüfpunkte.
- Lokale Regelverwaltung und zwei feste Transferaktionen innerhalb ihrer dokumentierten Grenzen.
- Bestehende Installations-, Paket- und historische Fixtureprüfungen.

Nicht abgenommen: universelle Mathematikentscheidung, freie Sprachanweisungen, dynamische Runtimeplugins, beliebige Gridexpansion, BigInt im Grid/Z, universelle Operations-/RAM-/Zeitgrenzen, maximale Signallatenz, allgemeine Aufgabenketten, externe Evidenz, globale menschliche Identitätsprüfung, Cockpit, NanoCell, Phase-DAG, GPU, Plattformportabilität oder bitidentische Builds.

## Verbindliche Prüfkriterien

| Kennung | Bestehender Vertrag / Akzeptanzkriterium | Maßgebliche CTest-Gruppen / Nachweise |
| --- | --- | --- |
| A01 | Acht Laufvarianten, Katalog und Bewertung stimmen; Fehler bleiben typisiert | `application_task_adapters`, `application_task_catalog`, `result_assessment_workflows`, `run_inspection_workflows` |
| A02 | Eingebaute Strategie-/Gateverträge und getrennte Aussagegrenzen bestehen | `research_contract_matrix`, `research_contract_catalog`, `research_gate_isolation`, `json_encoding_isolation` |
| A03 | Lineare Sitzung, native Grenzen und Import-/Replayprüfpunkte bleiben wirksam | `execution_session_states`, `execution_session_workflows`, `session_work_budget_contracts`, `session_work_budget_workflows`, `serialized_byte_budget_workflows`, sämtliche `*_control`-Gruppen |
| A04 | Lokale Adressen, Gridabfragen, Cache, Fortsetzung und feste Projektion bestehen | `core_contracts`, `grid_contracts`, `grid_replay_workflows`, `grid_cache_contracts`, `grid_resume_workflows`, `grid_axis_workflows`, `grid_research_contracts` |
| A05 | Exakte Domänen, begrenzte Formelfamilien und Bereichsvergleiche bestehen | Alle vorhandenen Exact-, Power-, Polynomial-, Rational-, Expression-, Portfolio- und Fraction-Comparison-Gruppen der vollständigen Suite |
| A06 | Numerische Auswahl, Abbruch-/Teilbefunde und Replay bleiben korrekt | `compatibility_contracts`, `compatibility_workflows`, `compatibility_replay_control` |
| A07 | Erfahrungseignung, Snapshotkorrekturen und Anwendungseinstiege bestehen | `experience_contracts`, `experience_workflows`, `experience_replace_workflows`, `experience_header_boundary`, `grid_experience_workflows` |
| A08 | Lokaler Dialog, Basisbindung, konkurrierender Wechsel und Journalgrenzen bestehen | `rule_proposal_contracts`, `rule_review_workflows`, `rule_history_contracts`, `rule_store_workflows` |
| A09 | Beide Transfers binden Daten und prüfen fachliche Quelle erneut; Exporte entsprechen dem direkten Weg | `experience_transfer_workflows`, `compatibility_transfer_workflows`, direkte Anwendungstests in `experience_contracts` und `compatibility_contracts` |
| A10 | Vollständige registrierte Suite besteht ohne ausgelassene Tests; konkrete Version wird überprüft | CTest-Gesamtlauf, JUnit-Protokoll und `cli_version` auf beiden Hosts |
| A11 | Installierte/verschobene Ausgabe, historische Fixtures und TGZ-Paket bestehen | `installed_cli_workflows` und zusätzlicher `verify_install.py --package`-Aufruf auf beiden Hosts |
| A12 | Dokumentierte CLI-Beispiele, Optionsabdeckung und Dateilinks bestehen | `docs/cli/verify_examples.py`; ergänzende Prüfung der überarbeiteten Architektur-/Vertragslinks |

„Technisch bestanden“ setzt voraus, dass alle registrierten Tests erfolgreich sind (keine Fehler, keine übersprungenen Fälle), Paket-/Installationsprüfung und CLI-Prüfung erfolgreich enden und die manuelle Code-/Dokumentprüfung die unten genannten Grenzen korrekt wiedergibt. Das Profil behauptet keine erschöpfende Eingaberaumabdeckung oder formale Softwarekorrektheit.

## Ausführung und Belege

Die vorhandenen Buildverzeichnisse werden neu gebaut; dies ist keine frische isolierte Neuinstallation des Betriebssystems oder ein Clean-Room-Build. Nachweise nennen diese Einschränkung. Beispiel lokal:

```bash
cmake --build build -j2
ctest --test-dir build --output-on-failure -j2 --output-junit /tmp/bruce093-acceptance-local.xml
python3 scripts/verify_install.py --build-dir build --fixtures tests/fixtures --version 0.93.0 --config Debug --package
python3 docs/cli/verify_examples.py ./build/bruce
```

Remote gilt derselbe Testumfang mit `Release` und dem vorhandenen `scripts/with-host-deps.sh`-Wrapper. Der maßgebliche Pfad ist `/home/chris/Desktop/Bruce_raw/build/bruce`. Prüfungen verwenden Testfixtures beziehungsweise temporäre Dateien und bestätigen keine produktiven Nutzerregeln.

Das Ergebnisprotokoll hält Version, Testanzahl und -ausgänge sowie Paket-/CLI-Erfolg fest. Ergänzende JSON-Daten enthalten die tatsächlich gelaufenen Testnamen und Zeiten auf beiden Hosts. JUnit-Ergebnisse sind Testnachweise; sie sind keine Benchmarkmessungen der Forschungsleistung.

## Manuelle Architekturprüfung

Für dieses Profil werden die acht `ApplicationTask`-Varianten, vier `ExperienceTask`-Varianten, Store→Quellenaudit-Abhängigkeit, getrennte Gate-/JSON-Bausteine, Prüfpunkteinheit und die beiden Transferverträge mit dem Code abgeglichen. Die Runtime wird nicht umgebaut, um eine Dokumentationsaussage nachträglich wahr zu machen.

Die folgenden Grenzen bleiben auch bei bestandenem Profil verbindlich:

1. Die menschliche Regelhoheit wird nur im beschriebenen lokalen Bedienablauf unterstützt; direkte Dateiwege bleiben möglich.
2. Transferberichte und Pläne sind keine Berechtigungen. Native Ergebnisbundles enthalten nicht generell Transferplan oder Journalgeneration.
3. Verfügbarkeit eines Berichts impliziert keine allgemeine Folgeauftragsfreigabe.
4. M0–M2 bleiben gegenüber dem umfassenderen ursprünglichen Blueprint teilweise offen; das Profil nimmt nur den benannten Teil ab.
