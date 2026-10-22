# Signed-Affine-Relation V1 – Design

**Status: VORSCHLAG — zur Prüfung vor Implementierungsplanung.**

## 1. Ziel und Grenzen

Signed-Affine-Relation V1 ist eine kleine, deterministische C++20-Referenzbrücke
zwischen dem vorhandenen Signed-Affine-DAG V1 und Toy-Relation V1. Sie leitet
für zwei tatsächlich ausgeführte, kollidierende Affine-Pfade ihre vollständigen
Koeffizienten ab, bildet diese kontrolliert auf Toy-Provenienzen ab und lässt die
bestehende Toy-Relation-API die Relation unabhängig prüfen.

Das Paket beweist eine Belegkette:

```text
validierter Signed-Affine-DAG
→ ausgeführter Pfadtrace
→ abgeleitete Form m × s + c
→ Toy-Provenienz (a=c mod n, b=m mod n)
→ Toy-Kollision
→ unabhängig verifizierte Toy-Relation
```

Es ist kein Suchverfahren und keine Verallgemeinerung der bestehenden
Komponenten. Es hat keine Benutzerparameter außer dem festen CLI-Demo,
keine Dateien, Persistenz, Netz- oder GPU-Ausführung, Plugins, Scheduler,
Phase-DAG-V2/V3- oder Cockpit-Integration. Es ist keine reale Kryptographie,
keine Kurvenarithmetik und kein DLP- oder Secret-Recovery-Verfahren. Im
festen Demo ist der Kandidat nur der bereits öffentliche Quellwert des
Beispiels.

Signed-Affine-DAG V1 und Toy-Relation V1 einschließlich ihrer öffentlichen
Verträge, CLI-Befehle und bestehenden Ausgaben bleiben unverändert.

## 2. Mathematische Semantik

Für genau eine Signed-Affine-Quelle `s` hat jeder ausgeführte Pfad eine
rekonstruierte Form

```text
value = m × s + c
```

mit `int64_t`-Koeffizienten. Die Rekonstruktion benutzt ausschließlich die
im ausgeführten V1-Trace belegten Operatoren:

| Trace-Eintrag | neue Form |
|---|---|
| Quelle | `1 × s + 0` |
| `add-two-v1` | `m × s + (c + 2)` |
| `double-v1` | `(2m) × s + 2c` |
| `subtract-one-v1` | `m × s + (c - 1)` |

Jeder Koeffizientenschritt und die Rückrechnung `m × s + c` verwenden
geprüfte `int64_t`-Arithmetik. Ein Überlauf schlägt vor jedem Rückgabewert
fehl.

Zu einem angeforderten Modul `n` baut die Bridge die Toy-Gruppe

```text
G = 1
Q = s mod n
```

und bildet einen Pfad `(m,c)` nach

```text
a = c mod n
b = m mod n
P = aG + bQ = c + m × s mod n
```

ab. Ein Gleichheitsmerge der ausgewählten Affine-Pfade ist dadurch eine echte
Toy-Kollision, nicht eine annotierte oder künstlich zugeordnete Kollision.
Die bereits vorhandene Toy-Relation V1 bestimmt und verifiziert daraus die
Relation. Die Semantik ist auf die Modulklasse `Z/nZ` der bestehenden
Toy-Relation begrenzt.

Referenz: Beim V1-Merge mit `s=2` sind `a-add` mit `m=1,c=2` und `z-double`
mit `m=2,c=0` beide `4`. Für `n=101` entstehen `(a,b)=(2,1)` und `(0,2)`,
der gemeinsame Punkt `4` und nach Toy-Verifikation `candidate_k=2`.

## 3. Öffentlicher Vertrag

Neue Dateien:

```text
include/bruce/signed_affine_relation/signed_affine_relation_v1.hpp
src/signed_affine_relation/signed_affine_relation_v1.cpp
```

Der Header verwendet die vorhandenen Typen aus
`bruce/signed_affine_dag/signed_affine_dag_v1.hpp` und
`bruce/toy_relation/toy_relation_v1.hpp` und stellt bereit:

```cpp
struct SignedAffineRelationRequestV1 {
    SignedAffineDagSpecV1 spec;
    std::uint32_t modulus;
    std::string left_node_id;
    std::string right_node_id;
};

struct SignedAffineDerivedPathV1 {
    std::string node_id;
    std::string source_node_id;
    std::int64_t source_value;
    std::int64_t multiplier;
    std::int64_t offset;
    std::int64_t output_value;
    ToyGroupProvenanceV1 toy_provenance;
};

struct SignedAffineRelationResultV1 {
    ToyCyclicGroupV1 group;
    SignedAffineDerivedPathV1 left;
    SignedAffineDerivedPathV1 right;
    ToyCollisionRecordV1 collision;
    ToyRelationRecordV1 relation;
};

SignedAffineRelationResultV1 derive_signed_affine_relation_v1(
    const SignedAffineRelationRequestV1& request);
```

`left` und `right` bewahren die durch den Aufrufer gewählte Knotenauswahl und
damit die Herkunft der Anzeige. `collision` wird ausschließlich durch
`detect_toy_relation_collision_v1` erzeugt und ist daher nach Provenienz-ID
kanonisch geordnet. `relation` wird durch `derive_toy_relation_v1` erzeugt
und durch `verify_toy_relation_v1` geprüft; eine erfolgreiche Rückgabe hat
immer Status `verified`.

Die Bridge ruft `execute_signed_affine_dag_v1` genau einmal auf und
rekonstruiert die Koeffizienten aus dessen vollständigen Pfadrecords. Sie
führt keine zweite DAG-Ausführung und keine eigene Relationableitung aus.
Trace-Einträge werden dabei defensiv gegen ihre Vorgänger, Eingabe-/Ausgabe-
werte, Operator-ID und rückgerechnete Form geprüft.

## 4. Validierung und Fehlerverhalten

Zuerst gelten die unveränderten Validierungs- und Ausführungsfehler des
Signed-Affine-DAG V1. Danach gilt:

| Bedingung | Fehler |
|---|---|
| nicht genau eine Quelle im ausgeführten Trace | `InvalidSignedAffineRelationSourceCount` |
| gleiche, leere oder unbekannte Knotenauswahl | `InvalidSignedAffineRelationSelection` bzw. `UnknownSignedAffineRelationNode` |
| nicht rekonstruierbarer oder widersprüchlicher V1-Trace | `InvalidSignedAffineRelationTrace` |
| Überlauf bei Koeffizientenkomposition oder Rückrechnung | `SignedAffineRelationCoefficientOverflow` |
| verschiedene Ausgabewerte der Auswahl oder keine Toy-Kollision | `NoSignedAffineRelationCollision` |
| ungültiges Modul oder ungültige Toy-Provenienz | bestehender Toy-Relation-Fehler |
| nicht invertierbarer Toy-Koeffizient | `NonInvertibleToyRelationCoefficient` |
| fehlgeschlagene unabhängige Toy-Verifikation | `RejectedToyRelationVerification` |

Die Funktion ist rein; bei jedem Fehler gibt sie kein partielles
`SignedAffineRelationResultV1` zurück. Ein nichtinvertierbarer Fall erzeugt
insbesondere weder Relation noch Kandidat.

## 5. Feste CLI-Referenz

Neue, immer verfügbare Route:

```text
bruce signed-affine-relation-v1 demo merge
```

Sie verwendet dieselbe mathematische V1-Merge-Form wie das vorhandene
deterministische Signed-Affine-Demo: Quelle `2`, `a-add` (`+2`) und
`z-double` (`×2`). Sie ruft die Bridge mit `n=101`, `left_node_id="a-add"`
und `right_node_id="z-double"` auf.

Die stdout-Antwort ist ein deterministisches JSON-Dokument mit dem Vertrag
`"signed-affine-relation-v1-cli-v1"`, Szenario `"merge"`, Quelle, Toy-Gruppe,
derivierten Formen beider ausgewählten Pfade einschließlich ihrer
Toy-Koeffizienten und Punkte, kanonischer Kollision und verifizierter
Relation. `candidate_k` ist `2`. Erfolg hat Exitcode `0`, einen finalen
Zeilenumbruch und leeres stderr. Falsche Argumente bleiben Fehler-JSON auf
stderr mit Exitcode `2`; es entsteht keine Teilausgabe. Es gibt absichtlich
kein CLI-Demo für Fehlerfälle und keine freie Formeleingabe.

## 6. Tests und Belege

Neue Unit-Tests decken mindestens ab:

1. das positive Merge-Beispiel mit exakten Formen, Toy-Provenienzen,
   kanonischer Kollision und verifiziertem Kandidaten `2`;
2. vertauschte Anforderungsknoten: dieselbe kanonische Kollision und
   Relation, aber bewahrte angeforderte Anzeigeherkunft;
3. negative Quelle sowie negative Offsets und ihre modulo-normalisierten
   Toy-Koeffizienten;
4. Mehrquellen-Spezifikation, gleiche/fehlende Knoten, unterschiedliche
   Ausgabewerte und jede Trace-Inkonsistenz, soweit über gültige
   V1-Ausführung erreichbar;
5. Koeffizientenüberlauf bei einer V1-ausführbaren Nullquelle mit vielen
   `double-v1`-Schritten, ohne Ergebnisveröffentlichung;
6. Nichtinvertierbarkeit: drei `add-two-v1`-Schritte gegen zwei
   `double-v1`-Schritte bei Quelle `2` treffen bei `8`; unter `n=15` ist der
   Multiplikatordelta nicht invertierbar und propagiert den bestehenden
   Toy-Fehler ohne Relation;
7. Determinismus bei wiederholter Ausführung.

Ein echter Python-CLI-Workflow prüft das gesamte, bytegenaue positive
JSON, leeres stderr und Exitcode `0` sowie falsche Argumente mit Exitcode
`2`. Bestehende Signed-Affine-, Toy-Relation-, Phase-DAG-V2/V3- und
Dokumentationstests bleiben Regressionen.

README, Blueprint und CLI-Handbuch beschreiben nur die belegte Referenz,
Formel, feste Route und Grenzen. Die geschützten Dateien
`docs/cli/source-index.md`, `docs/cli/examples.json` und
`docs/cli/validation.md` werden ausschließlich durch
`docs/cli/verify_examples.py` in einer vollständigen temporären Kopie
geprüft oder aktualisiert. Wenn der Generator das neue feste Demo nicht
ausführt, muss dies ausdrücklich dokumentiert bleiben und der echte
Workflow-Test ist der Beleg.

## 7. Integration und Nichtziele

`bruce_core` erhält die neue Quelle und einen eigenen Unit-Test. Die CLI
registriert nur die neue Route. Der Abhängigkeitsfluss ist:

```text
signed-affine relation V1 → signed-affine DAG V1
                          → toy relation V1
```

Keine öffentliche V1-Struktur, kein bestehendes CLI-JSON und keine
Phase-DAG-V2/V3-Schnittstelle wird verändert. Die vorhandene allgemeine
Affine-Phase-DAG-Roadmap bleibt **DOKUMENTIERT / KONZEPT**.
