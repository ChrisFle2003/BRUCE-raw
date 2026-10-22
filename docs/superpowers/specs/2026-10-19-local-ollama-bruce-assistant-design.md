# Lokaler Ollama-BRUCE-Assistent V1 — Design-Spezifikation

**Status: DOKUMENTIERT / KONZEPT.** Dieses Dokument beschreibt einen
vorgeschlagenen separaten lokalen Begleiter. Es ist kein Nachweis für eine
eingebaute BRUCE-Funktion, einen funktionierenden Ollama-Endpunkt oder die
Verfügbarkeit bestimmter Modelle.

## Ziel und Nutzererlebnis

Der lokale Assistent nimmt eine deutsch- oder englischsprachige Frage an,
findet mit einem lokalen Embedding-Modell die passendsten erlaubten
BRUCE-CLI-Routen und erstellt mit einem lokalen Antwortmodell einen
strukturierten Ausführungsvorschlag. BRUCE selbst bleibt allein maßgeblich für
Berechnung, Dateiformate, Exitcodes und Ergebnis-JSON. Das Antwortmodell darf
dieses Ergebnis nur erklären; es begründet oder ersetzt keine BRUCE-Evidenz.

V1 ist ein eigenständiges Python-Werkzeug `tools/bruce_ai.py`. Es ergänzt den
C++20-Kern, den vorhandenen stdio-MCP-Adapter und den lokalen HTTP-Inspector,
ohne ihre Verträge umzudeuten. Es gibt keinen BRUCE-Systembenutzer, Daemon,
Serverlistener, Cloud-Zugriff, Modell-Download oder automatischen Lauf.

Der Standardablauf ist:

```text
Frage
  -> lokales Embedding
  -> nächstgelegene Einträge eines lokalen Command-Katalogs
  -> lokales Antwortmodell: strukturierter Vorschlag oder fehlende Angaben
  -> lokaler Schema-, Pfad- und Grenzvalidator
  -> Vorschau
  -> ausdrückliche Nutzerbestätigung
  -> direkter BRUCE-Prozess mit argv (niemals Shell)
  -> unverändertes BRUCE-JSON + klar getrennte lokale Erklärung
```

Die Voraussetzungen sind ein lokal laufendes Ollama, ein explizit gebauter
BRUCE-Binary und lokal installierte Modelle. Die Implementierung prüft nur
deren Erreichbarkeit; sie führt niemals `ollama pull` aus.

## Abgrenzung und Architekturentscheidung

Der Assistent bleibt absichtlich außerhalb der `bruce`-Binary:

- Der C++-Kern bleibt frei von Python-, HTTP-, Modell- und Prompt-Logik.
- Der bestehende `tools/bruce_mcp.py` bleibt ein begrenzter MCP-Adapter; er
  wird weder als Router noch als Modelltransport umgedeutet.
- `bruce mcp` bleibt ein schreibgeschützter Loopback-Inspector und ist kein
  Ollama- oder MCP-Protokollserver.
- Das archivierte Semantic-Index-Experiment unter
  `runs/retired-semantic-index/` bleibt Archivmaterial. Seine alte
  Konzeptbeschreibung ist höchstens ein Entwurfsinput, keine aktive
  Implementierung oder Schnittstelle.

`tools/bruce_ai.py` verwendet nur Python 3.11+-Standardbibliothek:
`argparse`, `dataclasses`, `json`, `hashlib`, `math`, `pathlib`,
`subprocess`, `urllib.request` und `unittest`. Es gibt keine neue
Laufzeitabhängigkeit und keinen CMake-Linkbedarf.

Die unterstützten Operationen sind:

1. `index build`: Katalogeinträge lokal einbetten und einen explizit benannten
   Index erzeugen.
2. `ask`: Frage routen, einen Vorschlag erzeugen, anzeigen und nach einer
   interaktiven Bestätigung optional BRUCE mit validiertem `argv` starten.
3. `explain`: bereits vorhandenes BRUCE-JSON erklären, ohne BRUCE auszuführen.
4. `config check`: Konfiguration, lokale Pfade und lokale Modell-Erreichbarkeit
   prüfen, ohne Modelle zu laden oder Dateien zu ändern.

## Lokaler JSON-Konfigurationsvertrag

Jede Ausführung benötigt `--config CONFIG.json`. Es gibt keine versteckte
globale Konfiguration und keine Umgebungsvariablen als Alternative. Die Datei
ist UTF-8-JSON mit exakt diesem obersten Vertrag:

```json
{
  "schema": "bruce-local-ai-config-v1",
  "active_profile": "default",
  "bruce_program": "/absolute/path/to/bruce",
  "index_path": "/absolute/path/to/bruce-ai-index.json",
  "allowed_roots": ["/absolute/path/to/bruce-data"],
  "require_confirmation": true,
  "profiles": {
    "default": {
      "ollama_url": "http://127.0.0.1:11434",
      "embedding_model": "all-minilm",
      "response_model": "qwen3.5",
      "top_k": 5,
      "connect_timeout_ms": 1000,
      "request_timeout_ms": 30000
    }
  }
}
```

`--profile NAME` wählt ein vorhandenes Profil für genau einen Aufruf und
überschreibt `active_profile` nicht auf Platte. Alle Pfade sind absolut.
`bruce_program`, `index_path` und jedes Element von `allowed_roots` werden
eindeutig, ohne rekursive Verzeichnisanlage und ohne akzeptierte
Symlink-Umgehung geprüft. Konfigurationen mit unbekannten Feldern, doppelten
Profilnamen, leeren Modellnamen, relativen Pfaden, leeren Root-Listen,
nicht-positiven Grenzen oder ungültigen URLs werden abgewiesen.

`ollama_url` darf ausschließlich einen unverschlüsselten Loopback-HTTP-Host
`127.0.0.1` oder `[::1]` mit explizitem Port benennen. Dadurch kann ein
Konfigurationswechsel nicht versehentlich einen Cloud-Provider aktivieren.
Die Modellnamen sind reine lokale Ollama-Tags; `all-minilm` und `qwen3.5`
sind Konfigurationsbeispiele, keine zugesicherte Installation.

## Katalog, Embeddings und Routing

Ein versionierter, repository-eigener `bruce-ai-command-catalog-v1` beschreibt
jede routbare Route mit stabiler ID, natürlicher Beschreibung, Syntax,
Argument-Schema, Seiteneffektklasse und Quellreferenz. Der Katalog ist nicht
aus freiem Markdown erraten. Jede Route wird gegen aktuelle Parserquellen und
reale CLI-Tests gepflegt.

V1 beschränkt das Routing bewusst auf folgende Klassen:

- exakte Rechen- und Adressrouten (`calculate`, `coordinate`);
- mathematische, Bundle- und Forschungsrouten mit expliziten Eingaben
  (`grid`, `grid-batch`, `grid-resume`, `compare`, `research`,
  `inspect-run`, `replay`, `assess-run`, `gate-run`);
- fest parametrierte Referenzdemos;
- schreibgeschützte Mod-Informationen (`mods list`, `mods inspect`,
  `mods verify`, `mods trust status`).

Installation, Aktivierung, Update, Rollback, Trust-Transition,
Release-Signierung, Regelaktivierung und jede andere dauerhafte Änderung sind
in V1 nicht durch das Modell routbar. Sie bleiben direkte, explizite
BRUCE-Aufgaben des Nutzers.

`index build` bettet ausschließlich Katalogtext und die zugehörigen
quellengestützten Kurzbeschreibungen ein. Es schreibt einen atomar
veröffentlichten JSON-Index mit Katalogdigest, Konfigurationsprofilname,
Embedding-Modelltag, Vektordimension, normalisierten endlichen Vektoren und
Indexformatversion. Ein Query mit abweichender Modellkennung, Dimension oder
Katalogdigest wird nicht verwendet und verlangt einen expliziten Neubau.

Die Vektorsuche normalisiert den Query-Vektor, berechnet lokal Cosine-Scores,
bricht Gleichstände stabil über die Katalog-ID und liefert die besten
`top_k`-Kandidaten. Sie ist eine Empfehlung, niemals Syntax- oder
Autorisierungsnachweis.

## Modellprotokolle und strukturierte Verträge

Die Implementierung verwendet nur die dokumentierten lokalen Ollama-
Embedding- und Chat-Endpunkte; konkrete Transportfelder werden bei der
Implementierung gegen die aktuelle offizielle Ollama-Dokumentation
testbar festgelegt. Beide Clients haben feste Größen- und Zeitgrenzen,
verweigern Weiterleitungen und geben keine Zugangsdaten aus.

Das Antwortmodell erhält nur die Frage, die maximal `top_k` selektierten
Katalogeinträge und den festen JSON-Schema-Prompt. Es erhält weder beliebige
Repositorydateien noch Verzeichnislisten, Secrets, Umgebungsvariablen oder
freie Shell-Beispiele.

Der Vorschlag muss exakt einem dieser Formen entsprechen:

```json
{
  "schema": "bruce-local-ai-proposal-v1",
  "kind": "proposal",
  "command_id": "calculate",
  "arguments": {"op": "modpow", "left": "7", "right": "128", "modulus": "13"},
  "missing_inputs": [],
  "reason": "..."
}
```

oder

```json
{
  "schema": "bruce-local-ai-proposal-v1",
  "kind": "needs_input",
  "command_id": "research",
  "arguments": {},
  "missing_inputs": ["train_csv"],
  "reason": "..."
}
```

Der lokale Validator ignoriert jede zusätzliche Modellausgabe. Er akzeptiert
nur bekannte Katalog-IDs, die erlaubten benannten Felder und Werte im
zugehörigen Schema. Er baut das finale `argv` ausschließlich selbst in
kanonischer Reihenfolge. Kein Modellfeld kann den Executable-Pfad, einen
Argumentnamen, einen Shell-Operator, eine Umgebungsvariable, ein Arbeits-
verzeichnis oder eine zusätzliche Unterroute setzen.

Dateiwerte werden kanonisch aufgelöst und müssen innerhalb eines konfigurierten
`allowed_roots` liegen. Nicht vorhandene Eingaben führen zu `needs_input`;
sie werden nie erfunden. Ausgabepfade sind in V1 nicht modellsteuerbar.

Nach bestätigter Ausführung erhält das Antwortmodell das unveränderte,
größenbegrenzte BRUCE-JSON und eine feste Erklärungsvorlage. Seine Antwort
hat `bruce-local-ai-answer-v1` mit `summary`, `status`, `caveats` und
`source_result_digest`. Die CLI gibt immer zusätzlich das ursprüngliche
BRUCE-Ergebnis und dessen Digest aus. Modellprosa ist klar als lokale
Erklärung markiert und darf keine neue Evidenz, keinen mathematischen Beweis
oder eine erfolgreiche Ausführung behaupten, wenn der BRUCE-Status dies nicht
trägt.

## Ausführung, Bestätigung und Datenschutz

`ask` erzeugt zunächst ein JSON-Vorschaudokument mit Routing-Scores,
Katalog-ID, kanonischem `argv`, fehlenden Eingaben und erklärter
Seiteneffektklasse. Die Standardkonfiguration `require_confirmation: true`
fordert danach eine explizite einzelne Bestätigung `run`. Jede andere Eingabe
bricht ohne BRUCE-Prozess ab.

Ein zukünftiges `require_confirmation: false` ist in V1 absichtlich nicht
implementiert und wird als ungültige Konfiguration abgewiesen. Damit kann ein
Modell niemals allein durch eine JSON-Änderung BRUCE-Aufrufe starten.

Der BRUCE-Prozess läuft ohne Shell, mit begrenzter Laufzeit, auf einem festen
Argumentvektor und mit erfassten, begrenzten stdout/stderr-Bytes. Er erhält
keine Modellantwort als Argument. Sein Exitcode und sein Roh-JSON bleiben
unverändert sichtbar. Fehler des Assistenten haben eigene JSON-Fehlerobjekte
und Exitcode 2; sie werden niemals als BRUCE-Fehler ausgegeben.

Es gibt kein automatisches Prompt-, Ergebnis- oder Transcript-Logging.
Ein optionaler, exklusiver `--transcript-out` kann erst nach erfolgreicher
Ausführung ein klar versioniertes lokales JSON schreiben und enthält Frage,
Konfigurationsprofil, Katalog-/Indexdigests, Vorschau, BRUCE-Ergebnis und
Antwortmodelltext. Es ist keine Signatur, kein Auditbeweis und keine
Freigabeentscheidung.

## Fehlergrenzen

Alle Erfolge und Fehler sind JSON. Kontrollierte Fehler enthalten genau einen
festen `error`-Code sowie sichere, kurze Zusatzfelder. Beispiele sind:

- `InvalidAssistantConfig`, `UnknownAssistantProfile`, `UnsafeAssistantPath`;
- `OllamaUnavailable`, `OllamaTimeout`, `OllamaModelUnavailable`,
  `OllamaInvalidResponse`;
- `AssistantIndexMissing`, `AssistantIndexStale`, `AssistantIndexInvalid`;
- `AssistantProposalInvalid`, `AssistantInputRequired`,
  `AssistantConfirmationRejected`, `AssistantExecutionTimedOut`.

Rohe HTTP-/Python-Exceptiontexte, Promptteile, Modellantworten, absolute
Pfade außerhalb ausdrücklich bestätigter Nutzereingaben, Umgebungswerte und
potenziell sensible BRUCE-Ausgaben dürfen nicht in Fehlerobjekte gelangen.

## Tests und Abnahme

Unit- und Integrationstests verwenden einen lokalen HTTP-Mock und ein kleines
Test-BRUCE-Programm; sie benötigen kein installiertes Ollama und kein Modell.
Pflichtfälle umfassen:

1. strikte Konfigurations- und Profilwahl einschließlich Modellwechsel;
2. Verweigerung nicht-lokaler URLs, relativer/symlink-umgehender Pfade,
   unbekannter Felder und ungültiger Grenzen;
3. Indexaufbau, deterministische Cosine-Rangfolge, Gleichstände, Modell- und
   Dimensionswechsel sowie atomare Nichtüberschreibung;
4. fehlerhafte, zusätzliche, zu große oder unvollständige Modell-JSONs;
5. Modellvorschläge für unbekannte Kommandos, freie Argumente, Shellzeichen,
   nicht erlaubte Dateipfade und nicht routbare Mod-/Trust-/Release-Aktionen;
6. Vorschau ohne Prozessstart, Ablehnung ohne Prozessstart und bestätigter
   direkter `argv`-Start ohne Shell;
7. BRUCE-Exit 0/2, ungültiges BRUCE-JSON, große Ausgaben, Timeout und
   Antwortmodell-Ausfall nach erfolgreichem BRUCE-Lauf;
8. Erklärung mit unverändertem Ergebnisdigest, klarer Modell-Markierung und
   ohne neue Ergebnisbehauptung;
9. optionale echte Ollama-Probe nur bei expliziter Aktivierung und nur gegen
   Loopback, ohne Pull-Aktion.

Die Abnahme umfasst die Python-Tests, die bestehenden CTest-Läufe für die
referenzierten BRUCE-Routen, `git diff --check`, lokale Markdown-Links und
eine doppelte Indexerzeugung mit byteidentischem Ergebnis bei identischem
Mock-Modell.

## Offene Entscheidungen

- Die konkrete lokale Modelltag-Wahl ist eine Nutzerkonfiguration; BRUCE
  liefert weder `all-minilm` noch `qwen3.5` aus.
- V1 unterstützt keine modellgesteuerten Schreiboperationen. Eine spätere
  Erweiterung benötigt einen eigenen Vertrag und eine neue Sicherheitsprüfung.
- V1 erstellt keine persistenten Chat-Sitzungen, keine Langzeiterinnerung
  und keine automatische Experience-, Regel- oder Trust-Änderung.
- Die Antwortqualität lokaler Modelle ist keine mathematische,
  sicherheitstechnische oder fachliche Garantie.
