# Runtime-Tree-Mod-Host – Architekturvertrag

**Status: VORSCHLAG, vom Nutzer zur Planung freigegeben.**

## Zweck

BRUCE soll zur Laufzeit pro Benutzer geprüfte Erweiterungspakete installieren,
aktivieren und deaktivieren können. Pakete erweitern einen versionierten,
hierarchischen Wissens- und Template-Baum. Sie dürfen Datensätze, kontrollierte
Sprachzuordnungen, Templates, Testvektoren und optional eigene Logik liefern.

Das Ziel ist deterministisches Routing für kontrollierte Eingaben. Ein
nummerierter Baum ist eine stabile Adresse, keine mathematische Formel und
keine probabilistische Semantik. Mathematische Resultate entstehen erst durch
das dem Template zugeordnete, deterministische Core-Modul oder einen geprüften
Worker.

## Nichtziele

- Freie Sprache darf nicht als eindeutig verstanden behauptet werden.
- Ein LLM, Mod oder Worker darf keine Regeln aktivieren, Policy lockern oder
  Claim-Scopes selbst erhöhen.
- Dynamische native Shared Libraries werden nicht in den BRUCE-Host-Prozess
  geladen.
- Netzwerk, Shell, GPU-Scheduling, WebUI und Clusterbetrieb gehören nicht zum
  ersten vertikalen Schnitt.

## Begriffe und Invarianten

### Tree-Adresse

Jeder Knoten besitzt eine kanonische, unveränderliche Adresse aus positiven
Ganzzahlen. Beispiel: `[1, 1, 1]` steht für `math/rational/add`. Die Adresse
ist stabil über Releases; ein gelöschter Knoten wird nicht mit anderer
Bedeutung wiederverwendet.

### Routing

Ein kontrollierter Normalizer erzeugt Tokens und sucht ausschließlich in der
aktiven, versionierten Tree-Registry. Ein eindeutiger Treffer führt zu
`MATCHED`; kein Treffer zu `UNKNOWN`; mehrere zulässige Treffer zu
`AMBIGUOUS`. Der Router rät nie und verwendet keine Wahrscheinlichkeitswerte
als Autorität.

### Claim-Scope

Erlaubte Mindestmenge:

```text
unknown
candidate_only
verified_within_supported_scope
verified_exact_result
explanation_only
research_only
requires_review
denied
```

Nur der Host darf einen Claim-Scope setzen oder nach erfolgreicher
Host-Validierung aufwerten.

### Eigentümerschaft

Eine Registry besitzt einen Scope: `system` oder `user`. Nutzerpakete werden
nur in der Registry des installierenden Nutzers aktiviert. Systemweite
Aktivierung ist eine explizite, getrennte Verwaltungsaktion.

## Host- und Mod-Grenze

```text
BRUCE Host
├── Paketprüfung, Truststore und Aktivierungsjournal
├── Tree-Registry und Mount-Validierung
├── Normalisierung und deterministisches Routing
├── Policy, Capability- und Budgetentscheidung
├── Host-Validator, Trace, Ledger und Replay
└── Start isolierter Worker

Runtime-Mod-Paket
├── Manifest und Signatur
├── Tree-Fragmente, Datensätze und Templates
├── Golden-Testvektoren
└── optionaler Worker-Artefakt
```

Es gibt drei Mod-Klassen:

| Klasse | Inhalt | Host-Ausführung | Autorität |
| --- | --- | --- | --- |
| Data-Mod | Tree, Datensätze, Templates, Tests | nach Schema-/Signaturprüfung | keine |
| Logic-Mod | Parser, Solver, Auswertung | nur isolierter Worker | Kandidat |
| Explain-Mod | Darstellung oder Erklärung | restriktiv/isoliert | Erklärung |

Ein Mod darf nur seinen erklärten Mount-Punkt erweitern. Er darf weder
Core-Knoten noch Knoten anderer Eigentümer überschreiben und keine eigenen
Rechte oder Policies freischalten.

## Tree- und Template-Vertrag

Ein statischer Kern beginnt beispielsweise mit:

```text
root [0]
└── math [1]
    ├── rational [1,1]
    │   ├── add      [1,1,1]
    │   ├── subtract [1,1,2]
    │   ├── multiply [1,1,3]
    │   └── divide   [1,1,4]
    └── research [1,3]
```

Ein Paket kann unter einem freigegebenen Mount eine neue Adresse ergänzen:

```json
{
  "mount": [1, 1],
  "nodes": [{
    "id": [1, 1, 5],
    "key": "power",
    "template": "math.rational.power.v1",
    "aliases": ["hoch", "potenz", "^"],
    "claim_scope": "verified_exact_result"
  }]
}
```

Der Host akzeptiert ein Fragment nur, wenn Parent und Mount existieren, IDs
eindeutig sind, keine geschützten Knoten überschrieben werden, Aliasräume
eindeutig bleiben, referenzierte Templates vorhanden sind und die kanonische
Registry einen neuen Digest erhält.

## Paketvertrag

Ein unveränderliches Paket hat die Endung `.brucemod` und enthält mindestens:

```text
manifest.json
tree.json
templates/
datasets/
tests/golden-cases.json
signatures/package.sig
```

Optional enthält es einen Worker-Artefakt. Das Manifest bindet Mod-ID,
Version, Eigentümer-Scope, Mount, Capabilities, angeforderte Berechtigungen,
Template-IDs, Testvektor- und Paketdigest.

```json
{
  "mod_id": "user.math-rational-extra",
  "version": "1.0.0",
  "owner_scope": "user",
  "mount": [1, 1],
  "capabilities": ["tree.extend", "dataset.read", "worker.execute"],
  "requested_permissions": {
    "filesystem": "package_read_only",
    "network": "none",
    "shell": "none"
  }
}
```

Installationszustände:

```text
DISCOVERED → VERIFIED → TESTED → INSTALLED_INACTIVE → ACTIVE
                                              ├── DISABLED
                                              └── REVOKED
```

Die Installation prüft Format, Größe, Digest, Signatur gegen den passenden
Truststore, Mount-Konflikte und Golden-Testvektoren. Eine erfolgreich
installierte Version ist zunächst inaktiv. Ein Nutzer aktiviert sie explizit;
der Host schreibt dann Registry und Journal atomar. Updates ersetzen keine
aktive Version stillschweigend.

## Worker-Vertrag

Logic-Mods laufen als Child-Prozess außerhalb des Hosts. Der Host übergibt nur
einen kanonischen Auftrag mit Mod-/Templatebindung und harten Ressourcenlimits.

```json
{
  "request_id": "…",
  "mod_id": "user.math-rational-extra",
  "mod_version": "1.0.0",
  "template_id": "math.rational.power.v1",
  "normalized_input": {"base": "3/4", "exponent": "2"},
  "limits": {
    "max_runtime_ms": 50,
    "max_memory_bytes": 16777216,
    "max_output_bytes": 65536
  }
}
```

Der Worker liefert nur ein schema-validierbares `CandidateResult`. Standard:
kein Netzwerk, keine Shell, kein Zugriff auf Regel-/Registry-Dateien, nur
schreibgeschütztes Paket und eigener temporärer Arbeitsbereich. Der Host
validiert Antwort, Größe, Mod-/Templatebindung, Prozessstatus und — sofern
vorhanden — einen deterministischen Validator. Nur der Host erzeugt Ledger- und
Replay-Einträge.

Die erste Umsetzung verwendet Child-Prozesse. Eine spätere WebAssembly-Sandbox
muss denselben Paket- und Worker-Vertrag einhalten.

## Policy und Ressourcen

Der Host entscheidet vor jedem Core- oder Worker-Lauf:

```text
Tree-Match
→ Template
→ Capability- und Policy-Prüfung
→ Budgetentscheidung
→ Ausführung
→ Host-Validierung
→ Claim-Scope
→ Ledger
```

Die erste Matrix:

| Klasse | Zweck |
| --- | --- |
| `ZERO_GPU` | Parsing, Tree-Lookup, Policy, Hashing |
| `CPU_ONLY` | exakte Mathematik, kleine Worker |
| `GPU_MICRO` | Erklärung ohne Autorität |
| `LAB_QUEUE` | Experimente ohne Produktionsautorität |
| `DENY` | unbekannt, zu teuer oder nicht erlaubt |

Templates setzen maximale Ressourcen, zulässige Mod-IDs, Replay-Pflicht,
Reviewpflicht und einen maximalen Claim-Scope. Ein Worker kann keinen höheren
Scope als das Template verlangen.

## Replay und Ledger

Jeder aktive Lauf bindet mindestens:

- Eingabe- und Normalisierungsformatversion;
- Tree- und Registry-Digest;
- Mod-ID, Mod-Version und Paketdigest;
- Template-ID;
- Budgetentscheidung;
- Worker- oder Core-Ergebnis;
- Host-Validatorergebnis und Claim-Scope.

Ein Replay mit abweichender Registry- oder Paketversion schlägt kontrolliert
fehl, statt ein Ergebnis stillschweigend umzudeuten.

## MVP und Abnahme

Vor jeder Erweiterung muss der bestehende Buildblocker um
`management_inspection_cli.hpp` geklärt sein. Erst danach folgen:

1. Tree-Registry mit stabilen IDs und Mounts.
2. Signierte Data-Mod-Pakete je Nutzer: installieren, auflisten, prüfen,
   aktivieren und deaktivieren.
3. Deterministischer Router mit `MATCHED`, `UNKNOWN` und `AMBIGUOUS`.
4. Ein isolierter mathematischer Logic-Mod mit Golden-Testvektoren.
5. Policy-/Budgetcontroller ohne GPU oder WebUI.

Pflichttests:

- gleicher Input und gleiche Registry führen zu gleichem Pfad und Ergebnis;
- unbekannte oder mehrdeutige Eingaben führen nie zu einer geratenen Route;
- ein Mod überschreibt weder Core- noch Fremdnodes;
- veränderte Signatur, Digests oder Testdaten verhindern Aktivierung;
- Ressourcenüberschreitung erzeugt keinen Ledger-Commit;
- Replay mit anderer Paketversion meldet Versionsabweichung;
- Explain-/LLM-Mods können keine Regeln oder Claim-Scopes ändern.

## Abgrenzung zum bestehenden Kern

Der heutige Kern besitzt bereits feste `ApplicationTask`-Varianten, Replay,
Gates, lokale Regelhistorie, Experience-Snapshots sowie einen statischen
Language-/NanoCell-Mod-Katalog. Der Runtime-Tree-Mod-Host erweitert diese
Grenzen, ersetzt sie nicht. Insbesondere bleiben mathematische Behauptungen
an vorhandene Validatoren und deren echte Verträge gebunden.
