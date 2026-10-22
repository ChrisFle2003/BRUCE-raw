# DAG-Scheduler V4 – kanonische Bundles und Replay

**Status: VORSCHLAG — freigegebener Entwurf, vor Implementierungsplanung zu prüfen.**

## Ziel

V4 ergänzt den begrenzten Scheduler-V3-Adapter um portable, kanonische
Laufartefakte. Es übernimmt nur die vorhandene V3-Spec und den festen
Callback-Katalog; der V2-Kern bleibt unverändert.

## Öffentliche CLI

```text
bruce dag-scheduler-v4 execute SPEC --output BUNDLE
bruce dag-scheduler-v4 inspect BUNDLE
bruce dag-scheduler-v4 replay BUNDLE
```

`execute` schreibt ein neues Bundle atomar und exklusiv; ein bereits
vorhandenes Ziel wird nicht überschrieben. `inspect` und `replay` schreiben
keine Datei. Alle Routen sind unbedingt verfügbar, auch mit NanoCell/Cockpit
OFF.

## Bundle

Ein Bundle ist höchstens 5 MiB groß und beginnt exakt mit:

```text
BRUCE_DAG_SCHEDULER_RUN_V4
<SHA-256 des kanonischen Payloads>
```

Danach folgt ein längenpräfixierter, kanonischer Payload. Er bindet in fester
Reihenfolge die V2-Spec (einschließlich Request-Bytes und Digests), Receipts
(einschließlich direkter Input- und Output-Payloadbytes samt Digests) und den
Trace. Knoten, Abhängigkeiten, Receipts und Trace bleiben in ihren bestehenden
kanonischen Ordnungen.

**Sicherheitsgrenze:** Bundles enthalten Request- und Payload-Rohbytes. Sie
sind lokale vertrauliche Artefakte. CLI-Berichte zeigen weiter nur Metadaten;
Digest ist weder Verschlüsselung noch Autor- oder Wahrheitsnachweis.

## Decoder und Replay

Der Decoder begrenzt Gesamtgröße, Knoten, Abhängigkeiten, Receiptanzahl,
Traceanzahl, einzelne Strings und Payloads vor Allokationen. Er weist Header-,
Digest-, Längen-, Zähl-, Kanonizitäts- und Restbytesfehler ab.

`inspect` prüft Header, Digest, Dekodierung und Kanonizität und zeigt einen
Metadatenbericht ohne Rohbytes. `replay` führt den dekodierten Spec mit dem
festen V3-Katalog erneut über V2 aus, verlangt vollständige Gleichheit von
Spec, Receipts und Trace und verlangt byteidentisches erneutes Packen.

Replay bestätigt nur die Integrität und lokale Reproduzierbarkeit unter dem
festen Vertrag. Es ist kein mathematischer Beweis und keine Authentifizierung.

## Berichte und Tests

`execute`, `inspect` und `replay` verwenden klar getrennte V4-Metadatenverträge
mit Bundle-Digest, Scheduler-Version, DAG-ID, Budgets, Receipts und Trace.
Rohbytes und Hex-Requests bleiben ausgeschlossen.

Tests beweisen kanonische Bundlebytes, atomaren exklusiven Export,
Metadatenfreiheit, Replay-Gleichheit sowie Ablehnung manipulierter Header,
Digest, Länge, Request, Payload, Receipt und Trace. V3 bleibt bytegleich und
V4 funktioniert im Standard- sowie NanoCell/Cockpit-OFF-Build.

Nicht Teil von V4 sind freie Skripte/Callbacks, externe Speicher, Netzwerk,
Cockpit, Parallelität oder Migration alter V3-Läufe.
