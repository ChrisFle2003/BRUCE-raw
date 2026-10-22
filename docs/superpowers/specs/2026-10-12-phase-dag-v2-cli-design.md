# Phase-DAG V2 CLI: ausführbare Mehrknoten-Spezifikationen

**Status:** ENTWURF, VOM NUTZER FREIGEGEBEN\
**Datum:** 2026-10-12\
**Geltungsbereich:** neue lokale Produkt-CLI für Phase-DAG V2 mit dem eingebauten NanoCell-Adapter

## Ziel

Eine Person soll eine DAG mit mehreren NanoCell-Knoten und ihren Abhängigkeiten in einer lesbaren Spec-Datei beschreiben, sie mit `bruce` ausführen und ein portables `BRUCE_PHASE_DAG_RUN_V2`-Bundle erzeugen können. Das Ergebnis soll anschließend über die CLI geprüft und wiederholt werden können; das bestehende Cockpit soll das erzeugte Bundle unverändert inspizieren können.

Die Änderung nutzt den vorhandenen Phase-DAG-V2-Kern und erweitert ihn nicht um Dateisystem- oder Textformat-Abhängigkeiten.

## Nicht-Ziele

Nicht Teil dieses Vorhabens sind:

- freie natürliche Sprache als Eingabe,
- ein allgemeiner Beweiser,
- GPU-Ausführung,
- parallele oder verteilte DAG-Ausführung,
- fremde Adapter, Adapter-Plugins oder frei wählbare Adapter-Versionen,
- Import, Persistenz, Jobverwaltung oder Schreibfunktionen im Cockpit,
- eine Änderung des bestehenden Laufzeit-Bundle-Formats `BRUCE_PHASE_DAG_RUN_V2`.

## Öffentliche CLI

```text
bruce phase-dag-v2 execute SPEC --output BUNDLE
bruce phase-dag-v2 inspect BUNDLE
bruce phase-dag-v2 replay BUNDLE
```

`execute` liest eine Spec-Datei, führt sie mit dem fest eingebauten Katalog `nanocell-phase-v1` / `v1` aus und veröffentlicht das Ergebnis exklusiv unter `BUNDLE`.

`inspect` prüft und zeigt eine vorhandene Phase-DAG-V2-Laufzeitaufzeichnung. `replay` rekonstruiert den Lauf aus dem Bundle und prüft damit die deterministische Wiederholbarkeit. Die genaue menschen- und maschinenlesbare Ausgabe folgt den vorhandenen CLI-Konventionen.

Der Befehl steht nur zur Verfügung, wenn das NanoCell-Modul gebaut wurde. Ohne dieses Modul wird keine Ersatzimplementierung angeboten.

## Eingabeformat: `BRUCE_PHASE_DAG_SPEC_V1`

Die neue textuelle Eingabe ist ein produktorientiertes Format und ausdrücklich **nicht** das Ergebnisformat `BRUCE_PHASE_DAG_RUN_V2`.

Eine Spec beginnt mit genau dieser Kopfzeile:

```text
BRUCE_PHASE_DAG_SPEC_V1
```

Darauf folgen globale Schlüssel und danach durch Leerzeilen getrennte Knotenblöcke:

```text
BRUCE_PHASE_DAG_SPEC_V1
dag-id=demo
max-started-nodes=3
max-receipt-bytes=65536
max-trace-entries=32

node=a
depends=
core=1020a54050607080
active-index=2
context-id=reference.context_1
context-file=context-a.bin
technique=masked-match-v1
max-operations=16
max-trace-entries=18

node=b
depends=a
core=1020b54050607080
active-index=2
context-id=reference.context_1
context-file=context-b.bin
technique=rotate-match-v1
max-operations=16
max-trace-entries=18
```

### Globale Schlüssel

| Schlüssel | Bedeutung |
|---|---|
| `dag-id` | Kennung der DAG; unterliegt den bestehenden V2-Kennungsregeln. |
| `max-started-nodes` | Globales Startbudget der DAG. |
| `max-receipt-bytes` | Globales Budget für Receipts. |
| `max-trace-entries` | Globales Budget für Trace-Einträge. |

### Knotenschlüssel

| Schlüssel | Bedeutung |
|---|---|
| `node` | Eindeutige Knotenkennung. |
| `depends` | Kommagetrennte, lexikografisch sortierte und eindeutige Vorgängerkennungen; leer für Wurzelknoten. |
| `core` | Genau acht Byte als 16 Hexadezimalzeichen für den NanoCell-Kern. |
| `active-index` | Aktiver Byte-Index im NanoCell-Kern. |
| `context-id` | Kennung des NanoCell-Kontexts. |
| `context-file` | Binäre Kontextdatei, aufgelöst relativ zum Verzeichnis der Spec-Datei. |
| `technique` | NanoCell-Technik. |
| `max-operations` | Knotenbudget für Operationen. |
| `max-trace-entries` | Knotenbudget für Trace-Einträge. |

Die CLI liest Kontextdateien begrenzt, erzeugt daraus die kanonischen NanoCell-Requests und deren Digests und baut daraus eine `PhaseDagSpecV2`.

Unbekannte, doppelte oder fehlende Schlüssel, leere ungültige Werte, ungültige Hex-Werte, falsche Abhängigkeitsreihenfolgen, unbekannte Vorgänger, Zyklen, fehlende oder zu große Kontextdateien sowie Verletzungen der bestehenden V2-Budgets führen zu einer klaren Validierungsfehlermeldung. In diesem Fall wird kein Bundle veröffentlicht.

## Ausführung und Ergebnis

Nach erfolgreichem Parsen verwendet `execute` ausschließlich den vorhandenen Katalog `nanocell-phase-v1` / `v1` und ruft die bestehende Ausführung `execute_phase_dag_v2` auf. Das Resultat wird mit `pack_phase_dag_run_v2` serialisiert.

Die bestehende Semantik bleibt maßgeblich:

- erfüllte Abhängigkeiten starten Knoten gemäß der etablierten V2-Reihenfolge;
- ein fehlgeschlagener Vorgänger blockiert Nachfolger;
- ein erschöpftes Globalbudget markiert nicht gestartete Knoten als abgebrochen;
- unabhängige Knoten dürfen trotz eines Fehlerpfads anderer Knoten laufen;
- NanoCell-Ergebnisse `INCOMPLETE` und `REJECTED` sind fachliche Knotenergebnisse, keine CLI- oder Prozessfehler.

Ein gültig ausgeführter Lauf wird auch dann als Bundle veröffentlicht, wenn sein fachlicher Zustand `FAILED`, `BLOCKED` oder `CANCELLED` enthält. Der Prozess endet dabei erfolgreich, sofern Parsen, Ausführung und exklusive Veröffentlichung technisch erfolgreich waren.

Die Veröffentlichung verwendet `LocalFiles::publish_new_atomically` mit einer atomaren No-Replace-Operation und darf bestehende Ziele nicht überschreiben. Fehler vor der Veröffentlichung beim Lesen, Validieren, Ausführen oder Packen sowie eine Zielkollision enden mit Fehlercode `2` und hinterlassen kein neues Ausgabebundle. Eine nicht verfügbare atomare No-Replace-Operation scheitert ohne Link/Unlink-Fallback.

Nach erfolgreicher atomarer Veröffentlichung kann die anschließende Verzeichnissynchronisation scheitern. `ExportCommitDurabilityUnknown` bedeutet dann Fehlercode `2` und keinen Erfolgsreport, obwohl das vollständige Bundle bereits sichtbar sein kann; seine Dauerhaftigkeit nach einem Absturz ist ungewiss. Die CLI darf ein möglicherweise gültiges Bundle nicht als Rollback löschen. Vor einem erneuten Versuch müssen Ziel und Inhalt geprüft werden; ein vorhandenes Ziel bleibt auch beim Wiederholen geschützt.

## Interne Schnittgrenzen

Der Textparser und die Dateizugriffe liegen im CLI-Bereich. Der Phase-DAG-V2-Kern behält seine bestehenden, byteorientierten öffentlichen Strukturen und Funktionen. Die neue CLI-Schicht darf die vorhandene NanoCell-Request-Kodierung und den festen Adapterkatalog nutzen, führt aber keine neuen öffentlichen, generischen Adapter- oder Dateisystem-Schnittstellen in den Kern ein.

## Tests und Akzeptanzkriterien

Die Umsetzung ergänzt gezielte Tests für:

1. eine erfolgreiche Spec mit unabhängigen und abhängigen Knoten;
2. den Weg Spec → kanonischer Request → `PhaseDagSpecV2` → Bundle;
3. die deterministische Prüfung über `inspect` und `replay`;
4. die Inspektion eines real per CLI erzeugten Bundles durch das Cockpit;
5. ungültige Zyklen, fehlende oder unbekannte Abhängigkeiten und falsche Abhängigkeitsreihenfolgen;
6. fehlende, ungültige oder zu große Kontextdateien, ungültiges `core` und unzulässige Budgets;
7. fehlende Bundle-Veröffentlichung bei Validierungsfehlern;
8. atomar exklusives Publizieren ohne Überschreiben vorhandener Ausgabedateien und ohne nachträglichen Unlink-Fehler;
9. Erhaltung eines replayfähigen Bundles ohne Erfolgsreport bei `ExportCommitDurabilityUnknown`.

Die vorhandenen CMake-, CTest-, CLI-Dokumentations- und Linkprüfpfade bleiben verbindlich. Generierte CLI-Artefakte werden ausschließlich durch `docs/cli/verify_examples.py` in einer vollständigen temporären Kopie aktualisiert oder geprüft.
