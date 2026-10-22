# Toy-Relation V1 — Spielzeuggruppen-Kollisionschecker: Design

**Status:** VORSCHLAG — schrittweise bestätigt, noch nicht implementiert.

## Ziel und Abgrenzung

Toy-Relation V1 ist eine lokale C++20-Referenzbibliothek für die exakte
Relationsextraktion aus zwei bekannten Provenienzen derselben Position in einer
kleinen additiven zyklischen Restklassengruppe. Sie demonstriert die Algebra
hinter

```text
P = aG + bQ
```

und leitet bei einer Kollision die Relation

```text
(b1 - b2) * k ≡ a2 - a1 (mod n)
```

ab. Ein abgeleiteter Kandidat wird in einer getrennten Prüffunktion nochmals
gegen beide Provenienzen und die Gruppenparameter bewertet.

Die Implementierung ist kein diskreter-Logarithmus-Löser, keine Suche nach
unbekannten Schlüsseln und keine Realwelt-Kryptographie. Sie verwendet keine
elliptische Kurve, kein secp256k1, keine Netzwerk-, GPU-, Persistenz-, Plugin-
oder freie Eingabeschnittstelle. Der Kandidat in den festen Referenzfällen ist
kein Geheimnis und erzeugt keinen Sicherheitsanspruch.

Toy-Relation V1 ist von Phase-DAG V2/V3 sowie Signed-Affine-DAG V1 getrennt.
Es übernimmt keine Scheduler-, Payload- oder Bundle-Verträge.

## Gruppenmodell und Grenzen

Die Gruppe ist die additive Restklassengruppe `Z/nZ`. Ein
`ToyCyclicGroupV1` enthält:

```text
modulus       n, 3 <= n <= 65535
generator     G, 1 <= G < n und gcd(G,n) = 1
public_q      Q, 0 <= Q < n
```

Ein gültiger Gruppenparameter bedeutet `Q = kG` für einen Skalar `k` modulo
`n`. Die Bibliothek setzt dies für die Relation nicht blind voraus: Die
Rückverifikation prüft den Kandidaten gegen `Q` und gegen beide
Provenienzgleichungen. Die kleine Obergrenze erlaubt alle Zwischenprodukte mit
`std::uint64_t`; öffentliche Gruppenwerte und Koeffizienten sind
`std::uint32_t`.

Ein Koeffizientenzustand wird immer modulo `n` normalisiert. Sein kanonischer
Punkt ist:

```text
(a * G + b * Q) mod n
```

## Datenvertrag

```text
ToyCoefficientStateV1
├── a
├── b
└── canonical_point

ToyGroupProvenanceV1
├── provenance_id
└── coefficient_state

ToyCollisionRecordV1
├── canonical_point
├── existing_provenance_id
└── new_provenance_id

ToyRelationRecordV1
├── modulus
├── coefficient_delta       # (b1-b2) mod n
├── value_delta             # (a2-a1) mod n
├── candidate_k
└── verification_status
```

Provenienz-IDs sind nicht leer und müssen verschieden sein. Ein
`ToyCollisionRecordV1` entsteht nur, wenn zwei verschiedene Provenienzen
exakt denselben `canonical_point` tragen. Der Checker speichert keine
versteckten Eingaben, Pfade, Schlüsseldateien oder externen Referenzen.

## Öffentliche Operationen

Die Bibliothek stellt nur diese Operationen bereit:

1. Gruppenparameter und Koeffizientenzustand validieren und kanonischen Punkt
   bilden.
2. Aus zwei Provenienzen gleicher Position einen Collision Record erstellen.
3. Die modulare Gleichung aus dem Collision Record ableiten und, falls die
   Koeffizientendifferenz invertierbar ist, `candidate_k` mit dem erweiterten
   euklidischen Algorithmus berechnen.
4. Einen Relation Record unabhängig prüfen: Der Kandidat muss `Q == kG mod n`
   und beide Provenienzpunkte erfüllen.

Nicht invertierbare Koeffizientendifferenzen liefern keinen Relation Record.
Die Kernoperation signalisiert `NonInvertibleToyRelationCoefficient`; sie ist ein
kontrollierter, erwarteter Nicht-Erfolgsfall, weil die lineare Kongruenz keine
eindeutige Lösung garantiert. Die feste CLI-Demo fängt ausschließlich diesen
erwarteten Kernfehler ab und rendert den bereits belegten Collision Record mit
einem expliziten Nicht-Erfolgsstatus; alle anderen Kernfehler bleiben CLI-Fehler.

## Fehlervertrag

| Code | Ursache |
|---|---|
| `InvalidToyRelationModulus` | `n` außerhalb 3..65535 |
| `InvalidToyRelationGenerator` | Generator außerhalb des Bereichs oder nicht koprim zu `n` |
| `InvalidToyRelationPublicPoint` | `Q` nicht normalisiert |
| `InvalidToyRelationCoefficient` | Koeffizient nicht normalisiert |
| `InvalidToyRelationProvenanceId` | leere oder gleiche Provenienz-ID |
| `NoToyRelationCollision` | kanonische Punkte unterscheiden sich |
| `NonInvertibleToyRelationCoefficient` | `(b1-b2)` ist modulo `n` nicht invertierbar |
| `RejectedToyRelationVerification` | Kandidat erfüllt Gruppen- oder Provenienzgleichung nicht |

Alle Eingaben werden vor Ableitung oder Rückverifikation geprüft. Fehler
veröffentlichen weder einen Teil-RelationRecord noch einen Kandidaten.

## Feste Referenzfälle

### Positiver Kollisionsfall

Die CLI verwendet `n=101`, `G=1`, `Q=17` und die zwei Provenienzen:

```text
left:  (a,b) = (2,3)   -> 53
right: (a,b) = (36,1)  -> 53
```

Die Kollision ergibt `coefficient_delta=2`, `value_delta=34` und
`candidate_k=17`. Die unabhängige Rückverifikation ist `VERIFIED`.

### Nichtinvertierbarer Kollisionsfall

Die CLI verwendet `n=15`, `G=1`, `Q=4`:

```text
left:  (a,b) = (0,5)  -> 5
right: (a,b) = (5,0)  -> 5
```

Die Koeffizientendifferenz `5` ist modulo `15` nicht invertierbar. Ein
Collision Record entsteht, aber der Relation-Checker liefert kontrolliert
`NonInvertibleToyRelationCoefficient` und keinen Kandidaten.

## CLI

Der immer verfügbare Einstieg hat ausschließlich zwei feste Demos:

```text
bruce toy-relation-v1 demo collision
bruce toy-relation-v1 demo noninvertible
```

`collision` gibt einen stabilen JSON-Bericht mit Vertrag
`toy-relation-v1-cli-v1`, Gruppenmetadaten, beiden Provenienzen, Collision
Record, Relation Record und Verifikationsstatus aus. `noninvertible` gibt
Gruppenmetadaten, Provenienzen und Collision Record aus; der kontrollierte
Nicht-Erfolgsstatus steht im Bericht, ohne einen `candidate_k` zu erfinden.

Der Befehl akzeptiert keine Gruppenparameter, Dateien, Suchgrenzen,
Schlüsselmaterialien oder beliebige Formeln. Falsche Argumente führen wie die
bestehende CLI zu Error-only-JSON auf `stderr` und Exitcode 2.

## Referenztests

Tests entstehen vor der Implementierung und beweisen mindestens:

1. Der positive Referenzfall erzeugt exakt die kanonische Position `53`, einen
   Collision Record, `candidate_k=17` und unabhängigen Status `VERIFIED`.
2. Der Nichtinvertierbarkeitsfall erzeugt eine Kollision, aber keinen
   Relation Record und den exakten kontrollierten Fehler.
3. Verschiedene Punkte, gleiche/leere IDs, nicht normalisierte Parameter,
   ungültige Generatoren und ungültige Moduli werden vor Ableitung abgewiesen.
4. Ein absichtlich falscher Kandidat wird durch die unabhängige Verifikation
   zurückgewiesen.
5. Wiederholte Läufe und vertauschte Eingabereihenfolgen ergeben denselben
   kanonischen Bericht.
6. Reale CLI-Tests sichern beide festen Demos, Help, Fehlergrenzen und
   deterministische JSON-Ausgabe.

## Integration und Dokumentation

Die Bibliothek liegt unter `include/bruce/toy_relation/` und
`src/toy_relation/`, gehört zu `bruce_core` und hat keine neue Abhängigkeit.
Eine Header-CLI nach bestehendem Muster wird ohne optionale Mod-Gates in
`bruce` eingebunden. Readme, Blueprint und CLI-Handbuch benennen den kleinen
Referenzzweck und grenzen ihn von der weitergehenden Affine-Phase-DAG-Roadmap
ab. Generierte CLI-Artefakte werden ausschließlich mit
`docs/cli/verify_examples.py` in einer vollständigen temporären Kopie
aktualisiert.

## Akzeptanzkriterien

Die Arbeit ist erst abgeschlossen, wenn Kern und CLI kompilieren, alle
Referenz- und Fehlerverträge bestehen, V2/V3 und Signed-Affine-DAG V1 nicht
regressieren, Dokumentation und CLI übereinstimmen und die Grenzen gegen
Krypto- und Suchbehauptungen ausdrücklich erhalten bleiben.
