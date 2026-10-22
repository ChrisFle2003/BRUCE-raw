# Logic-Mod Artifact DAG V5 – Optionaler Remote-Restore

**Status:** IMPLEMENTIERT UND BELEGT — V5-Core, CLI und die unten verlinkten
Verträge/Workflows sind im Repository vorhanden. Diese Spezifikation bleibt
der Entwurfs- und Grenzvertrag, nicht der alleinige Implementierungsbeleg.

V5 ergänzt DAG V4 um die lokale Option `--remote-restore HTTPS_BASE`. Vor dem
FD-6-Payload eines Nichtwurzelknotens prüft der Host dessen erwartetes lokales
Vorgängerartifact. Fehlt es tatsächlich, darf ausschließlich der Host V4
Restore verwenden, prüft das lokal publizierte Ergebnis erneut und startet
erst danach den Worker. Ohne Option bleibt V4 unverändert.

Remote darf nur eine bereits durch Knotenbindung/Record erwartete Identität
wiederherstellen; es schafft keine neue Wahrheit. Unlesbare, beschädigte oder
symlink-basierte lokale Ziele gelten nicht als fehlend und lösen keinen
Netzwerkzugriff aus. TLS-, Digest-, Größen-, Manifest- oder Codecfehler starten
keinen Worker. Replay bleibt vollständig offline und workerfrei.

Die Option gehört nicht in Spec, DAG-ID, Record oder JSON. Kein automatischer
Mirror, kein Root-Restore, keine Credentialspeicherung und kein Worker-Netzwerk.

**Belege:** [Host-Core](../../../src/mod/logic_mod_artifact_dag_v5.cpp),
[CLI](../../../apps/logic_mod_artifact_dag_v5_cli.hpp),
[Core-Verträge](../../../tests/unit/logic_mod_artifact_dag_v5_tests.cpp),
[CLI-/TLS-Workflows](../../../tests/integration/test_logic_mod_artifact_dag_v5_cli.py),
[Benutzervertrag](../../cli/commands.md#logic-mod-artifact-dag-v5--hostseitiger-remote-restore-erwarteter-vorgänger)
und [Formatgrenze](../../cli/formats.md#logic-mod-artifact-dag-v5-remote-restore-ohne-formatwechsel).
