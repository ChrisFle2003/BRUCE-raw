# BRUCE-Identitaet und Invarianten

## Produktziel

BRUCE ist eine integrierte mathematische Forschungs- und Loesungsmaschine. Ein sichtbares Research-Cockpit verbindet mehrere interne Strategien; es sind keine unabhaengigen Einzelapps.

```text
Benutzeraufgabe
  -> exakte und begrenzte Strategien
  -> Gegenpruefung und Replay
  -> evidenzgebundene Erklaerung
```

## Wahrheitsgrenze

**Einordnung Core 0.81:** Die folgende VERIFIED-Zielregel ist kein vorhandener universeller Statusvertrag und keine hinreichende Bedingung für einen allgemeinen mathematischen Beweis. Der Code trennt insbesondere `VERIFIED_ON_SAMPLES`, Replayerfolg, Vollständigkeit und begrenzte Bruchbeweise. Ebenso sind die unten geforderten umfassenden Zeit-/RAM-/CPU-Budgets noch keine durchgängige Runtimegarantie. Siehe [Blueprint-Fortschreibung](03-blueprint-consolidation.md).


Eine mathematische Aussage darf nur `VERIFIED` werden, wenn alle Punkte erfuellt sind:

1. Die Berechnung lief exakt im vereinbarten Zahlensystem.
2. Alle Eingabe-, Formel-, Regel- und Tabellenversionen sind referenziert.
3. Eine unabhaengige Gegenpruefung lief durch.
4. Ein Replay liefert denselben Ablauf und dasselbe Ergebnis.
5. Es gibt keine relevante offene Gegeninstanz innerhalb des vereinbarten Suchraums.

Ein Kandidat, ein guter Score oder ein Sprachmodelltext ist kein Beweis.

## Unveraenderliche Invarianten

### 1. Single-Triangle-Kernel

Das Dreieck bedeutet eine feste Koordinatenformel, keinen geometrischen Baum aus Kind-Dreiecken.

```text
value = triangle_formula(domain_id, layer, x, y, parameter, formula_version)
```

### 2. Adresse ist nicht Wert

Fuer nichtnegative Koordinaten kann Cantor-Paarung verwendet werden:

```text
z = ((x + y) * (x + y + 1)) / 2 + y
```

Im lokalen 8x8-Feld gilt:

```text
local_z = 8 * y + x
x = local_z mod 8
y = floor(local_z / 8)
```

`x^y` darf ein Wert sein, aber nie alleinige Adressidentitaet: `2^6` und `4^3` ergeben beide `64`.

### 3. Fuenf getrennte Identitaeten

| Identitaet | Pflichtfelder |
| --- | --- |
| Koordinate | `domain_id`, `layer`, `x`, `y`, `z` |
| Regel | `formula_version`, `rule_version`, `policy_version` |
| Token | `token_id`, `token_table_version` |
| Wert | Wert, Zahlentyp, Genauigkeit, Ergebnisstatus |
| Evidenz | `input_fingerprint`, `trace_id`, `evidence_id`, `experience_id` |

### 4. Lazy und budgetiert

Kein Modul darf Kandidatenraum, Grid oder Trace unbegrenzt materialisieren. Jeder Lauf besitzt ein explizites Budget fuer Zeit, CPU-Arbeit, Speicher, Cache, Kandidaten, Tiefe, Trace, Gegenpruefungen und externe Aufrufe.

### 5. Reproduzierbarkeit

Identischer Input plus identische Versionen, Regeln, Parameter und Evidenzsnapshot erzeugen identischen Ablauf, identische Auswahl und identisches Ergebnis.

### 6. Kein autonomes Gate

Strategien und Sprachmodelle duerfen Kandidaten erzeugen oder erklaeren. Sie duerfen niemals `LearnGate`, `PublishGate`, Sicherheitsgrenzen oder Budgets setzen.

### 7. Menschliche Regelhoheit

Aktive Regeln dürfen ausschließlich nach ausdrücklicher menschlicher Freigabe der konkreten Änderung wechseln. BRUCE darf Vorschläge erstellen und prüfen, aber nicht selbst freigeben oder aktivieren. Dies umfasst Gewichte, Filter, Deaktivierung und Rollback. Ein allgemeines „weiter“ und das Quellenlabel `manual` ersetzen keine Freigabe. [Verbindliche Entscheidung und offene technische Durchsetzung](../adr/013-human-rule-authority.md).

## Status

**Architekturvertrag.** Eine erste begrenzte Runtime existiert; ihr konkreter Umfang und ihre Aussagegrenzen stehen im [Core-V1-Benutzungsstand](../implementation/06-core-v1-usage.md). Die vollständige Zielarchitektur ist damit nicht implementiert.
