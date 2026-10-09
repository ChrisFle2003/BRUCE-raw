# BRUCE Data-Mods

Data-Mods erweitern den nummerierten BRUCE-Tree zur Laufzeit. Sie enthalten
nur deklarative Daten: keine Programme, Shared Libraries, Shell-Befehle,
Netzwerkzugriffe, GPU-Aufträge oder LLM-Hooks.

## Was ein Mod kann

Ein aktiver Mod kann kontrollierte Texte einer Tree-Adresse und einer Vorlage
zuordnen. Die Zuordnung ist deterministisch:

- `MATCHED`: genau eine Vorlage passt.
- `UNKNOWN`: keine Vorlage passt.
- `AMBIGUOUS`: mehrere Vorlagen passen; BRUCE wählt keine davon aus.

Eine Route führt keine Rechnung oder andere Logik aus. Sie wählt nur eine
deklarierte Vorlage aus.

## Bestandteile

Jeder V1-Mod hat drei kanonische JSON-Eingaben:

- `manifest.json`: `mod_id`, `signing_key_id` und `version`.
- `tree.json`: Mount-Adresse, Knoten, Muster und Claim-Scope.
- `golden-cases.json`: erwartete Routen für die Aktivierungsprüfung.

Der lokale Builder ergänzt feste V1-Metadaten, berechnet Digests und signiert
das Paket mit Ed25519. Das Ergebnis ist eine `.brucemod`-Datei plus ein
öffentlicher Truststore.

## Sicherheitsmodell

- Der private Seed liegt ausschließlich in der ignorierten Projektdatei `.env`.
- `.env` muss dem aktuellen Benutzer gehören und Berechtigung `0600` haben.
- Ein Truststore enthält nur öffentliche Schlüssel und wird beim Prüfen sowie
  Installieren explizit angegeben.
- Neue Pakete werden zunächst `installed_inactive` gespeichert.
- Erst `activate` prüft Signatur, Mount-Konflikte und Golden Cases gegen den
  zukünftigen Tree.
- Paket, Truststore, Registry und Route-Report werden nie still überschrieben.

## Lebenszyklus

```text
JSON-Eingaben
  → bruce-mod-builder
  → signiertes .brucemod + öffentlicher Truststore
  → mods verify
  → mods install (inaktiv)
  → mods activate
  → route / route replay
  → mods disable
```

Ein Route-Report bindet den Tree-Digest und die aktiven Paketidentitäten. Ein
Replay schlägt fehl, wenn sich Tree oder aktive Paketversion geändert haben.

## Befehle

```sh
bruce mods verify PACKAGE --trust-store TRUST.pub
bruce mods install PACKAGE --mod-root MOD_ROOT --trust-store TRUST.pub
bruce mods list --mod-root MOD_ROOT
bruce mods activate MOD_ID VERSION --mod-root MOD_ROOT
bruce mods disable MOD_ID --mod-root MOD_ROOT
bruce route --text TEXT --mod-root MOD_ROOT [--output REPORT.json]
bruce route replay REPORT.json --mod-root MOD_ROOT
```

Die ausführliche erste Übung steht in [TUTORIAL.md](TUTORIAL.md). Ein
vollständiges, kontrolliertes Beispiel findest du unter
[math-rational-power](math-rational-power/README.md).

## Vollständige V1-Feldreferenz

### `manifest.json`

Der Autor schreibt nur diese drei Felder. Der Builder ergänzt die festen
V1-Versionen, `owner_scope: "user"`, die Fähigkeiten `dataset.read` und
`tree.extend` sowie den Payload-Digest.

| Feld | Typ | Regeln |
| --- | --- | --- |
| `mod_id` | String | 1–128 ASCII-Zeichen aus Buchstaben, Ziffern, `.`, `-`, `_`; nicht `core`, nicht mit `.` beginnen |
| `signing_key_id` | String | Öffentliche Schlüssel-ID, die später im Truststore steht |
| `version` | String | Eindeutige Paketversion; dieselben Zeichenregeln wie `mod_id` |

Beispiel:

```json
{"mod_id":"user.physics-vectors","signing_key_id":"christoph-local-v1","version":"1.0.0"}
```

### `tree.json`

```json
{"mount":[1,1],"nodes":[{"aliases":["hoch"],"claim_scope":"candidate_only","id":[1,1,5],"key":"power","route_pattern":["<rational>","hoch","<integer>"],"template_id":"math.rational.power.v1"}]}
```

| Feld | Typ | Bedeutung |
| --- | --- | --- |
| `mount` | Adress-Array | Existierender Elternbereich, unter dem der Mod neue Kinder anlegt |
| `nodes` | Array | Ein oder mehrere neue Tree-Knoten |
| `id` | Adress-Array | Neue, eindeutige Adresse mit 1–32 positiven Zahlen; `[0]` ist ausschließlich der logische Core-Sentinel |
| `key` | String | Eindeutiger technischer Kurzname unter demselben Elternknoten |
| `template_id` | String | Öffentliche Kennung der ausgewählten Vorlage; bei Strukturknoten leer |
| `route_pattern` | String-Array | Vollständige Tokenfolge für einen Treffer; bei Strukturknoten leer |
| `aliases` | String-Array | Lesbare Metadaten; sie verändern das Matching nicht |
| `claim_scope` | String | Aussagegrenze des Knotens, keine Ausführungsberechtigung |

Ein Mod darf nur neue Nachkommen seines `mount` ergänzen. Er darf weder Core
noch einen bereits aktiven fremden Knoten ersetzen. Konflikte beim Aktivieren
lassen den aktiven Tree unverändert.

### Muster und Platzhalter

Muster sind absichtlich eng. Sie werden nach ASCII-Kleinschreibung und
normalisierten Leerzeichen vollständig verglichen. Es gibt keine Synonyme,
keine Wortumstellung, keine Rechtschreibkorrektur und keine Wahrscheinlichkeit.

| Atom | Erlaubt |
| --- | --- |
| Literal | ein exakt geschriebenes Token, etwa `hoch` |
| `<integer>` | `-` optional, danach Ziffern |
| `<rational>` | Integer oder Bruch `Ziffern/Ziffern`, Nenner ungleich null |

`3/4 hoch 2` trifft das Beispielmuster. `3/4 potenz 2` ist `UNKNOWN`.
Passen mehrere Muster, lautet das Ergebnis `AMBIGUOUS`; es wird keine Vorlage
ausgewählt.

### Claim scopes

Die erlaubten Werte sind `unknown`, `candidate_only`,
`verified_within_supported_scope`, `verified_exact_result`,
`explanation_only`, `research_only`, `requires_review` und `denied`.
Sie dokumentieren, wie eine Vorlage interpretiert werden darf. Sie verleihen
keine zusätzliche Fähigkeit; V1-Daten-Mods bleiben immer deklarativ.

### `golden-cases.json`

Golden Cases sind beim Aktivieren verpflichtend. Sie beweisen, dass der
zukünftige aktive Tree die erwarteten Ergebnisse liefert.

```json
[{"input":"3/4 hoch 2","status":"MATCHED","template_id":"math.rational.power.v1"},{"input":"3/4 potenz 2","status":"UNKNOWN","template_id":""}]
```

| Feld | Regeln |
| --- | --- |
| `input` | Kontrollierter Eingabetext, maximal 4096 Bytes |
| `status` | `MATCHED`, `UNKNOWN` oder `AMBIGUOUS` |
| `template_id` | Bei `MATCHED` nicht leer; bei `UNKNOWN` und `AMBIGUOUS` leer |

Ein bewusst mehrdeutiger Testfall sieht so aus:

```json
[{"input":"3/4 hoch 2","status":"AMBIGUOUS","template_id":""}]
```

## Kanonisches JSON

Alle drei Eingabedateien müssen UTF-8, kompakt und kanonisch sein:

- Objektfelder alphabetisch sortiert
- keine Leerzeichen außerhalb von Strings
- keine doppelten oder unbekannten Felder
- keine finale Leerzeile
- keine Kommentare, Pretty-Prints oder alternativen Zahlenschreibweisen

Der Builder und der Runtime-Prüfer lehnen nichtkanonisches JSON ab. Dadurch
haben identische Daten dieselben signierten Bytes.

## Paket, Signatur und Truststore

Der Builder erzeugt eine `.brucemod`-Datei mit exakt vier Mitgliedern:
`manifest.json`, `tree.json`, `tests/golden-cases.json` und
`signature.ed25519`. Das Paket darf höchstens 1 MiB groß sein.

Die Ed25519-Signatur deckt alle Datenmitglieder ab. Der Truststore beginnt mit
`BRUCE_RUNTIME_MOD_TRUSTSTORE_V1`; jede weitere Zeile enthält
`key-id:64-hex-zeichen-öffentlicher-schlüssel`. Private Schlüssel gehören nie
in den Truststore oder ein Paket.

## Versionieren und aktualisieren

- Ändere Daten oder Golden Cases nur mit einer neuen `version`.
- Eine neue Paketversion kann installiert werden, ohne die alte sofort zu
  aktivieren.
- Aktivierung und Deaktivierung sind separate, explizite Schritte.
- Alte Route-Reports werden nach Tree- oder Paketänderungen absichtlich nicht
  neu interpretiert, sondern mit `RouteRegistryDigestMismatch` abgelehnt.

## Fehlerdiagnose

| Situation | Erwartetes Verhalten |
| --- | --- |
| unbekannter Signaturschlüssel | Prüfung/Installation wird abgelehnt |
| veränderte Paketbytes | Digest- oder Signaturprüfung schlägt fehl |
| ungültiger Mount oder Adresskonflikt | Aktivierung schlägt fehl; aktiver Tree bleibt unverändert |
| fehlgeschlagener Golden Case | Aktivierung schlägt fehl |
| bereits vorhandene Ausgabe | Builder überschreibt sie nicht |
| geänderter Tree beim Replay | `RouteRegistryDigestMismatch` |
