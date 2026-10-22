# Signed-Affine-DAG V1 — Referenzprototyp: Design

**Status:** VORSCHLAG — von der Nutzerin bzw. dem Nutzer schrittweise als Design bestätigt; noch nicht implementiert.

## Ziel und Abgrenzung

Signed-Affine-DAG V1 ist eine neue, lokale C++20-Referenzbibliothek für kleine,
deterministische affine Berechnungsgraphen. Sie soll beweisen, dass BRUCE gleiche
Integerwerte als einen semantischen Zustand zusammenführen kann, ohne die
unterschiedlichen mathematischen Herleitungen zu verlieren. Jeder Ausführungspfad
hat ein ganzzahliges Vorzeichengewicht. Mehrere Pfade zum selben Wert werden exakt
summiert.

V1 ist ein eigenständiges Subsystem. Es ändert weder Phase-DAG V2/V3 noch deren
Spec-, Bundle-, Payload- oder Scheduler-Verträge.

Nicht zu V1 gehören freie Sprache, ein allgemeiner Formelprover, Gruppen- oder
Kryptooperationen, GPU- oder Netzwerkausführung, Persistenz, Plugins, Datenbanken,
allgemeine Scheduling-APIs und ein Cockpit.

## Architektur

Die Bibliothek liegt unter `include/bruce/signed_affine_dag/` und
`src/signed_affine_dag/`, wird Teil von `bruce_core` und hat keine neuen externen
Abhängigkeiten. Eine kleine CLI-Demo wird im bestehenden `bruce`-Programm
registriert. Sie verwendet nur zwei fest eingebaute Referenzszenarien und nimmt
keine freie Graph- oder Formelbeschreibung an.

```text
SignedAffineDagSpecV1
        |
        v
Strikte Validierung des Execution DAG
        |
        v
Deterministische Ausführung in topologischer Knoten-ID-Reihenfolge
        |
        +--> Execution Record und vollständige Provenienz
        |
        v
Semantic-State-Index nach Integerwert
        |
        v
Zusammengeführte Zustände mit exakter Gewichtssumme
        |
        v
SignedAffineDagResultV1 / CLI-Referenzbericht
```

Der **Execution DAG** enthält jede konkrete Anwendung eines Operators. Er muss
azyklisch sein. Der **Semantic State Graph** ist ein abgeleiteter Index: alle
Ausführungsknoten mit gleichem Ergebniswert verweisen auf denselben
`SemanticState`. Daher sind Zusammenführungen dort erlaubt und erwünscht.

## Datenvertrag

V1 verwendet für Start-, Zwischen- und Ergebniswerte `std::int64_t`. Alle affine
Arithmetik und alle Summen von Pfadgewichten prüfen Überlauf; ein Überlauf beendet
den Lauf kontrolliert, ohne einen Teilerfolg zu veröffentlichen.

Ein `AffineOperatorV1` besteht aus einer stabilen ID sowie den Koeffizienten `a`
und `b` für `x -> a*x + b`. Der feste Katalog enthält ausschließlich:

| ID | Formel |
|---|---|
| `add-two-v1` | `x -> x + 2` |
| `double-v1` | `x -> 2*x` |
| `subtract-one-v1` | `x -> x - 1` |

Ein `ExecutionNodeV1` enthält eine eindeutige Knoten-ID, optional genau einen
Vorgängerknoten, eine Operator-ID und ein Pfadgewicht aus `-1` oder `+1`. Eine
Quelle verwendet keinen Operator und liefert einen expliziten Startwert. V1
unterstützt absichtlich keine Mehrfachvorgänger; die Zusammenführung findet über
den semantischen Wertindex statt, nicht über eine allgemeine N-zu-1-Operator-API.

Jeder erfolgreiche Knoten erzeugt einen `PathRecordV1` mit Knoten-ID,
Vorgänger-ID (falls vorhanden), Operator-ID, Eingabe- und Ergebniswert sowie
Pfadgewicht. Ein `SemanticStateV1` enthält seinen kanonischen Wert, alle zu ihm
gehörenden `PathRecordV1` und die exakt summierte Pfadgewichtsbilanz.

Ergebnisse ordnen Semantic States aufsteigend nach Wert. Innerhalb eines Zustands
sind Path Records nach Knoten-ID geordnet. Die Reihenfolge ist Teil des
Referenzverhaltens.

## Validierung und Ausführung

Vor Ausführung gelten folgende Prüfungen:

- mindestens eine Quelle und höchstens ein Vorgänger pro nicht-quellendem Knoten;
- eindeutige, nicht leere Knoten-IDs;
- alle Vorgänger referenzieren einen vorhandenen Knoten;
- nur die drei fest eingebauten Operator-IDs;
- Pfadgewicht ausschließlich `-1` oder `+1`;
- keine Operatorangabe an einer Quelle;
- keine Quelle ohne expliziten `std::int64_t`-Startwert;
- kein Zyklus im Execution DAG.

Die Ausführung verwendet Kahn-Topologiesortierung; gleichzeitig bereite Knoten
werden nach Knoten-ID ausgeführt. Für eine Quelle wird ihr Startwert registriert.
Für jeden anderen Knoten wird der Wert seines Vorgängers mit dem festen Operator
berechnet. Der erzeugte Path Record wird immer dem Semantic State des Ergebnisses
hinzugefügt. Ein bereits bekannter Ergebniswert erzeugt keinen zweiten Semantic
State, sondern nur eine weitere Provenienz.

Die Bilanz eines Semantic States ist die Summe aller dort gespeicherten
Pathgewichte. Eine Bilanz `0` löscht weder den Zustand noch seine Provenienz; sie
belegt die exakte Aufhebung gleichartiger Resultate durch entgegengesetzte Pfade.

## Fehlervertrag

Der öffentliche API- und CLI-Fehlercode benennt mindestens diese kontrollierten
Fälle:

| Code | Ursache |
|---|---|
| `InvalidSignedAffineDagNodeId` | leere oder doppelte Knoten-ID |
| `UnknownSignedAffineDagPredecessor` | fehlender Vorgänger |
| `InvalidSignedAffineDagOperator` | Operator nicht im festen Katalog |
| `InvalidSignedAffineDagPathWeight` | Gewicht nicht `-1` oder `+1` |
| `InvalidSignedAffineDagSource` | unzulässige Quellenfelder |
| `SignedAffineDagCycle` | Zyklus im Execution DAG |
| `SignedAffineDagArithmeticOverflow` | Überlauf in Affintransformation oder Bilanz |

Ungültige Specs werden vollständig vor der ersten Berechnung zurückgewiesen.

## CLI-Demo

Die CLI erhält einen kleinen, expliziten Einstieg:

```text
bruce signed-affine-dag-v1 demo merge
bruce signed-affine-dag-v1 demo cancellation
```

`merge` startet bei `2` und erzeugt mit `add-two-v1` sowie `double-v1` zwei
verschiedene Pfade zum Wert `4`; der Ergebnisbericht enthält genau einen Zustand
für `4` mit zwei Path Records und Bilanz `2`.

`cancellation` erzeugt zwei voneinander verschiedene Pfade mit demselben
Ergebniswert und Gewichten `+1` bzw. `-1`; der Bericht enthält beide Path Records
und Bilanz `0`.

Die CLI gibt nur einen stabilen, menschenlesbaren JSON-Bericht mit Szenario,
semantischen Zuständen, Provenienzen und Bilanzen aus. Sie schreibt keine Dateien
und akzeptiert keine beliebigen Formeln oder Graphdateien.

## Referenztests

Die Tests entstehen vor der Implementierung und belegen mindestens:

1. Der Merge-Fall bildet für Wert `4` genau einen Semantic State mit zwei
   unterschiedlichen, vollständigen Provenienzen.
2. Der Cancellation-Fall erhält beide Provenienzen und berechnet für ihren
   gemeinsamen Zustand genau Bilanz `0`.
3. Wiederholte Läufe liefern bytegleich denselben Ergebnisbericht und dieselbe
   Zustands-/Provenienzreihenfolge.
4. Unbekannte Vorgänger, doppelte IDs, unbekannte Operatoren, ungültige Gewichte
   und Zyklen werden vor Ausführung abgewiesen.
5. Überläufe der affinen Transformation und der Bilanz werden kontrolliert
   abgewiesen.
6. Die CLI-Referenzfälle sind über die vorhandenen Python-Workflowtests
   ausführbar.

## Integrations- und Dokumentationsgrenzen

Die CMake-Ziele und Tests folgen dem vorhandenen `bruce_core`-/CTest-Muster.
Das CLI-Handbuch beschreibt ausschließlich die beiden eingebauten Demos und ihre
Beweisgrenzen. Architektur- und Readme-Texte unterscheiden den neuen
Referenzprototypen klar von Phase-DAG V2/V3 und von der weitergehenden
`affine-phase-dag.md`-Roadmap.

## Akzeptanzkriterien

Die Arbeit ist erst abgeschlossen, wenn die Bibliotheks- und CLI-Verträge
kompilieren, die genannten Referenztests bestehen, vorhandene V2/V3-Tests nicht
regressieren und die neue Dokumentation mit dem tatsächlichen CLI-Verhalten
übereinstimmt.
