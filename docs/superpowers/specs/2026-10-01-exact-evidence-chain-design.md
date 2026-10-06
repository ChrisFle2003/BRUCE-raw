# Exact Evidence Chain V1 – Design

**Status:** Zur menschlichen Prüfung vor der Implementierungsplanung  
**Datum:** 01.10.2026  
**Geltungsbereich:** Ein zusätzlicher lokaler Evidenzpfad um den bestehenden exakten Affine-Research-Lauf. Historische Bundleformate und andere Research-Familien bleiben unverändert.

## Ziel und Erfolgskriterium

BRUCE soll für einen eng definierten Exact-Affine-Lauf nicht nur Trainings- und Hold-out-Paare zählen, sondern deren **konkrete Eingabebytes**, Rollen, technische Disjunktheit und den genauen Gegenprüfungsbereich in einem replaybaren Artefakt binden.

Ein erfolgreicher Lauf beweist danach ausschließlich:

1. die eingebetteten CSV-Eingaben sind syntaktisch gültig;
2. Trainings- und Hold-out-x-Werte sind disjunkt;
3. der eingebettete Exact-Affine-Lauf verwendet genau diese Samples;
4. der unveränderte Exact-Lauf der gebundenen, kurzschließenden Trainings-/Holdout-Policy folgt, ohne eine vollständige Prüfung aller Kandidaten oder Samples zu behaupten;
5. das Ergebnis und der Trace durch Replay reproduzierbar sind.

Das Artefakt beweist **nicht** die Herkunft der Dateien, die fachliche/statistische Unabhängigkeit der Hold-out-Daten oder eine allgemeine mathematische Formel. `VERIFIED_ON_SAMPLES` bleibt kein allgemeiner Beweis, und kein Ergebnis öffnet Lern-, Veröffentlichungs- oder Regelaktivierungsgates.

## Abgrenzung

Nicht Teil dieses Pakets sind:

- ein allgemeiner `EvidenceGateV1`, externe Prüfer oder eine vollwertige Counterexample-Finder-Runtime;
- Erweiterungen anderer Strategien, Grid, Portfolio, NanoCell oder Sprache;
- neue Regelaktivierung, Experience-Promotion oder Veröffentlichung;
- freie Textinterpretation, Netzwerkzugriff oder Cloud-Speicher;
- Behauptungen über kryptografische Suche, BSGS, Mining oder Private Keys.

## Bestehende Basis

Der Pfad baut auf `ExactResearchTask` mit `strategy=affine` und dem bestehenden `BRUCE_EXACT_BUNDLE_V1` auf. Dieser Lauf besitzt bereits exakte Arithmetik, getrennte `training`/`holdout`-Samples, Vorwärtsauswertung plus inverse Gegenprüfung, kanonisches Bundle und deterministisches Replay.

Die neue Schicht darf diesen nativen Codec weder umdefinieren noch V1 migrieren. Sie kapselt nur einen nativen Exact-Affine-Bundle, dessen Task anschließend gegen die eingebetteten CSV-Daten verglichen wird.

## Öffentliche Oberfläche

Die CLI erhält einen separaten, eng begrenzten Namensraum:

```text
bruce evidence create \
  --train TRAIN.csv --holdout HOLDOUT.csv \
  [bestehende Exact-Affine-Suchoptionen] \
  --output CHAIN.bruce

bruce evidence replay CHAIN.bruce
```

`evidence create` akzeptiert ausschließlich Optionen, die der bestehende Exact-Affine-Lauf benötigt. Er akzeptiert weder `--strategy` noch Portfolio-/Grid-/Experience-/Regeloptionen. Die Strategie ist fest `affine`; zulässige Domänen sind ausschließlich `bigint` und `rational`, Standard ist `bigint`. Die vorhandenen Exact-Standardgrenzen werden übernommen und im eingebetteten Native-Bundle festgeschrieben.

`evidence replay` benötigt keine Originaldateien. Es liest ausschließlich das lokale Chain-Artifact und schreibt keine Regel-, Experience- oder Veröffentlichungsdatei.

## Format: `BRUCE_EVIDENCE_CHAIN_V1`

Das Format ist ein Digest-geschütztes Längenformat. Alle Längen sind dezimale Bytezahlen; eingebettete CSV- und Native-Bundle-Bytes werden unverändert gespeichert, nicht umformatiert.

```text
BRUCE_EVIDENCE_CHAIN_V1
<sha256 des vollständigen Bodys>
contract affine-evidence-chain-v1
independence declared-disjoint-v1
countercheck_policy affine-inverse-v1
countercheck_scope short-circuit-training-then-holdout-v1
training_csv_bytes <N>
<genau N Trainingsbytes>
holdout_csv_bytes <N>
<genau N Hold-out-Bytes>
native_bundle_bytes <N>
<genau N Bytes eines BRUCE_EXACT_BUNDLE_V1>
RESULT
<kanonisches Ergebnis-JSON>
```

`independence` darf in V1 ausschließlich `declared-disjoint-v1` sein. Dieser Wert bezeichnet die lokale Nutzerdeklaration plus technisch geprüfte Disjunktheit; er darf nicht `independent`, `proven` oder ähnlich heißen.

`countercheck_policy` und `countercheck_scope` sind feste, exakt geschriebene Vertragswerte. Der Replayer lehnt unbekannte, doppelte, fehlende, umgeordnete oder anders geschriebene Werte ab. `short-circuit-training-then-holdout-v1` beschreibt den unveränderten Exact-Kontrollfluss: Jeder erreichte Kandidat wird zuerst an Training geprüft; die erste Kontradiktion beendet seine Prüfung. Die inverse Gegenprüfung läuft nur nach einer passenden Vorwärtsauswertung. Holdout beginnt nur nach bestandener Trainingsprüfung und wenn Resultat-/Budgetfluss es erreicht. Budgets können auch innerhalb eines Kandidaten stoppen; nicht enumerierte Kandidaten wurden nicht geprüft. `INCOMPLETE` ist kein Abdeckungsnachweis. Weder der Scope noch ein erfolgreicher Replay behaupten, dass jeder Kandidat oder alle Samples geprüft wurden.

## Ablauf

### Create

1. Trainings- und Hold-out-Datei bis zu den bestehenden Importgrenzen lokal lesen.
2. Beide Dateien mit dem bestehenden Exact-CSV-Parser in der gewählten exakten Domäne parsen.
3. Bestehende Validierung für nichtleere und über alle x-Werte disjunkte Samplemengen anwenden.
4. Einen `ExactResearchTask` mit Strategie `affine` und genau diesen Samples ausführen.
5. Den resultierenden `BRUCE_EXACT_BUNDLE_V1` erzeugen.
6. Ein Chain-Artifact mit Originalbytes, festen Vertragswerten, Native-Bundle und kanonischem Ergebnis erstellen; vor dem Export replaysicher prüfen.
7. Das Ergebnis ausgeben und das Artifact nur bei Erfolg lokal exportieren.

Der Status wird unverändert aus dem Exact-Run übernommen. Bei einem Budgetabbruch bleibt der Status `INCOMPLETE`; die Chain darf den Lauf nicht zu einer Samplebestätigung aufwerten.

### Replay

1. Header, Digest, Reihenfolge, Längen und genau einen `RESULT`-Abschnitt prüfen.
2. Beide eingebetteten CSVs in der aus dem Native-Bundle gelesenen Domäne parsen.
3. Nichtleere Mengen und x-Disjunktheit erneut prüfen.
4. Den Native-Bundle als `BRUCE_EXACT_BUNDLE_V1` lesen und replayen.
5. Prüfen, dass dessen Task Strategie `affine` verwendet und dass dessen kanonische Trainings-/Hold-out-Samples exakt den eingebetteten CSV-Samples entsprechen.
6. Prüfen, dass das Native-Ergebnis und der Ergebnisabschnitt der Chain bytegenau den durch Replay rekonstruierten Resultaten entsprechen.
7. Das rekonstruierte kanonische JSON liefern.

Ein Fehler ist ein Integritäts- oder Replayfehler, niemals eine Umklassifikation zu `VERIFIED_ON_SAMPLES`.

## Ergebnisvertrag

Die JSON-Antwort enthält zusätzlich zum nativen Exact-Ergebnis eine feste Evidenzbeschreibung:

```json
{
  "evidence_contract": "affine-evidence-chain-v1",
  "independence": "declared-disjoint-v1",
  "countercheck_policy": "affine-inverse-v1",
  "countercheck_scope": "short-circuit-training-then-holdout-v1",
  "training_samples": 3,
  "holdout_samples": 2,
  "result": {
    "engine": "bruce-exact-v1",
    "status": "VERIFIED_ON_SAMPLES",
    "general_proof": false
  }
}
```

Das Feld `result.general_proof` bleibt `false`. Die Chain besitzt keine Felder für `learn_gate`, `publish_gate`, Regelaktivierung oder menschliche Identität.

## Fehlerverhalten

| Fall | Erwartung |
|---|---|
| Fehlende, leere oder ungültige CSV | Bestehender Parser-/Importfehler; kein Export. |
| Gleicher x-Wert in Training und Hold-out | `HoldoutOverlapOrDuplicate`; kein Export. |
| Nicht-Affine Strategie im Native-Bundle | Evidenz-Replayfehler. |
| Manipulierte Vertragszeile, Länge, Body oder Digest | Evidenz-Replayfehler. |
| CSV-Samples stimmen nicht mit dem Native-Task überein | Evidenz-Replayfehler. |
| Native-Bundle ist manipuliert oder nicht replaybar | Vorhandener `InvalidEvidence`-/`ReplayMismatch`-Fehler wird propagiert bzw. präzise umschlossen. |
| Ergebnisabschnitt stimmt nicht mit Replay überein | Evidenz-Replayfehler. |
| Kandidat scheitert am Hold-out | Reguläres Ergebnis `REFUTED`, kein Prozessfehler. |

Fehlernamen und Prioritäten werden vor der Implementierung an bestehende CLI-/Bundle-Konventionen angepasst; neue Literale müssen im Diagnosekatalog und den Tests erscheinen.

## Abnahmetests

1. Rationaler positiver Affine-Lauf: `VERIFIED_ON_SAMPLES`, feste Evidenzfelder, Artifactexport, Replay ohne Quell-CSV.
2. Überlappender x-Wert: Abweisung vor Ausführung und ohne Export.
3. Hold-out-Gegenbeispiel: `REFUTED`; Replay ergibt exakt denselben Befund.
4. Rehashed Manipulation von Policy, Scope, Längenfeld und `independence`, fehlende/doppelte/umgeordnete Felder, übergroße Blöcke/Envelope und angehängte Payload: Parser bzw. Replay lehnt mit passender Diagnose ab, nicht nur wegen des äußeren Digests.
5. Rehashed Manipulation der eingebetteten CSVs oder von Native-Bundle/Result: Replay lehnt die semantische Abweichung ab; ein eigener Native-Test berechnet inneren und äußeren Digest neu und erreicht den nativen Replayvergleich. Ein gültiger replaybarer Nicht-Affine-Native-Bundle wird vom Chain-Vertrag abgewiesen.
6. Budgetabbruch: `INCOMPLETE` bleibt erhalten und setzt keinen höheren Evidenzstatus.
7. Historische `BRUCE_EXACT_BUNDLE_V1`-Fixtures und der bestehende `research`/`replay`-Pfad bleiben unverändert lesbar und grün.

## Kompatibilität und Sicherheit

- V1 ist additiv: weder `BRUCE_EXACT_BUNDLE_V1` noch dessen CLI oder JSON werden geändert.
- Verarbeitung ist lokal, deterministisch und ohne Netzwerkzugriff.
- Eingebettete Rohbytes haben dieselben sachlichen Größenbegrenzungen wie ihre Eingänge; zusätzlich erhält die Chain ein explizites Gesamtbytebudget nach bestehendem Bundle-Muster.
- Ein Digest bindet Inhalt, aber keine menschliche Identität und keine externe Datenherkunft.
- Das Artifact darf nur als Evidenz für den benannten, begrenzten Lauf interpretiert werden.

## Entscheidungslog

- Gewählt wurde ein separates Envelope statt `BRUCE_EXACT_BUNDLE_V2`, damit der stabile native Research-Codec und historische Bundles unverändert bleiben.
- Gewählt wurde `declared-disjoint-v1` statt einer behaupteten Unabhängigkeit, weil lokale Software die Provenienz oder fachliche Unabhängigkeit der Eingaben nicht beweisen kann.
- Die Countercheck-Policy wird fest an Exact-Affine gebunden; Verallgemeinerung auf weitere Strategien ist ein separates Paket.
