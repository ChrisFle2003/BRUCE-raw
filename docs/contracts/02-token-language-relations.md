# Token-, Sprach- und Relationsmodell

## Ziel

BRUCE soll in einer begrenzten Domäne Sprache regelbasiert, lernend und erklaerbar behandeln. Das Ziel ist keine Behauptung menschlichen Bewusstseins.

```text
Rohtext -> Normalisierung -> Token -> Merkmale/Relationen
       -> Rollen und Kontext -> Z-Kompatibilitaet
       -> Auswahl oder Mehrdeutigkeit -> Erklaerung
```

## Komponentenvertrag

| Komponente | Verantwortung | Eingabe | Ausgabe |
| --- | --- | --- | --- |
| `InputNormalizerV1` | deterministische Unicode-/Formatnormalisierung | Rohtext | `NormalizedInputV1` |
| `TokenizerV1` | Segmentierung | normalisierter Text | Tokenfolge |
| `TokenTableV1` | versionierte Identitaeten | Tokenform | `TokenIdV1` |
| `TokenV1` | Tokenmetadaten | ID, Tabelle | Tokenobjekt |
| `LexicalFeatureV1` | Merkmale | Token | Filtergruppen |
| `RelationV1` | pruefbare Beziehung | Tokenpaar | Relation plus Evidenz |
| `RoleAssignmentV1` | Satzrolle | Kontext/Kandidaten | Rollenbelegung |
| `ContextWindowV1` | begrenzter Kontext | Tokenfolge | Kontextreferenz |
| `AmbiguitySetV1` | gleichrangige Deutungen | Kandidaten | Alternativmenge |
| `LanguageExplanationV1` | Erklaerung | Trace | Nutzerausgabe |
| `ExternalFeedbackV1` | unabhaengige Rueckmeldung | Bestaetigung/Gegenbeispiel | Evidenzreferenz |

## Rollen

Mindestens modellierbar:

```text
Subjekt, Objekt, Verb, Kategorie, Eigenschaft,
Relation, Negation, Kontext, Ausschluss
```

## Grenzen

- Unicode wird nicht in eine einzige 64-Bit-Maske gezwungen.
- Token-ID und sichtbarer Text bleiben getrennt.
- Mehrdeutigkeit bleibt sichtbar, falls kein dokumentierter Tie-Break greift.
- Eine Tokenrelation wird nur mit Evidenz zur lernbaren Regel.
- Das Sprachmodell darf keine Relation als wahr setzen.

## Messbarer MVP-Nutzen

BRUCE kann Token versioniert normalisieren, Rollen aus Regeln ableiten, konkurrierende Interpretationen zeigen, Beziehungen mit Gegenbeispielen einschränken und jede Zuordnung bis zu ihren Regeln und Evidenzen erklaeren.

## Tests

- gleiche Eingabe ergibt gleiche Token-IDs bei gleicher Tabellen-Version;
- alte Trace verwendet alte Token-Tabelle;
- Unicode-Normalisierung ist idempotent;
- zwei gleichrangige Rollen ergeben `AmbiguitySetV1`;
- Gegenbeispiel entkraeftet Relation nur ueber neue Version.

## Status

**Blueprint-Vorschlag.**
