# BRUCE Core 0.2 – dynamische Z-Kompatibilität

BRUCE kann jetzt eigene Regeln zur Laufzeit einlesen, Kandidaten darüber bewerten und die Auswahl vollständig erklären. Regeln sind Daten; eine Änderung benötigt keinen Neubau. Ein verpflichtender Wissens-/Antwortspeicher entsteht dadurch nicht. Nur `--output` archiviert einen Lauf.

## Direkt ausprobieren

```bash
cmake --build build
python3 scripts/demo.py
./build/bruce compatibility --rules examples/compatibility.rules \
  --candidates examples/compatibility-candidates.csv
```

Der Beispielablauf enthält fünf Kandidaten. Die Regeldatei bevorzugt gerade Werte, schließt Werte über 100 und einen ausdrücklich deklarierten Gegenbeispielfall aus und verlangt den angegebenen Koordinatenkontext. Die IDs `3` und `9` bleiben gleichrangig: Ergebnis `AMBIGUOUS`. Eine kleinere ID entscheidet keine Wahrheit.

## Regeln schreiben

```text
BRUCE_RULES_V1
snapshot meine-regeln-v1
rule gerade positive value mod_eq 2 0 5 manual -
rule zu_gross reject value gt 100 0 0 manual -
rule gegenbeispiel negative value eq 6 0 0 manual gegenbeispiel-6
rule kontext context x le 7 0 0 manual -
```

Eine Regelzeile hat zehn Felder:

```text
rule ID ART FELD OPERATOR ARGUMENT REST GEWICHT QUELLE EVIDENZREFERENZ
```

| Feld | V1-Vertrag |
| --- | --- |
| ID / Snapshot-ID | 1–64 ASCII-Zeichen: Buchstaben, Ziffern, `_`, `-`, `.`; `-` allein ist reserviert |
| ART | `positive`, `reject`, `negative` oder `context` |
| FELD | `x`, `y`, `z`, `value`, `layer` |
| OPERATOR | `eq`, `ne`, `lt`, `le`, `gt`, `ge`, `mod_eq` |
| ARGUMENT | int64-Vergleichswert; bei `mod_eq` positiver int64-Modulus |
| REST | bei Vergleichen `0`; bei `mod_eq` in `[0, Modulus)` |
| GEWICHT | nur für positive Regeln, ganzzahlig von -1000000 bis 1000000; sonst `0` |
| QUELLE | derzeit ausschließlich `manual` |
| EVIDENZREFERENZ | für negative Regeln eine benannte ID; sonst `-` |

`positive` addiert bei Treffer sein Gewicht zum Score. `reject` schließt bei Treffer aus. `negative` schließt bei Treffer aus und erhält die benannte Referenz. `context` verlangt, dass die Bedingung erfüllt ist; mehrere Kontextbedingungen gelten gemeinsam. Alle Regeln werden bis zum Budgetende geprüft, auch wenn eine frühere Regel bereits ausschließt, damit die Prüfspur vollständig bleibt.

Der modulare Rest ist auch für negative Werte nichtnegativ: `-3 mod 2 = 1`. Vergleiche mit uint64-Koordinaten und -Layern werden ohne vorzeichenbehafteten Überlauf ausgeführt. Ein leerer Regelsatz lässt alle Kandidaten mit Score 0 gleichrangig.

**Negative Evidenz bleibt hier eine deklarierte Regel mit Referenz.** BRUCE hat noch keinen Experience Store, der die externe Wahrheit oder Unabhängigkeit dieser Referenz bestätigt. Es werden keine unabhängigen Evidenzzahlen aus einer Wiederholung oder einer frei eingetragenen Zahl erzeugt.

Der Snapshot wird beim Einlesen validiert, anschließend nur über lesende Zugriffe verwendet und kanonisch serialisiert. Sein SHA-256-Digest erfasst Inhalt und Reihenfolge aller Regeln. Die frei gewählte Snapshot-ID allein schützt nicht gegen veränderten Inhalt; derselbe Name mit anderen Regeln hat einen anderen Digest.

## Kandidaten eingeben

```text
id,domain,layer,x,y,value
3,math,0,1,0,2
9,math,0,2,0,4
```

IDs, Layer, x und y sind uint64; `value` ist int64. `z` wird aus x und y durch die bestehende Cantor-Paarung berechnet und muss in uint64 passen. Wert und Adresse sind getrennt. Gleiche Werte oder Koordinaten verschiedener Kandidaten werden nicht zu einer Identität zusammengezogen.

Alle Kandidaten eines Laufs müssen dieselbe Domain und denselben Layer besitzen. Kandidaten-IDs sind eindeutig. BRUCE sortiert sie nach numerischer ID; die Reihenfolge der Eingabezeilen beeinflusst das Ergebnis nicht. Die Domain-ID folgt denselben Zeichenregeln wie die Snapshot-ID.

## 64-Bit-Filtergruppen

Jeweils 64 Kandidaten bilden eine Gruppe. Bit 0 gehört zum ersten Eintrag in `candidate_ids`, Bit 63 zum letzten. Weitere Kandidaten erzeugen weitere Gruppen; 130 Kandidaten ergeben Gruppen mit 64, 64 und 2 Einträgen.

| Maske | Bedeutung |
| --- | --- |
| `positive_match_mask` | mindestens eine positive Regel traf zu; noch keine Zulässigkeit |
| `hard_reject_mask` | mindestens eine harte Ausschlussregel traf zu |
| `negative_evidence_mask` | mindestens eine deklarierte negative Regel traf zu |
| `context_role_mask` | alle Regeln ausgewertet und alle Kontextbedingungen erfüllt |
| `complete_mask` | Kandidat vollständig geprüft |
| `eligible_mask` | tatsächlich zum Ranking zugelassene Kandidaten |

Die Zulässigkeit wird bitweise berechnet:

```text
eligible = complete & context & ~hard_reject & ~negative_evidence
```

Unbenutzte Bits in der letzten Gruppe bleiben durch `complete` geschlossen. Die Reihenfolge nach stabiler ID wird mit den Kandidaten und Regeln exportiert. Ein positiver Score kann keinen harten Ausschluss überstimmen.

## Vorhandene Forschungsergebnisse verwenden

```bash
./build/bruce research --train examples/affine-training.csv \
  --holdout examples/affine-holdout.csv --output forschung.bruce
./build/bruce compatibility --rules examples/research-selection.rules \
  --from-run forschung.bruce --at 10 --output auswahl.bruce
./build/bruce replay auswahl.bruce
```

BRUCE replayt zuerst den Ursprungslauf. Nur vollständige Forschungsruns sind zulässig. Nicht widerlegte Hypothesen werden an dem mit `--at` angegebenen x ausgewertet. Ihre Kandidatenkoordinaten sind `(a-min,b-min)`, ihr Wert ist `a*x+b`, ihre ID bleibt die Forschungs-ID. Der Kontext heißt `affine-v1`, Layer 0. Ein Werteüberlauf führt zu einem Fehler, nicht zum stillen Weglassen des Kandidaten.

Diese Projektion wird tatsächlich von der neuen Engine konsumiert. Im Export bleibt das gesamte Ursprungspaket erhalten; Replay leitet die Projektion erneut ab. Ersetzte Kandidaten oder falsche Projektionswerte werden erkannt. Das Ursprungspaket darf selbst nur ein Forschungsexport sein; beliebig verschachtelte Kompatibilitätspakete werden nicht akzeptiert.

`origin_status` zeigt den mathematischen Status des ursprünglichen Laufs. Ein `SELECTED` aufgrund geladener Regeln macht aus ursprünglichem `AMBIGUOUS` keinen mathematisch eindeutigen Beweis.

## Ergebnisse, Budgets und Replay

| Status | Bedeutung |
| --- | --- |
| `SELECTED` | genau ein zulässiger Kandidat besitzt den besten Score |
| `AMBIGUOUS` | mehrere zulässige Kandidaten besitzen denselben besten Score |
| `NO_COMPATIBLE_CANDIDATE` | die Regeln schließen alle Kandidaten aus |
| `INCOMPLETE` | Budget oder Abbruch verhindert eine endgültige Auswahl; `selected_ids` ist leer |

`general_proof`, `learn_gate` und `publish_gate` bleiben `false`. Die JSON-Ausgabe enthält die verwendeten Regeln samt Bedingungen und Referenzen, Kandidaten mit exakten Adressen/Werten, Scores, Masken und eine Regelspur. Die Regelspur nennt pro ausgeführter Prüfung Kandidat, Regel, Treffer und Wirkung. Große Zahlen und Masken werden als Strings ausgegeben, um Float-Rundung beim Anzeigen zu vermeiden.

Maximal 128 Regeln und 4096 Eingabekandidaten; Regel- und Kandidatendatei jeweils maximal 1 MiB. Der neue Befehl verwendet eigene Budgets:

| Option | Standard und Höchstwert |
| --- | --- |
| `--max-candidates` | 4096 |
| `--max-checks` | 1048576 |
| `--max-trace` | 65536 |

Der Mindestwert ist jeweils 1. `--max-checks` zählt Regelprüfungen, `--max-trace` Regelereignisse. Reicht ein Budget genau für den gesamten Lauf, ist dieser vollständig. Ein unvollständiger Lauf darf bereits erklärte Teilbefunde zeigen, aber keinen endgültigen Gewinner. `Ctrl+C` wird zwischen Kandidaten umgesetzt und die logische Abbruchposition exportiert. Beim Forschungsimport findet zunächst der begrenzte Ursprungs-Replay statt; dessen Arbeit ist nicht im Z-Prüfbudget enthalten.

`--output` exportiert Regeln, Kandidaten, Budgets, Abbruchposition, optionalen Ursprung und Ergebnis in `BRUCE_COMPATIBILITY_BUNDLE_V1`. Der Export ist exklusiv und überschreibt keine Datei. Ohne `--output` wird nichts gespeichert. `replay` prüft den Paketdigest, den inneren Snapshotdigest und danach den neu berechneten gesamten Inhalt. Nachträgliche Änderungen der ursprünglichen Regeldatei haben auf den archivierten Lauf keinen Einfluss. Alte `BRUCE_BUNDLE_V1`-Forschungsexporte bleiben unterstützt.

Wie bisher: SHA-256 prüft Integrität, keine externe Authentizität. Ein vollständig neu erzeugter, intern korrekter Lauf mit neu berechnetem Digest ist keine Fälschung, die ein unbeglaubigter Digest erkennen könnte.

## Abgrenzung zur weiteren Roadmap

Die numerische Z-Kompatibilität mit dynamischen Snapshots, getrennten Masken, Auswahl, Trace und Replay ist implementiert. [Core 0.3](10-exact-domains-usage.md) ergänzt BigInt-/Brucharithmetik und exakte affine Forschung. Neue Forschungsexporte können importiert werden, wenn ihre Projektion exakt als int64 darstellbar ist; Z-Prädikate selbst bleiben int64-basiert. Token-/Sprachrollen, externe Evidenzprüfung, automatische Regelpromotion, Experience-Korrekturketten, allgemeine Formelsynthese, NanoCell, Cockpit und Phase-DAG sind noch offen. Der vollständige M1-/Gesamtprojektabschluss wird daraus nicht pauschal abgeleitet.
