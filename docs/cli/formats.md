# Dateiformate und Zahlendarstellung

[Handbuch](README.md) · [Befehle](commands.md)

## Zahlen

CLI-Ganzzahlen sind dezimal, ohne Leerzeichen, `+`, Tausendertrennzeichen, Hexpräfix oder Exponentialnotation. Negative Zahlen sind nur bei vorzeichenbehafteten Werten zulässig. UInt64 reicht von 0 bis 18446744073709551615, Int64 von −9223372036854775808 bis 9223372036854775807. Adressberechnungen können schon vor der Eingabeobergrenze überlaufen.

`bigint` akzeptiert dezimale ganze Zahlen. `rational` zusätzlich `Zähler/Nenner`, zum Beispiel `-3/4` oder `6/-8`. Nenner 0 ist ungültig. Brüche werden gekürzt, das Vorzeichen wird normalisiert; `2/2` wird als `1` ausgegeben. Dezimalzahlen wie `0.5` sind kein unterstütztes Eingabeformat: `1/2` verwenden. Die Bitgrenze gilt auch für die zunächst eingelesenen Komponenten, nicht erst nach dem Kürzen. Übermäßig lange Texte werden vor großen GMP-Allokationen abgewiesen.

Große und exakte Werte erscheinen überwiegend als JSON-Strings. Int64-Forschung gibt beispielsweise ihre Koeffizienten dagegen als JSON-Zahlen aus. Verbraucher dürfen deshalb weder pauschal Float-Konvertierung noch einen einheitlichen Typ für alle numerischen Felder voraussetzen. [Beispielausgaben](examples.json) zeigen die tatsächlichen Typen.

## CSV-Forschungspaare

```csv
x,y
0,3
1,5
4,11
```

`x` ist Eingabe, `y` erwartete Ausgabe. Erste Zeile exakt `x,y`, danach genau zwei numerische Felder pro Zeile. Keine Anführungszeichen, Kommentare, Leerzeilen, Spaltennamenübersetzungen oder BOM. LF und CRLF sind unterstützt; ein abschließender Zeilenumbruch ist zulässig. Eine zusätzliche leere Datenzeile ist ungültig.

Eine angegebene Datei muss 1..4096 Paare enthalten. Holdout weglassen, wenn keiner existiert; kein leeres Holdout-CSV angeben. Jede Datei ist höchstens 1048576 Bytes groß. Exakte Forschung prüft zusätzlich die zusammengezählte normalisierte Sampletextgröße gegen 1048576 Bytes. Alle X-Werte müssen sowohl innerhalb der Dateien als auch über Training und Holdout hinweg eindeutig sein. Exakte Brüche werden vor dem Vergleich normalisiert: `1/2` und `2/4` sind derselbe X-Wert.

Rationales Beispiel:

```csv
x,y
0,1/2
1/2,3/4
1,1
```

Training bestimmt Kandidaten; Holdout prüft sie an zusätzlichen Inputs. Die fachliche Unabhängigkeit der Daten kann BRUCE nicht allein durch disjunkte X-Werte garantieren.

## Exact-Affine-Evidenzkette

`bruce evidence create` speichert ein eigenes, SHA-256-gebundenes `BRUCE_EVIDENCE_CHAIN_V1`-Bundle. Header und feste Felder stehen in genau dieser Reihenfolge; `N` ist jeweils eine dezimale Byteanzahl, keine Zeilenzahl:

```text
BRUCE_EVIDENCE_CHAIN_V1
<SHA-256 des vollständigen Bodys>
contract affine-evidence-chain-v1
independence declared-disjoint-v1
countercheck_policy affine-inverse-v1
countercheck_scope short-circuit-training-then-holdout-v1
training_csv_bytes <N>
<genau N ursprüngliche Trainings-CSV-Bytes>
holdout_csv_bytes <N>
<genau N ursprüngliche Holdout-CSV-Bytes>
native_bundle_bytes <N>
<genau N Bytes eines BRUCE_EXACT_BUNDLE_V1>
RESULT
<kanonisches Ergebnis-JSON>
```

Die beiden CSV-Blöcke enthalten die ursprünglichen Bytes einschließlich ihrer Zeilenenden, nicht normalisierte oder neu formatierte CSVs. Beide Eingabedateien sind auf je 1048576 Bytes begrenzt; die Chain unterliegt zusätzlich der allgemeinen Bundlegrenze von 33554432 Bytes. Der eingebettete Native-Task muss die Exact-Route `affine`/`enumerate` in `bigint` oder `rational` und genau die aus den CSVs geparsten Trainings-/Holdout-Samples enthalten. Die x-Werte müssen auch dateiübergreifend eindeutig sein.

`declared-disjoint-v1` bedeutet eine lokale Deklaration zusammen mit technisch geprüfter x-Disjunktheit. Es belegt weder externe Datenherkunft noch fachliche oder statistische Unabhängigkeit. `countercheck_scope short-circuit-training-then-holdout-v1` beschreibt den tatsächlichen kurzschließenden Kontrollfluss, keine Vollabdeckung aller Kandidaten oder Samples. Jeder erreichte Kandidat wird zuerst an Training geprüft; die erste Kontradiktion beendet seine Prüfung. Die inverse Gegenprüfung läuft nur nach einer passenden Vorwärtsauswertung. Holdout beginnt nur nach bestandener Trainingsprüfung und wenn Resultat-/Budgetfluss es erreicht. Budgets können innerhalb eines Kandidaten stoppen; `INCOMPLETE` ist kein Abdeckungsnachweis. Status, Stopgrund, Zähler und Trace dokumentieren den tatsächlichen Verlauf; wegen Budgets nicht enumerierte Kandidaten wurden nicht geprüft. `bruce evidence replay` liest nur diese lokale Chain, prüft Header, Digest, feste Felder, Längen, CSV-/Native-Bindung sowie das rekonstruierte Ergebnis und gibt kanonisches JSON zurück. Digest und Replay sind Integritäts-/Reproduktionskontrollen, kein allgemeiner mathematischer Beweis und keine externe Prüfung. Die Chain ist kein `ApplicationTask`-Bundle für `bruce replay`, `inspect-run`, `assess-run` oder `gate-run`.

## Sprachentwurf und Sprachlauf

`language interpret` akzeptiert genau zwei externe CSV-Pfade, `--train` und `--holdout`. Jede Datei wird vor der Interpretation mit der bestehenden Grenze von 1048576 Bytes gelesen und als Originalbytes samt SHA-256-Digest im Entwurf gebunden. Für den Satz `Untersuche @train mit @holdout auf rationale Formeln.` heißen die internen Aliase `train` und `holdout`. Für `Untersuche @left und @right auf rationale Formeln.` werden dieselben beiden Dateipfade in dieser Reihenfolge als `left` und `right` gebunden. Andere Aliasse oder aus Text abgeleitete Dateipfade gibt es nicht. Die Mini-Grammatik normalisiert ASCII-Großschreibung und ASCII-Whitespace; andere Formulierungen sind nicht unterstützt. Eingabetext muss gültiges UTF-8 sein.

Der erste Satz ergibt `UNDERSTOOD` mit genau einer kanonischen Alternative `rational-portfolio-v1`. Der zweite ergibt `AMBIGUOUS` mit `train=left;holdout=right` und `train=right;holdout=left`. Erst `language select` trägt eine der beiden IDs in einen neuen Entwurf ein; ohne diese Wahl verweigert `execute` den Lauf. Nicht unterstützter Text wird ohne Entwurfsexport abgewiesen. Die JSON-Statuslabels sind großgeschrieben; die internen Statusfelder im Dateiformat sind eigene kanonische Werte.

`BRUCE_LANGUAGE_DRAFT_V1` ist ein SHA-256-gebundenes, längenpräfixiertes kanonisches Byteformat. Es enthält Grammatikversion, Originaltext, normalisierten Text, beide ursprünglichen CSV-Bytes und Digests, Anfragegrenzen, Status, Alternativen und gegebenenfalls die ausgewählte ID. `select` und `execute` nehmen nur dieses Format an; Text- oder JSON-Dateien gelten nicht als Entwurf. `BRUCE_LANGUAGE_RUN_V1` bindet den vollständigen Entwurf, Rollen-Digests, ein natives Core-Bundle mit Digest, die Sprachphasen, Erklärung und die gerenderte Core-Evidenz. Neue `execute`-Exporte schreiben darüber `BRUCE_MOD_RUN_V1`: ein SHA-256-umschlossenes längenpräfixiertes Format mit Manifest `language`/`language-mod-v1`/`bruce-mod-api-v1`, beiden Codeckennungen, Entwurf und innerem V1-Lauf samt einzelnen Inhaltsdigests. `language replay` prüft Manifest, Codecs, Digests, Entwurf-zu-Lauf-Bindung und das native Core-Replay. Direkte historische `BRUCE_LANGUAGE_RUN_V1`-Dateien bleiben ein read-only Kompatibilitätsweg. Es schreibt keine neue Datei; die gespeicherten Laufbytes bleiben unverändert.

Die Formatdecoder begrenzen ihren Payload auf 128 MiB zuzüglich Header/Digest. Die CLI verwendet den bestehenden begrenzten Leser und den exklusiven Export mit einer engeren 32-MiB-Dateigrenze; der eingebettete native Core-Bundlepfad ist ebenfalls auf 32 MiB begrenzt. Das ist eine Bytegrenze, keine allgemeine Prozessspeicher- oder Laufzeitgarantie. Die direkte Ausgabe `language-execution-v1` beziehungsweise `language-replay-v1` umschließt unter `result` den `language-run-v1`-Befund; darin bleibt `core` die geprüfte native Sitzung. Ein natives `INCOMPLETE` bleibt `INCOMPLETE` und wird nicht als Beweis oder Gate-Freigabe umgedeutet.

## NanoCell-Kontext und Bundle

NanoCell-`--core` ist ausnahmsweise **hexadezimal**: genau 16 Ziffern für acht Bytes, ohne `0x`, Zwischenräume oder Trennzeichen. Beispiel: `1020a54050607080` mit `--active-index 2` wählt `a5`. Andere CLI-Ganzzahlen bleiben dezimal. `--context-file` wird als rohe Bytefolge mit höchstens 4096 Bytes gelesen, nicht als Pfadbezeichnung oder JSON; ein abschließender Zeilenumbruch verändert den SHA-256-Digest. Für `masked-match-v1` und `rotate-match-v1` lautet der Referenzinhalt exakt `nanocell-reference-context-v1`, für `context-conflict-v1` exakt `nanocell-reference-context-conflict-v1`.

Mit `--output` entsteht ausschließlich ein exklusiv veröffentlichter `BRUCE_MOD_RUN_V1`-Umschlag. Er enthält das Manifest `nanocell`/`nanocell-mod-v1`/`bruce-mod-api-v1`, die kanonische NanoCell-Anfrage mit Digest sowie den unveränderten inneren `BRUCE_NANOCELL_RUN_V1` samt Digest. Die Endung ist frei wählbar (`.nanocell` ist lesbar); die Datei ist weder JSON noch ein allgemeines `.bruce`-Anwendungsbundle. `bruce nanocell replay DATEI` akzeptiert neue äußere Umschläge bis 12 KiB und prüft Manifest, Codecs, Digests, kanonische Anfrage-zu-Run-Bindung und erneute Ausführung. Direkte historische `BRUCE_NANOCELL_RUN_V1`-Dateien bleiben read-only kompatibel und behalten ihre 5703-Byte-Grenze. Der NanoCell-Pfad gehört nicht zu `bruce replay`, `inspect-run`, `assess-run` oder `gate-run`. Innerer und äußerer Lauf enthalten Kontextbytes; nicht mit öffentlichen Daten teilen, wenn der Kontext vertraulich ist.

## Kandidaten-CSV für Kompatibilität

```csv
id,domain,layer,x,y,value
1,demo,0,2,6,64
2,demo,0,4,3,64
3,demo,0,1,1,-1
```

| Feld | Typ / Bedeutung |
| --- | --- |
| `id` | Eindeutige UInt64-Kandidatenkennung |
| `domain` | Gemeinsame Domain-ID, keine Zahlendomänenauswahl für GMP |
| `layer` | UInt64; alle Kandidaten müssen dieselbe Schicht verwenden |
| `x`, `y` | UInt64-Koordinaten; nicht pauschal auf das lokale 8×8-Grid begrenzt |
| `value` | Int64-Wert für Regelvergleiche |

1..4096 Kandidaten, höchstens 1048576 Bytes. `z` ist keine CSV-Spalte: Es wird durch die globale Paarungsfunktion berechnet und muss UInt64-genau darstellbar sein. Zeilen werden nach ID sortiert; Reihenfolge im Input löst Gleichrangigkeit nicht auf. Alle Kandidaten gehören zur selben Domain und Schicht.

Bezeichner für Domain, Snapshot, Regeln und Evidenz: 1..64 ASCII-Zeichen aus `A-Z a-z 0-9 _ - .`; der einzelne Bindestrich `-` ist reserviert und kein gültiger Bezeichner. CSV-Felder besitzen kein Quoting und kein Whitespace-Trimmen.

## Regeldatei `BRUCE_RULES_V1`

```text
BRUCE_RULES_V1
snapshot demo-v1
rule prefer positive value eq 64 0 10 manual -
rule exclude reject value lt 0 0 0 manual -
rule context-ok context layer eq 0 0 0 manual -
```

Header und Snapshotzeile sind Pflicht, danach 0..128 Regeln. Bis zu 1048576 Bytes. Zeilen werden in Whitespace-getrennte Wörter zerlegt; keine Kommentare und keine Leerzeilen als Regeln. Jede Regel hat genau zehn Felder:

```text
rule ID KIND FIELD OP ARGUMENT REMAINDER WEIGHT manual EVIDENCE
```

| Feld | Zulässige Werte |
| --- | --- |
| ID | Eindeutiger Regelbezeichner |
| KIND | `positive`, `reject`, `negative`, `context` |
| FIELD | `x`, `y`, `z`, `value`, `layer` |
| OP | `eq`, `ne`, `lt`, `le`, `gt`, `ge`, `mod_eq` |
| ARGUMENT | Int64-Vergleichswert; bei `mod_eq` positiver Modul |
| REMAINDER | 0 für normale Vergleiche; bei `mod_eq` 0..ARGUMENT−1 |
| WEIGHT | −1000000..1000000 für `positive`, sonst exakt 0 |
| Quelle | Ausschließlich `manual` |
| EVIDENCE | Bei `negative` gültiger Bezeichner, sonst exakt `-` |

`positive`: Bei Treffer wird das Gewicht auf den Score addiert, negative Gewichte sind möglich. `reject`: Treffer schließt hart aus. `negative`: Treffer schließt mit einer angegebenen negativen Evidence-ID aus. Die ID selbst wird nicht extern nachgeprüft. `context`: Jeder Kontextvergleich muss zutreffen; ein Nichttreffer macht den Kandidaten unzulässig. Alle Regeln werden in Dateireihenfolge geprüft, solange das Budget reicht.

Für `mod_eq` wird der euklidische Rest verwendet, auch bei negativen Int64-Werten. Nur vollständig geprüfte und kontextpassende Kandidaten ohne Ausschluss sind auswählbar. Höchster Score gewinnt; mehrere gleiche Maximalwerte bleiben `AMBIGUOUS`.

Je 64 sortierte Kandidaten bilden eine Maskengruppe. `complete_mask`, `context_role_mask`, `positive_match_mask`, `hard_reject_mask`, `negative_evidence_mask` und `eligible_mask` haben verschiedene Rollen. Letztere entspricht `complete & context & ~reject & ~negative`; unbenutzte Bits bleiben ausgeschlossen.

## Koeffizientenlisten

`compare` erwartet kommaseparierte rationale Koeffizienten ohne Leerzeichen in aufsteigender Potenzreihenfolge. `-1,0,1` steht für x²−1, `-1,1` für x−1, `1` für die Konstante 1. Jede Liste besitzt mindestens einen Wert und höchstens 65 Einträge. Leere Felder und nachgestellte Kommata sind ungültig. Führende Koeffizienten mit Wert 0 können durch Normalisierung verschwinden; die ursprünglichen Definitionsausschlüsse bleiben fachlich relevant.

## Bundles `.bruce` und Snapshots `.experience`

Die Dateiendung dient Menschen; die Decoderauswahl erfolgt durch den Inhalt. Bundles enthalten einen versionierten Textheader, eine SHA-256-Prüfsumme und einen versionsabhängigen Payload mit Aufgabe, logischen Entscheidungen und Ergebnis. Verschachtelte Inhalte werden teilweise durch Byteanzahlen abgegrenzt. Dateien nicht als CSV oder JSON behandeln und keine Zeilenenden umschreiben.

Allgemeine Bundle-/Dateigrenze: 33554432 Bytes (32 MiB). Spezifische Importe können enger sein: CSV/Regeln/Kandidaten/Erfahrung 1 MiB, Grid-Forschungsquelle 2 MiB. Eine gültige große Datei kann deshalb bei einem engeren Import trotzdem abgelehnt werden.

Erfahrungssnapshots enthalten versionierte Ereignisketten, eingebettete Quellnachweise und Korrekturbeziehungen. Mit `experience inspect` prüfen, nicht mit `replay`. Bundles mit `replay` oder `inspect-run` prüfen. Eine passende Prüfsumme allein ersetzt nicht den semantischen Replay.

`--output` schreibt über eine private temporäre Datei im Zielverzeichnis und veröffentlicht ohne Überschreiben eines vorhandenen Zielnamens. Fehler beim Erstellen, Schreiben, Synchronisieren oder Veröffentlichen führen zu Exit 2. Existierende Dateien werden nicht überschrieben. Verzeichnisse werden nicht automatisch erzeugt. Die Implementierung synchronisiert die Datei; daraus folgt keine allgemeine Zusage für sämtliche Dateisystem-/Stromausfallszenarien. Bei `ExportCommitDurabilityUnknown` ist der Zielname bereits verlinkt; sein Inhalt kann sichtbar sein, obwohl die Verzeichnissynchronisation scheiterte. [Details](local-files.md#fehler-und-aufräumen).

## Regelvorschläge und bestätigte Exporte

`BRUCE_RULE_PROPOSAL_V1` enthält zwei kanonische Regelfassungen in einem eigenen gequoteten Textformat mit äußerem Digest. Es ist weder JSON noch ein mathematisches Replaybundle. Der bestätigte Export ist wieder eine gewöhnliche `BRUCE_RULES_V1`-Datei. [Byteaufbau, Grenzen und Digestbindung](rule-review.md#paketformat-und-digestbindung).

## Aktive Regelspeicher

`BRUCE_RULE_STAGE_V1` und `BRUCE_RULE_HISTORY_V1` binden Vorschläge und bestätigte Wechsel an Speicherpfad, Generation und Journalverlauf. Die Historie wird durch `rules-activate` nach erneuter Prüfung unter Schreibsperre atomar ersetzt; sie ist kein exklusiver Bundleexport. [Byteformat, Grenzen und Schreibvertrag](active-rules.md#speicherformat-und-schreibvertrag).

## Erfahrungsübergabepläne

`BRUCE_EXPERIENCE_ADD_TRANSFER_V1` bindet Quelldigest, Basisdigest und zwei Bytegrenzen in einem SHA-256-umschlossenen kanonischen Textformat. Der Plan enthält keine Eingabebundles. [Exaktes Format](experience-transfer.md#planformat).

## Z-Auswahlpläne

`BRUCE_COMPATIBILITY_TRANSFER_V1` bindet Quelldigest, kanonischen Regeldigest, Auswertungsstelle und Budgets in einem SHA-256-umschlossenen Textkörper. Der Plan enthält weder Quelle noch Regeln. [Exaktes Format](compatibility-transfer.md#planformat).
