# Ergebnisse, Budgets, Abbruch und Fehler

[Handbuch](README.md) · [Befehle](commands.md) · [Vollständige Beispielantworten](examples.json)

## Ergebnisstatus richtig lesen

| Status | Fachliche Aussage |
| --- | --- |
| `CANDIDATE` | Passende Lernhypothese; zusätzliche Holdout-Bestätigung fehlt |
| `VERIFIED_ON_SAMPLES` | Konkrete Lern- und Holdout-Paare bestanden; kein allgemeiner Formelbeweis |
| `AMBIGUOUS` | Mehrere fachlich gleichrangige Ergebnisse; stabile Sortierung ist keine Entscheidung |
| `REFUTED` | Lernhypothesen durch Holdout widerlegt |
| `NO_CANDIDATE` | Kein passender Kandidat im abgeschlossenen deklarierten Suchraum |
| `INCOMPLETE` | Nicht vollständig verarbeitet, etwa wegen Budget oder Signal; Teilhypothesen sind keine abschließende Auswahl |
| `SELECTED` | Kompatibilität wählt einen eindeutigen zulässigen Kandidaten |
| `NO_COMPATIBLE_CANDIDATE` | Vollständige Regelprüfung lässt keinen Kandidaten zu |
| `EVALUATED` | Einzelner Grid-Pfad ausgewertet |
| `STOPPED` | Grid-Sentinel beendet einen Zweig |
| `COMPLETED` | Grid-Stapel vollständig verarbeitet; auch absichtlich gestoppte Blätter können darin vorkommen |
| `PROVEN_EQUIVALENT` | Vergleich der gelieferten Polynombrüche beweist gleiche Funktion samt rationalem Definitionsbereich |
| `PROVEN_DIFFERENT` | Vergleich beweist einen Unterschied der Werte oder Definitionsbereiche |
| `UNDECIDED` | Eingesetzte begrenzte Vergleichsverfahren entscheiden nicht |

Bei Forschung können einzelne `hypotheses[].status` vom Gesamtstatus abweichen. Immer Gesamtstatus, `stop_reason` und Einzelbefund zusammen auswerten. Der Diagnosekatalog enthält weitere versions-/pfadspezifische Stopgründe.

## Exact-Affine-Evidenzergebnis

`evidence create` und `evidence replay` liefern direkt dasselbe kanonische JSON: `evidence_contract: "affine-evidence-chain-v1"`, `independence: "declared-disjoint-v1"`, `countercheck_policy: "affine-inverse-v1"`, `countercheck_scope: "short-circuit-training-then-holdout-v1"`, `training_samples`, `holdout_samples` und unter `result` das native Exact-Research-Ergebnis. Die Antwort besitzt keine allgemeine `execution`-Sitzungshülle. `bigint` ist die Standarddomäne; ausschließlich die Exact-Affine-/Enumerate-Route ist gebunden.

`declared-disjoint-v1` umfasst die lokale Deklaration und technisch geprüfte Disjunktheit der x-Werte; fachliche Unabhängigkeit und externe Provenienz folgen daraus nicht. `short-circuit-training-then-holdout-v1` beschreibt den kurzschließenden Kontrollfluss, keine gemessene Vollabdeckung. Jeder erreichte Kandidat wird zuerst an Training geprüft; die erste Kontradiktion beendet seine Prüfung. Die inverse Gegenprüfung läuft nur nach einer passenden Vorwärtsauswertung. Holdout beginnt nur nach bestandener Trainingsprüfung und wenn Resultat-/Budgetfluss es erreicht. Budgets können innerhalb eines Kandidaten stoppen; `INCOMPLETE` ist kein Abdeckungsnachweis. Ein `INCOMPLETE`-Native-Lauf bleibt nach Replay `INCOMPLETE`. Status, Stopgrund, Zähler und Trace sind für die tatsächlich ausgeführte Prüfung maßgeblich. Kandidat, Score, `VERIFIED_ON_SAMPLES`, Digest und Replay beweisen keine allgemeine mathematische Formel. `result.general_proof` bleibt false; die Kette öffnet weder Learn-/Publish-Gates noch eine Regelaktivierung und führt keine Strategy-Promotion aus. Der Pfad führt auch keinen allgemeinen `ApplicationTask`, externen Checker oder Cloud-/Veröffentlichungsweg ein. [Befehl](commands.md#evidence--lokale-exact-affine-evidenzkette) · [Byteformat](formats.md#exact-affine-evidenzkette).

## NanoCell-Ergebnis und Replay

`nanocell run` und `nanocell replay` liefern direkt `nanocell-cli-v1`, ohne allgemeine `execution`-Hülle. `MATCHED` heißt: Der ausgewählte Kernzustand trifft nach der festen Technik deren Maske/Wert. `NO_MATCH` heißt: vollständige Ausführung ohne Treffer. `CONTEXT_REJECTED` heißt: Kontextdigest passt nicht zur Technik; dann ist `final_state` null und es wird keine Operation ausgeführt. `INCOMPLETE` heißt: Operations- oder Tracebudget beendet die Ausführung vor der vollständigen Technik. Alle vier sind fachliche Antworten mit Exit 0, keine Aussagen über allgemeine mathematische Beweise.

Der Befund zeigt `final_state` als dezimale Bytezahl oder null, `technique_id`, `technique_digest`, `core_digest`, `context_id`, `context_digest`, `active_index`, `operations_used`, `max_operations`, `trace_entries_used`, `max_trace_entries` und `trace` mit `START`, `OPERATION`, `TERMINAL` samt Zustand und Operationszähler. Die Antwort enthält die Kontextbytes nicht, das Bundle jedoch schon. `replay` prüft die gespeicherte Anfrage und den Befund durch erneute Ausführung und liefert bei gültiger Datei dieselbe Antwort; beschädigte oder nichtkanonische Dateien liefern Exit 2. Das ist ein isolierter NanoCell-Vertrag, keine Integration in die acht allgemeinen Auftragstypen oder deren Gates.

## JSON-Felder nach Ergebnisfamilie

Die Ausgabe ist ein versionsabhängiger Vertrag. Nicht jede Familie besitzt jedes Feld. Die [Beispieldatei](examples.json) enthält die vollständigen gemessenen Antworten einschließlich verschachtelter Felder; folgende Übersicht erläutert deren Rollen.

| Feld / Gruppe | Bedeutung |
| --- | --- |
| `engine`, `rule_version`, `policy_version`, `policy` | Version des mathematischen Ausführungs-/Entscheidungsvertrags, nicht automatisch die gesamte Core-Version |
| `status`, `stop_reason` | Fachlicher Zustand und Ursache der Beendigung |
| `general_proof`, `learn_gate`, `publish_gate` | Explizite Aussage-/Freigabegrenzen. Ein Samplebefund öffnet nicht automatisch Lernpromotion oder Veröffentlichung |
| `claim_scope`, `proof_scope`, `scope`, `explanation` | Geltungsbereich und lesbare Interpretation |
| `theoretical_candidates` | Beschriebener Suchraum, kein ausgeführter Arbeitsumfang |
| `candidates_executed`, `operations_used` | Tatsächlich gezählte Kandidaten bzw. logische strategieabhängige Arbeit |
| `integer_range_rejections` / numerische Ausschlusszähler | Wegen Zahlen-/Bereichsgrenzen verworfene Fälle; beim Interpretieren der Abdeckung beachten |
| `hypotheses` | Formeln, Koeffizienten, IDs, Status und je Familie Herkunft/Normalform/Definitionsbedingungen |
| `strategy_order`, `strategy_runs` | Portfolio-Reihenfolge und getrennte Unterläufe; Budgets werden gemeinsam verteilt |
| `experience_records_used`, `experience_votes` | Verwendete Erfahrungseinträge und Stimmen; 0 ist bei unpassendem Kontext möglich |
| `trace` | Logische Entscheidungen/Prüfungen des gewählten Pfads; keine CPU-Profilspur |
| Grid `path`, `depth`, `theoretical_space`, `evaluations` | Adresse, Tiefe, symbolischer Raum und tatsächlich ausgewertete Blätter |
| Batch `queries`, `cache` | Einzelanfragen und Cachebuchhaltung (Treffer, Fehlschläge, Einfügen, Verdrängen, Umgehen, Bytes, Höchststand, Auswertungen) |
| Resume `generation`, `audit_evaluations`, `new_evaluations`, `batch` | Generation, Prüfaufwand, neue Rechenarbeit und kumulativer Stapel |
| Grid-Forschung `research` | Eigentliches Forschungsergebnis innerhalb der Quellen-/Projektionshülle |
| Kompatibilität `groups`, `candidates`, Auswahlfelder | Maskengruppen, Kandidatenbewertungen und gleichrangige Auswahl; IDs sind keine Wahrheitsränge |
| Vergleich `value_relation`, `domain_relation` | Getrennte Wert-/Definitionsbereichsrelation, ggf. `NOT_APPLICABLE` bei leerem Definitionsbereich |
| Vergleich `left`, `right` | Normalisierte Zähler/Nenner, erhaltene `excluded_polynomial` und `empty_domain` |
| Vergleich `domain_certificate` | Begründung, Restpolynome, Wurzelangaben, ggf. modulares Zertifikat oder rationaler Zeuge und ausgeschlossene Seite |
| Erfahrung `events`, `active_records`, `records`, `corrections` | Ereigniszahl, aktive Einträge, historische Datensätze und Korrekturverknüpfungen |

IDs und große Zahlen als Zeichenketten verlustfrei weiterverarbeiten. Auch numerische UInt64-JSON-Felder können die exakte Ganzzahlreichweite eines JavaScript-`Number` überschreiten. Für reine JSON-Befehle (nicht die Terminaldialoge `rules-approve`/`rules-activate`) ist eine formatierte Darstellung mit Python 3 möglich: `bruce ... | python3 -m json.tool`. In automatisierten Shell-Pipelines `set -o pipefail` benutzen, damit ein erfolgreicher JSON-Formatter einen CLI-Fehler nicht verdeckt.

## Gemeinsame Sitzungsoptionen

Nur `research`, `grid`, `grid-batch`, `grid-resume`, `compare` und `compatibility` nehmen diese Optionen an:

| Option | Standard | Verhalten |
| --- | --- | --- |
| `--verification` | `none` | `none` oder `replay` |
| `--execution-cancellation` | `none` | Zusätzliche Import-Abbruchpunkte, Werte unten |
| `--verification-cancellation` | `none` | Zusätzliche Prüf-Abbruchpunkte, Werte unten |
| `--serialization-cancellation` | `none` | `digest-checkpoints` oder `grid-record-checkpoints` |
| `--max-replay-bytes` | nicht begrenzt durch diese Option | UInt64; Bundlegröße unmittelbar vor Replay; 0 blockiert jeden nichtleeren Bundle-Replay |
| `--max-serialized-bytes` | nicht begrenzt durch diese Option | UInt64; Bundlegröße nach Aufbau und vor Replay; 0 blockiert jedes nichtleere Bundle |
| `--max-session-checkpoints` | kein gemeinsames Konto | UInt64; gemeinsames Konto der vorhandenen Prüfpunkte über die Phasen; 0 scheitert beim ersten gezählten Punkt |

Alle drei Budgetoptionen und alle Abbruchwerte außer `none` verlangen `--verification replay`. Ohne explizite Budgets bleiben die internen Format-/Strategiegrenzen bestehen. Bytewerte bezeichnen Bytes, nicht Kilobytes und nicht frei wählbare Suffixe wie `1M`.

### Ablauf mit Replay

`CREATED → EXECUTING → SERIALIZING → REPLAYING → READY`.

Die CLI rechnet die Aufgabe aus, serialisiert ein Bundle, rekonstruiert es und vergleicht die Ergebnisse. Erst danach exportiert sie bei `--output`. Erfolgreiches JSON ist `{"execution":{...},"result":{...}}`. Ohne Replay ist das native Ergebnis direkt an der Wurzel.

`execution.replay_verified=true` sagt, dass das Ergebnis reproduziert wurde. `execution.result_complete` ist genau dann false, wenn der native Status `INCOMPLETE` ist. Es bedeutet nicht „Formel allgemein bewiesen“: Auch `UNDECIDED`, `NO_CANDIDATE` oder `STOPPED` sind damit keine unvollständigen Ausführungen.

Die `execution.contract`-Version hängt von den benutzten Funktionen ab (v1 bis v15); die jüngste Core-Version schreibt nicht pauschal immer v15. v14 bezeichnet das gemeinsame Prüfpunktkonto, v15 den Serialisierungsbytevertrag. Mehrere aktivierte Funktionen werden durch die implementierte Priorität in eine gemeinsame Hülle aufgenommen.

Eine Ausnahme innerhalb dieser Phasen führt zu `FAILED`, `failed_at`, `cause`, Phasenverlauf und optional Budgetverbrauch im Fehler-JSON. Fehler beim vorherigen Datei-/Optionslesen sowie beim späteren Export haben dagegen keine solche Sitzungsfehlerhülle. Bei einer gescheiterten Sitzung wird kein Bundle exportiert.

### Was die Budgets nicht begrenzen

`max-operations` misst logische, strategieabhängige Schritte. `max-session-checkpoints` zählt Beobachtungspunkte, nicht identische mathematische Kosten. Bytebudgets prüfen ein bereits aufgebautes Bundle und verhindern keine vorherige Speicherallokation. Keine dieser Optionen garantiert eine maximale Laufzeit, maximalen Prozess-RAM oder eine maximale Signallatenz. Das optionale Grid-Gesamtkonto ist ein gesondert beschriebenes Kostenmodell.

## Signalverhalten

Forschung, Vergleich, Kompatibilität, Grid-Batch und Grid-Resume registrieren SIGINT/SIGTERM für vorhandene lokale Suchabbruchpunkte. Ein erkannter Suchabbruch kann ein normales, exportierbares `INCOMPLETE`-Ergebnis mit Exit 0 liefern. Ein Signal an einer Prüf-/Importgrenze kann die gesamte geprüfte Sitzung mit Exit 2 abbrechen. Timing und aktive Phase entscheiden.

Einzelnes `grid`, `calculate`, `coordinate`, eigenständiges `replay`, `inspect-run` und Erfahrungskommandos besitzen keine allgemeine Zusage eines exportierbaren Signal-Teilbefunds. Betriebssystemterminierung kann andere Exitcodes liefern. Die kurzen Hilfezeilen über Ctrl+C sind nicht als Zusage für jeden Befehl zu lesen.

### Import-Abbruchpunkte (`--execution-cancellation`)

| Wert | Zulässiger Einsatz |
| --- | --- |
| `none` | Kein zusätzlicher Importcallback |
| `experience-checkpoints` | Exakte oder Grid-Forschung mit `experience-order` |
| `grid-source-checkpoints` | Grid-Forschung |
| `grid-and-experience-checkpoints` | Grid-Forschung mit `experience-order` |
| `compatibility-source-checkpoints` | Kompatibilität mit Forschungsquelle, also `--from-run` |
| `resume-import-checkpoints` | Grid-Resume |

Diese Optionen machen vorgelagerte CLI-Dateilesevorgänge nicht automatisch unterbrechbar; sie betreffen die nachgelagerte Verarbeitung/Auditierung in der Ausführung.

### Prüf-Abbruchpunkte (`--verification-cancellation`)

| Wert | Zulässiger Einsatz |
| --- | --- |
| `none` | Keine zusätzliche signalbasierte Prüfabbruchkontrolle |
| `phase-boundaries` | Alle sitzungsfähigen Aufgaben, an den eingebauten Prüfgrenzen |
| `grid-query-boundaries` | Grid-Batch und Grid-Resume |
| `research-checkpoints` | Int64-, exakte und Grid-Forschung |
| `comparison-checkpoints` | Bruchvergleich |
| `compatibility-checkpoints` | Z-Kompatibilität |

Die Option kann nur einmal vorkommen, daher keine Listen oder Kombination mehrerer Werte angeben.

### Serialisierung (`--serialization-cancellation`)

`digest-checkpoints` beobachtet die äußere Prüfsummenbildung blockweise, im gemeinsamen Digestpfad mit 65536-Byte-Blöcken. `grid-record-checkpoints` ist ausdrücklich nur für Grid-Batch, Grid-Resume und Grid-Forschung erlaubt und beobachtet deren vorhandene Datensatz-/Kopierpfade. Beide lassen sich nicht durch doppelte Optionsangabe kombinieren.

Das gemeinsame Sitzungsbudget nutzt darüber hinaus interne Kopierprüfpunkte auch bei ExactResearch, Compatibility und FractionComparison. Das macht `grid-record-checkpoints` nicht als explizite CLI-Option für diese Typen zulässig. Einzelne große Aufbau-/Kopieroperationen können weiterhin ohne innere Beobachtung stattfinden.

## Fehlerbehandlung und typische Ursachen

| Meldung / Präfix | Abhilfe oder Interpretation |
| --- | --- |
| `UnknownCommand`, `UnknownOption` | Schreibweise und Befehlszugehörigkeit im Optionsindex prüfen |
| `DuplicateOption`, `MissingOptionValue`, `MissingOption` | Optionen genau einmal mit separatem Wert angeben |
| `InvalidInteger`, `InvalidExactInteger` | Dezimale Ganzzahl bzw. exakten Bruch ohne Leerzeichen verwenden |
| `InvalidSampleHeader`, `InvalidSampleRow`, `EmptySamples` | CSV-Grammatik prüfen |
| `DuplicateOrOverlappingSampleInput`, `DuplicateTrainingInput`, `HoldoutOverlapOrDuplicate` | Doppelte Inputs entfernen; Holdout disjunkt wählen |
| `CannotReadFile`, `InputByteLimit` | Pfad, Berechtigung und passende Importgröße prüfen |
| `ExportExistsOrUnavailable` | Neuen Zielnamen verwenden oder Ziel-Dateisystem/Berechtigungen prüfen |
| `ExportCommitDurabilityUnknown` | Ziel wurde bereits exklusiv verlinkt, aber die Synchronisation des Zielverzeichnisses scheiterte; Exit 2 bedeutet hier nicht, dass der Export fehlt. Existenz und Inhalt vor einem erneuten Versuch prüfen, nicht blind unter anderem Namen wiederholen |
| `IntegerBitLimit`, `IntegerOverflow` | Zahlenbereich beachten; je Pfad Fehler oder Teilbefund |
| `InvalidExperienceConfiguration` | Snapshot und `experience-order` zusammen verwenden |
| `ExperienceRequiresCompletedHoldoutRun` | Vollständig geprüfte unabhängige Quelle mit Holdout bereitstellen |
| `GridResumeRequiresIncompleteSource` | Nur unvollständigen Grid-Stapel fortsetzen |
| `SerializationByteBudgetExceeded` | SERIALIZING wurde abgeschlossen, Bundle überschreitet Grenze |
| `ReplayByteBudgetExceeded` | Bundle vor Replay zu groß |
| `SessionCheckpointBudgetExceeded` | Gemeinsames Prüfpunktkonto erschöpft; Phase und Verbrauch im Fehler beachten |
| `InvalidEvidence`, `ReplayMismatch`, `InvalidExperienceDigest` | Datei verändert, beschädigt oder semantisch nicht reproduzierbar; unveränderte Quelle verwenden |

Die vollständige aus Quellen gewonnene Liste der Diagnose-Literale steht im [Diagnosekatalog](source-index.md). Sie enthält auch interne Replay-/Vertragsfehler; konkrete Meldungen können Werte oder Betriebssystemtexte anhängen. Parserprüfreihenfolge beeinflusst, welcher Fehler bei mehreren gleichzeitigen Problemen zuerst erscheint.

## Regelprüfung: separate Ausgabe- und Fehlerverträge

`rules-propose` liefert `proposal` und `approved:false`. `rules-approve` schreibt zunächst einen Klartextdialog; nach exakter Bestätigung und erfolgreichem Export folgen `approved_export:true`, `confirmation` und `proposal` als JSON. Ablehnung ist Exit 2, kein fachlicher Ergebnisstatus. Beide Befehle besitzen keine Sitzungs-/Budgetoptionen und erzeugen keine Forschungsbundles. [Alle Fehler und Dialogdetails](rule-review.md).

`rules-activate` führt nach Terminalbestätigung einen aktiven Versionswechsel aus. `rules-status` und die erfolgreiche Aktivierung liefern `rule-history-v1`. Das ist ein eigener Vertrag außerhalb des Sitzungscontrollers. [Felder, Schreibfehler und Grenzen](active-rules.md).

## Steuerung expliziter Übergabeausführungen

Ab Core 0.94 besitzen `experience apply-add` und `compatibility-apply` ein eigenes gemeinsames Prüfpunktkonto mit optionaler Signalprüfung. Es ist nicht der allgemeine Sitzungscontroller. [Zählmodell, Abbruchpriorität, Ausgabehülle und atomare Restarbeit](transfer-control.md).

Seit Core 0.95 verwenden auch `experience plan-add` und `compatibility-plan` diese Transfersteuerung.
