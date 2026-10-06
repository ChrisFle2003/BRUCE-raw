# Begrenzte Ausdruckssynthese – Core 0.14

Freigegebene Fortsetzung nach dem Viererportfolio: Eine eigenständige Strategie `expression` setzt kleine Ausdrücke aus einer festen Grammatik zusammen. Sie verwendet den bestehenden Forschungs-, Budget-, Evidenz-, Replay- und Z-Pfad. Das ist eine erste begrenzte Ausdruckssynthese, keine allgemeine Formelentdeckung und keine fünfte Portfoliofamilie in dieser Lieferung.

## Grammatik und Identität

Terminalsymbole: `x` und die gekürzten rationalen Konstanten mit Zähler in `[minimum,maximum]` und positivem Nenner höchstens `max_denominator`. Binäre Operationen: `+`, `-`, `*`, `/`. Ausdrucksgröße zählt sämtliche Blätter und Operatoren. `max_nodes` ist ungerade und liegt zwischen 1 und 9, Standard 7. Zahlenbereich ausschließlich rational, Planer `grammar`.

Das Produkt aus Zählerbreite und Nennergrenze ist auf 64 Konstanten-Rohslots begrenzt. Mehrfachdarstellungen werden vor der Grammatikbildung ausgeschlossen. `x` ist ein zusätzliches Terminal, also höchstens 65. Die vorhandenen Bit- und Eingabegrenzen gelten auch hier.

Die Enumeration erfolgt nach Größe, dann Operator in der Reihenfolge `+,-,*,/`, dann linker Teilbaumgröße und den Teilbaumrängen. Bei `+` und `*` wird das Paar nach Teilbaumgröße und Rang geordnet; gleich große Teilbäume verwenden Kombinationen mit Wiederholung. Subtraktion und Division bleiben geordnet. Assoziativität, Distributivität und Kürzung werden nicht als Gleichheitsregeln angewandt. Verschiedene Schreibweisen derselben Funktion können deshalb mehrere Kandidaten bleiben.

Dynamische Zähltabellen pro Größe und Unranking erzeugen jeweils nur einen Baum. Keine vollständige Baumablage und kein datenabhängiges Verwerfen von Unterausdrücken. IDs sind die kumulierten Größenblockränge und hängen nur von Grammatikgrenzen ab, nicht von Samples. Der größte Raum enthält 1327858375545 Ausdrücke. Der Split bei `2^25` hält die zugehörigen Koordinaten und Cantor-Adressen im bestehenden 64-Bit-Bereich.

## Zwei Ausführungswege und Definitionsbereich

Vorwärtsauswertung: rekursiver Baumdurchlauf mit der vorhandenen rationalen NumberDomain. Gegenrechnung: separater Durchlauf der Postorder-Knoten mit ganzzahligen Zähler-/Nennerpaaren, Kreuzprodukten und Normalisierung. Jede verlangte positive Sampleprüfung muss beide Wege bestehen.

Beide Wege prüfen ihre Zwischenwerte gegen das Bitlimit. Die Ganzzahl-Gegenrechnung kann deshalb bei Kreuzprodukten an eine Grenze stoßen, obwohl ein gekürztes rationales Zwischenergebnis kleiner wäre. Dann bleibt die Suche `INCOMPLETE`, statt die Gegenprüfung zu überspringen. Das ist eine konservative Ressourcengrenze, kein mathematisches Gegenbeispiel.

Jede Division verlangt einen definierten, von null verschiedenen rechten Teilbaum. Alle Divisoren werden als `domain_conditions` ausgegeben; verschachtelte Divisoren behalten auch ihre inneren Bedingungen. `x/x` behält `x != 0`; auch Multiplikation mit null hebt eine ungültige Division nicht auf. Nennernullstellen widersprechen Samples mit erwarteter endlicher Ausgabe. Keine automatisch verkürzte Gesamtbruchdarstellung.

Die Baumstruktur wird geprüft: höchstens neun Knoten, nur bekannte Operatoren, Kinder vor dem Elternknoten, jeder Nichtwurzelknoten genau einmal referenziert. Die Ergebnisdarstellung enthält den Baum, die Formel, die Divisionsbedingungen und den Sample-Status.

## Budgets und Replay

Konstantenerzeugung, Grammatikzählung und Unranking zählen zur Planungsarbeit. Rekursive Knotenbesuche, Rechenoperationen, Ganzzahl-Gegenrechnung und Samplevergleiche zählen zum gemeinsamen Operationsbudget, jeweils vor Ausführung. Der Zähler beschreibt logische Arbeit und keine CPU-Instruktionen. Ein Checkpoint vor der Planung und vor jedem Kandidaten friert Callback-Abbrüche für Replay ein.

Trace-, Ergebnis-, Kandidaten-, Bit- und Operationsgrenzen bleiben wirksam. Nicht abgeschlossene Enumeration ergibt `INCOMPLETE`, auch bei positiven Teilbefunden. Die kanonische Grammatik wird unabhängig von Lern-/Hold-out-Werten erzeugt. `NO_CANDIDATE` gilt ausschließlich für den vollständig untersuchten begrenzten Raum. Kein Samplebefund ist ein allgemeiner Beweis.

Container `BRUCE_EXPRESSION_BUNDLE_V1`, Engine `bruce-expression-v1`, Regel `binary-arithmetic-v1`, Policy `defined-expression-samples-v1`, Planer `size-ranked-v1`. Der Replay regeneriert Grammatik, Bäume, Befund und Trace. Alte Replay-Versionen bleiben unverändert. Die Z-Projektion wertet den gespeicherten Baum tatsächlich aus und verlangt weiterhin einen ganzzahligen Int64-Wert; Pole werden ausdrücklich abgewiesen.

## Abnahmeweg

CLI-Anforderung zuerst rot; vollständige kleine Grammatik gegen unabhängige geordnete Python-Grammatik mit Fraction-Auswertung; 2802 Bäume mit beiden Rechenwegen auf positiven, negativen und rationalen Werten; Abbruch an allen kleinen Referenz-Checkpoints und Operationsgrenzen; Bit-/Trace-/Ergebnisgrenzen; Definitionslücken, Z-Import und Replay ohne Ursprungsdateien; historische Fixture; unabhängiges Review; Debug-/UBSan-Suiten, Beispiele und Demo. Ergebnisse im [Abnahmebericht](acceptance/expression-synthesis-v1-2026-09-22.md).
