# Auswählbare Ausdrucksoperatoren – Core 0.20

Freigegebene Fortsetzung der begrenzten Ausdruckssuche: Eine Aufgabe darf ihre erlaubten binären Rechenarten explizit festlegen. Beide vorhandenen Planer untersuchen dieselbe so deklarierte Grammatik. Es handelt sich um eine Einschränkung des Suchauftrags, nicht um einen aus Samples abgeleiteten Ausschluss mathematischer Möglichkeiten.

## Schnittstelle und Grammatik

`research --strategy expression --operators '+*'` verwendet nur Addition und Multiplikation. Zulässig sind die 15 nichtleeren Teilmengen von `+-*/`, jedes Zeichen höchstens einmal. Reihenfolge der Eingabe ist beliebig; die kanonische Reihenfolge bleibt `+-*/`. Leere Auswahl, unbekannte Zeichen, Leerzeichen und Duplikate werden abgewiesen. CLI und direkte API normalisieren die Auswahl vor der Suche; auch der direkt nutzbare Grammatikkonstruktor validiert sie.

Standard bleibt `+-*/`. Eine explizite vollständige Auswahl ist einschließlich beliebiger Zeichenreihenfolge identisch zum bisherigen Standard. Die CLI weist `--operators` außerhalb der eigenständigen Ausdruckssuche zurück. In der direkten API ist ein abweichender Wert ebenfalls nur für diese Strategie erlaubt. Das Fünferportfolio verwendet weiterhin die volle Grammatik.

Der Konstruktor von `ExpressionGrammar` erhält die Operatorauswahl als zusätzliches optionales Argument. Zählung und rangbasierte Erzeugung durchlaufen nur deklarierte Operatoren; der Index enthält nur deren Blöcke. Terminale, Konstanten, Größenreihenfolge, Kommutativitätsregel und rekursive Baumaufteilung bleiben unverändert. Die Folge der erzeugten Bäume entspricht der bisherigen vollständigen Folge, gefiltert auf ausschließlich erlaubte Operatoren. Die kompakten Kandidaten-IDs werden innerhalb der gewählten Grammatik neu gezählt und sind nicht zwischen verschiedenen Grammatiken austauschbar.

Die maximalen Grenzen bleiben 64 Konstanten, neun Knoten und höchstens 32 Indexintervalle. Der maximale Raum mit allen vier Operatoren bleibt 1327858375545 Ausdrücke; keine Teilmenge darf darüber hinausgehen. Reine Terminalräume mit Knotengrenze 1 bleiben zulässig. Eine Aufgabe erzeugt weiterhin nur aktuell benötigte Bäume.

## Auswertung und Grenzen

Beide Planer `grammar` und `indexed-grammar` sowie beide Gleichheitsmodi `syntax` und `rational` sind unterstützt. Vorwärtsauswertung, separate Ganzzahl-Bruch-Gegenrechnung, Hold-out, Normalisierung und Divisionsbedingungen bleiben erhalten. Die Operatorauswahl ändert keine Auswertungsregeln: Enthält sie Division, bleiben Polstellen wirksam.

Aufbau und Benutzung der kleineren Grammatik werden mit den bestehenden logischen Arbeitsschritten abgerechnet. Das Prüfen der höchstens vier Konfigurationszeichen gehört wie die Validierung der Planerauswahl zur Eingabeprüfung. Operations-, Kandidaten-, Ergebnis-, Bit-, Trace- und Abbruchgrenzen bleiben wirksam. Fehlende Arbeit erzeugt INCOMPLETE. NO_CANDIDATE gilt nur für die vollständig untersuchte gewählte Grammatik, deren Konstanten und Knotengrenze.

## Ergebnis, Replay und Z-Projektion

Abweichende Operatorauswahlen schreiben `BRUCE_EXPRESSION_BUNDLE_V4`, Engine `bruce-expression-v4`, Regel `selected-binary-v1`, Policy `defined-expression-samples-v4`. Der Container speichert explizit Operatoren, Planerversion (`size-ranked-v1` oder `block-index-v1`), Gleichheitsversion, Grenzen und Abbruchposition. JSON und Starttrace nennen die Operatoren, `claim_scope` beschränkt den Befund auf diese Grammatik und erklärt die lokalen IDs.

Replay verlangt eine kanonische echte Teilmenge. Eine vollständige Auswahl gehört weiter zum bisherigen V1-/V2-/V3-Format; nichtkanonische V4-Angaben werden abgewiesen. Replay erzeugt alle Bäume erneut und vergleicht das gesamte Ergebnis einschließlich Kosten, Zählung, Herkunft und Trace. Historische Ausdrucks-/Portfolio-/Experience-Bundles behalten ihren bisherigen Pfad.

Z-Projektion importiert V4, wertet weiterhin den Originalbaum aus und verlangt einen Int64-Projektionswert sowie einen vollständigen Forschungslauf. Die Kennung enthält eine eindeutige Operator-Bitmaske (`+`=1, `-`=2, `*`=4, `/`=8), Planer-/Gleichheitsmodus und Zahlendomäne. So bleiben Kandidaten-IDs verschiedener Operatorräume unterscheidbar, ohne unzulässige Sonderzeichen in die Z-Kennung zu übernehmen. Der Forschungsnachweis bleibt im Auswahl-Replay enthalten.

## Abnahme und offene Schritte

CLI-Anforderung zunächst rot mit `UnknownOption: --operators`; alle 15 Teilmengen gegen nachträglich gefilterte vollständige Grammatik, knotenweiser Vergleich beider Planer, große Rand-/Zufallsränge, kleine vollständige Abbruchreihen, gleiche Ergebnisse innerhalb derselben Auswahl, sichtbarer negativer Befund bei unzureichender Auswahl, erhaltene Polstellen, Manipulationsprüfung und historische Replays. [Abnahmebericht](acceptance/expression-operators-v1-2026-09-22.md).

Neue Operatorarten oder benutzerdefinierte Grammatikregeln werden damit nicht eingeführt. Die Einbindung dieser Auswahl in Portfolio und Erfahrungskontexte ist ein weiteres Paket. M2-/Gesamtprojektabschluss bleibt offen.
