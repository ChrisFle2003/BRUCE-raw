# Common Runtime Compatibility V1 — Entwurf

**Status: IMPLEMENTIERT UND BELEGT für Stufe 0 und alle vier Stage-1-Familienadapter.**
Diese Spezifikation beschreibt einen Kompatibilitätsvertrag für die bestehenden
Steuerungsfamilien. Sie behauptet keine neue Runtime-Implementierung.

## Ziel

BRUCE besitzt vier tatsächlich getrennte Kontrollfamilien:

- Sitzung mit `session-checkpoints-v1`;
- Gate-Inspektion mit eigener Prüfpunktsteuerung;
- Transfer mit `transfer-checkpoints-v1`.
- read-only Management-Inspektion mit `management-checkpoints-v1`.

Common Runtime Compatibility V1 soll ihre Grenzen maschinenlesbar und
migrationsfähig beschreiben, ohne sie stillschweigend zu vereinheitlichen.
Ein späterer gemeinsamer Host darf nur über diesen Vertrag eingeführt werden.

## Repository-Evidenz

**IMPLEMENTIERT UND BELEGT:** Die Unterschiede zwischen Budgeteinheit,
Kontrollreihenfolge, Fehlerhülle und Veröffentlichungsgrenze sind im aktuellen
Core 0.102 dokumentiert und ausführbar geprüft.

Belege:

- `docs/contracts/06-control-and-publication.md`;
- `docs/adr/015-control-contract-compatibility.md`;
- `scripts/verify_control_contract_matrix.py`;
- CTest `control_contract_matrix` und `control_contract_matrix_unit`;
- `include/bruce/application/transfer_control.hpp`;
- `include/bruce/application/management_inspection_control.hpp`;
- `src/application/execution_session.cpp` und `src/application/session_budget.cpp`.

## Abgrenzung

Diese V1 führt nicht ein:

- keinen gemeinsamen Scheduler;
- keinen neuen Worker- oder Prozessvertrag;
- keine gemeinsame Runtime-CLI außer den ausdrücklich spezifizierten
  Stage-1-Familienbestätigungen;
- keine Migration bestehender Bundle- oder Berichtformate;
- keine universelle Zeit-, RAM-, Operations- oder Signallatenzgarantie;
- keine automatische Regelaktivierung;
- keine Netzwerk-, GPU-, Plugin- oder externen-Store-Ausführung;
- keine freie Sprache und kein allgemeines Beweisen.

Die bestehenden Aufrufe bleiben byte- und fehlerhüllenkompatibel. Stufe 1
exponiert die read-only Bestätigung `--compatibility-family` auf vier
getrennten Familienpfaden. Jede Bestätigung validiert den Descriptor intern
vor dem jeweiligen I/O und gibt keinen Descriptor aus:

- Gate: `gate-run`;
- Session: sechs generische `execute_cli`-Routen;
- Transfer: sieben Transfer- und Experience-Routen;
- Management: `experience inspect` und `rules-status`.

## Kompatibilitätsobjekt

Ein zukünftiger Bericht darf folgende ausschließlich beschreibende Felder
führen:

```text
contract = "bruce-common-runtime-compatibility-v1"
family = "session" | "gate" | "transfer" | "management"
budget_unit
checkpoint_order
error_envelope
publication_boundary
legacy_route
```

Das Objekt ist eine Beschreibung des tatsächlich gewählten Pfads. Es ist kein
Berechtigungstoken, kein Freigabenachweis und kein Ersatz für die erneute
Prüfung einer Eingabe.

## Unveränderliche Regeln

1. Session-, Gate-, Transfer- und Managementzähler bleiben verschiedene Einheiten.
2. Eine gemeinsame Darstellung darf Fehlerhüllen nicht umschreiben.
3. Die bestehende Kontrollreihenfolge bleibt je Familie erhalten:
   - Session: pfadabhängige Reihenfolge bleibt bestehen;
   - Gate: Signalprüfung vor Verbrauch;
   - Transfer: Signal, Budgetprüfung, dann Verbrauch.
   - Management: Signal- und Budgetprüfung bleiben im read-only
     Inspektionscontroller.
4. `INCOMPLETE` als fachliches Ergebnis bleibt von einem Steuerungsfehler
   getrennt.
5. Lesen, atomare Fachoperation und Veröffentlichung behalten ihre bisherigen
   Grenzen.
6. Ein neuer Host muss alte Aufrufe entweder unverändert weiterleiten oder mit
   einem explizit neuen Vertrag ablehnen; stille Umdeutung ist unzulässig.

## Migrationsstufen

### Stufe 0 — Beschreibung — IMPLEMENTIERT UND BELEGT

Die aktuelle Matrix wird als read-only Kompatibilitätsbericht und kanonischer
Descriptor beschrieben. Keine gemeinsame Runtime und keine CLI-Ausgabe des
Descriptors.

### Stufe 1 — Explizite Auswahl — IMPLEMENTIERT UND BELEGT

Die vier Familienadapter können ihre bereits gewählte Kontrollfamilie
ausdrücklich bestätigen. Ohne Auswahl bleiben die bisherigen Routen aktiv.
Ausgabe und Fehlerhülle bleiben bytegleich; falsche Familien werden vor dem
jeweiligen Datei-, Bundle- oder Storezugriff kontrolliert abgewiesen. Die
konkreten Routen und Nachweise stehen in den vier Stage-1-Spezifikationen.

### Stufe 2 — Adapter

Ein gemeinsamer Host delegiert an die bestehende Familie. Der Adapter darf
keine Budgeteinheiten zusammenrechnen und muss die ursprüngliche Fehlerhülle
und Veröffentlichungskante erhalten.

### Stufe 3 — Neue gemeinsame Runtime

Erst nach einem separaten V2-Vertrag dürfen Kontrollpunkte, Statusmodell,
Abbruchreihenfolge oder Fehlerhüllen vereinheitlicht werden. Das ist nicht
Bestandteil dieser Spezifikation.

## Abnahmekriterien für eine spätere Implementierung

1. Jeder unterstützte Pfad meldet seine Familie und seine Einheit korrekt.
2. Alte CLI-Aufrufe erzeugen bytegleiche Erfolgs- und Fehlerausgaben.
3. Nullbudget, Signalabbruch, unvollständiger Befund und Exportfehler werden
   pro Familie mit den bestehenden Prioritäten nachgewiesen.
4. Ein Adapter kann keine Session-Punkte als Transfer-Punkte verwenden.
5. Kein neuer Bericht wird als menschliche Freigabe interpretiert.
6. Ein Wiederholungs- und Mutationstest weist nach, dass Berichte keine
   Eingabeprüfung ersetzen.
7. Bestehende Matrix-, Session-, Gate-, Transfer- und Managementtests bleiben
   grün.

## Offene Entscheidungen

- Soll ein späterer, separater Descriptor-Ausgabemodus veröffentlicht werden?
- Soll ein späterer Adapter nur lokale Aufrufe oder auch Cockpit-Aufträge
  kapseln?
- Welche neue Fehlerhülle wäre für eine echte V2-Runtime zulässig, ohne die
  Legacy-Verträge zu brechen?

## Grenzen

**DOKUMENTIERT / KONZEPT:** Common Runtime Compatibility V1 ist ein
Migrations- und Belegvertrag. Er beweist weder eine allgemeine Runtime noch
universelle Rechenkontrolle, mathematische Wahrheit, externe Authentizität
oder sichere Ausführung beliebiger Plugins.
