# Logic-Mod Plugin V1 — signierter isolierter Worker

**Status:** IMPLEMENTIERT UND BELEGT für den unten eingegrenzten V1-Umfang.
Die Implementierung und unabhängigen Task-Reviews konkretisieren die
ursprüngliche Spezifikation: flüchtige Registry, statischer Linux-ELF,
versiegelte memfds und obligatorischer Seccomp-Notify-Einzel-exec-Gate.
Maßgeblich sind die verlinkten Quellen und ausführbaren Verträge; die
Spezifikation selbst ist kein Laufbeleg.

## Ziel

Logic-Mod Plugin V1 führt ausführbare, signierte `.brucemod`-Pakete ein, ohne
fremden nativen Code in den BRUCE-Host zu laden. Ein explizit aktiviertes Paket
liefert genau ein digestgebundenes Worker-Artefakt. Der Host startet es als
begrenzten Child-Prozess, validiert sein begrenztes Ergebnis und behält allein
Policy-, Budget-, Claim-Scope-, Ledger- und Replay-Autorität.

## Bestehende Grundlage

**IMPLEMENTIERT UND BELEGT:** Runtime Data Mods besitzen signierte Pakete,
Truststore-Prüfung, Tree-Fragmente und Golden Cases. `ModManifestV1` und die
Worker-/Artifact-Grenzen existieren bereits.

**IMPLEMENTIERT UND BELEGT:** ein separat signierter Logic-Umschlag mit
statischem Worker, explizite In-Memory-Aktivierung, begrenzte Ausführung und
Hostvalidierung rationaler Potenzen.

**DOKUMENTIERT / KONZEPT:** dauerhafte produktive Logic-Registry, freie
Code-Plugins, Wasm und dynamisches Laden nativer Bibliotheken in den Host.

## Paket- und Aktivierungsvertrag

Ein Logic-Mod verwendet den separaten `BRUCE_LOGIC_MOD_PLUGIN_V1`-Umschlag
(höchstens 1 MiB) mit unveränderten signierten Data-Mod-Bytes, kanonischem
`logic.json`, genau einem Worker und äußerer Signatur mit dem eingebetteten
Signaturschlüssel. Der vollständige Paketdigest bindet beide Signaturen und
alle Inhalte. Mod-ID, Version, Besitzer-Scope, Mount und Routing-Golden-Cases
stammen aus dem eingebetteten Manifest. Logic-Metadaten binden exakt die
Capability `worker.execute`, dieselben Templates wie der eingebettete Tree,
einen direkten Workerdateinamen und Worker-SHA-256. Das extrahierte Artefakt
ist regulär, owner-only (0500 oder 0700), gehört dem effektiven Benutzer und
hat genau einen Hardlink; Symlinks und externe Pfade sind unzulässig.
Golden Cases prüfen signiertes Routing mit Abdeckung jedes Templates,
keine Worker-Ausführung.

```text
DISCOVERED → VERIFIED → GOLDEN_TESTED → INSTALLED_INACTIVE → ACTIVE
                                                        ├── DISABLED
                                                        └── REVOKED
```

Aktivierung ist explizit und mutexgeschützt. Ein Update ersetzt keine aktive
Version still. Nur aktive, gebundene Mod-/Template-Paare sind ausführbar.
Deaktivierte Pakete können ausdrücklich reaktiviert werden, widerrufene nicht.
Die Registry lebt ausschließlich im Prozessspeicher. Die CLI bietet genau
eine `logic-mod-plugin-v1 session` mit expliziter Aktionsliste; kein dauerhafter
Installations- oder Aktivierungszustand überlebt Prozessende. Jede Ausführung
bindet frisch unmittelbar vor Spawn und führt nur unveränderliche Bindungsbytes
aus. Bindungen sind Autorisierungssnapshots und keine während einer bereits
autorisierten Ausführung nachträglich widerrufbaren Fähigkeiten.

## Workergrenze

Der Host startet einen statischen 64-Bit-Little-Endian-ELF ohne Interpreter
als Child-Prozess aus versiegeltem memfd. Ein zweiter versiegelter memfd
liefert auf stdin den kanonischen Request mit Mod-ID, Version, Paketdigest,
Workerdigest, Template-ID, Capability, Registrydigest, normalisierter Eingabe
und festen Budgets. stdout und stderr teilen einen begrenzten Outputkanal;
alle anderen Workerdeskriptoren sind nach exec geschlossen. Umgebung ist leer,
argv ist fest, cwd privat und temporär. Der Worker erhält keine lesbaren
Paketdateien, Dateizugriffe, Payloadreferenzen, Hostpfade, Truststores,
Registry-Schreibrechte, Shell-, Netzwerk-, Unterprozess-, Budget- oder
Claim-Scope-Autorität.

Linux mit ausführbaren versiegelten memfds, `close_range`, Seccomp User
Notification und `SECCOMP_USER_NOTIF_FLAG_CONTINUE` ist Voraussetzung.
`NO_NEW_PRIVS`, eine Syscall-Allowlist und Ressourcenlimits werden vor exec
gesetzt. Der vertrauenswürdige Startstub überträgt den Notify-Listener an den
Host, der genau das erste `execveat` dieses Stubs freigibt, bevor irgendein
Workerbefehl läuft. Jeder spätere execveat-Versuch wird mit `EXEC_DENIED`
beendet und der Child-Prozess gekillt/reapiert; reguläres execve wird direkt
durch Seccomp beendet. Es gibt keinen permissiven Fallback bei fehlender
Isolation oder fehlendem Notify/CONTINUE-Support.

Der Worker antwortet über einen privaten, begrenzten Frame mit einem
`CandidateResultV1`: Status, Ergebniscodec, Ergebnisbytes, Ergebnissdigest und
optionalem begrenztem Trace-Digest. Der Host prüft Frame, Größen, Digests,
Mod-/Templatebindung, Prozessstatus und Validatorergebnis vor Akzeptanz.

## Policy und Evidenz

Der Host prüft vor dem Start Capability, Template, Aktivierungszustand und
Zeit-, Speicher-, Output- und Bytebudgets. Standard sind 250 ms Wandzeit,
64 MiB Adressraum, 4096 Eingabebytes und 16384 stdout/stderr-Bytes zusammen;
C++-Maxima sind 5000 ms, 256 MiB, 4096 und 65536 Bytes, Speicherminimum 8 MiB.
Die CLI verwendet feste Standardbudgets. CPU-Limit ist aufgerundete Wandzeit
in Sekunden; Dateigröße und Core-Dumps sind null. Der Host überwacht Wandzeit
und Output und beendet/reapiert den Child-Prozess auf Fehlerpfaden.
Der Worker kann weder Budget noch Claim-Scope erhöhen. Jeder Lauf bindet Registry-Digest, Paketdigest,
Workerdigest, Template, Budgetentscheidung, Request-Digest,
Workerantwortdigest und Host-Validatorergebnis. Nur `math.rational.power.v1`
mit Codec `rational-v1`, einer dezimalen ganzen/rationalen Basis mit höchstens
64 Zeichen und Exponent 0..16 wird unabhängig mit GMP geprüft. Akzeptierter
Scope ist stets `candidate_only`. Replay fordert eine neue aktive Bindung an,
validiert die gespeicherte Antwort und vergleicht jeden Recordbyte, ohne einen
Worker zu starten. Abweichende Identitäten schlagen kontrolliert fehl.
Dies ist Transkriptkonsistenz und Hostvalidierung, keine signierte
Ausführungsattestation und kein allgemeines Beweisverfahren.

## Fehler und Nichtziele

Ungültige Signatur, Manifest-, Worker- oder Golden-Case-Bindung verhindern
Installation oder Start. Policy-, Capability- und Templateverletzungen starten
keinen Worker. Crash, Timeout, Speicher-/Output-/Frameverletzung sowie ein
ungültiges Candidate Result ergeben einen kontrollierten Fehlstatus; kein
Ergebnis wird akzeptiert.

Nicht enthalten: dauerhafte Registry, produktiver Logic-Paketbuilder,
externe Payloadreferenzen, Dateizugriffe, Wasm, Shared Libraries im Host, Shell, Netzwerk, GPU,
Plugin-zu-Plugin-Aufrufe, automatische Updates, Marketplace,
netzwerkgestützte Schlüsselverwaltung und freie Sprache.

## Abnahme

Ausführbare Verträge belegen ein gültiges signiertes Demo-Plugin mit Golden Cases, Paket-/
Manifest-/Worker-Manipulation vor Start, Aktivierungs-/Widerrufsgrenzen,
Capability- und Templatekontrolle, Ressourcen- und Framefehler, fehlende
Claim-Scope-Autorität, gebundenen Replay sowie unveränderte bestehende
Data-Mod-, Worker- und Artifact-Store-Verträge. Der ursprüngliche
execveat-Pfad-Bypass wurde unabhängig reproduziert, durch den Einzel-exec-
Notify-Gate korrigiert und erneut unabhängig geprüft. Fehlender
Plattformsupport bleibt ein kontrollierter Startfehler.

Quellen und Belege:
[Paket-/Registrykern](../../../src/mod/logic_mod_plugin_v1.cpp),
[Workerkern](../../../src/mod/logic_mod_worker_v1.cpp),
[Paketverträge](../../../tests/unit/logic_mod_plugin_v1_tests.cpp),
[Workerverträge](../../../tests/unit/logic_mod_worker_v1_tests.cpp),
[reale CLI-Workflows](../../../tests/integration/test_logic_mod_plugin_v1.py).
Die öffentlichen [Befehle](../../cli/commands.md#logic-mod-plugin-v1--explizite-isolierte-sitzung)
und [Dateiformate](../../cli/formats.md#logic-mod-plugin-v1-paket-und-laufrecord)
beschreiben den exakten V1-Umfang. Der unveränderte CLI-Beispielverifier führt
keine Logic-Sitzungen aus; generierte Beispiele ersetzen diese Tests nicht.
