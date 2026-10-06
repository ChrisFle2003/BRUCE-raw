# BRUCE-Math: Hypothesen, Gegenpruefung und Replay

## Forschungspfad

```text
TaskSpec
  -> HypothesisCandidate
  -> exakte Strategieausfuehrung
  -> unabhaengige Gegenpruefung
  -> Hold-out-Instanz
  -> Replay
  -> Evidenzstatus
```

## Rollen der Komponenten

| Komponente | Darf | Darf nicht |
| --- | --- | --- |
| `HypothesisGeneratorV1` | Kandidaten vorschlagen | Wahrheit setzen |
| `ExactMathStrategyV1` | exakt rechnen | PublishGate oeffnen |
| `CounterexampleFinderV1` | Gegeninstanzen suchen | Suchraum verschweigen |
| `HoldOutRunnerV1` | unabhängige Instanzen pruefen | Trainingsdaten als Hold-out ausgeben |
| `ReplayRunnerV1` | identischen Lauf wiederholen | Versionen ersetzen |
| `EvidenceGateV1` | Status klassifizieren | neue Mathematik erfinden |

## Beweisgrenzen

- Ein Fit auf bekannte Punkte ist keine allgemeine Formel.
- Ein Roundtrip ist keine unabhängige Rekonstruktion.
- Float-Ergebnisse sind keine exakten BigInt-Beweise.
- Eine ganzzahlige Aussage braucht exakte Arithmetik und passende Rest-/Teilbarkeitspruefung.

## Replay-Vertrag

Ein Replay laedt ausschliesslich referenzierte Versionen und prueft:

```text
InputFingerprint
VersionBundle
RunBudget
StrategyId
TraceDigest
ResultDigest
```

Bei Abweichung lautet das Ergebnis `ReplayMismatch`, nicht `Verified`.

## Status

**Blueprint-Vorschlag.**
