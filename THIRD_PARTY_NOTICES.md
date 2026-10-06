# BRUCE Core: Hinweise zu Systembibliotheken

Dieses Dokument erfasst die beim Linux-Abnahmebuild verwendeten Laufzeitbibliotheken. BRUCE Core verteilt deren Binärdateien und Quelltexte nicht mit. Der Zielrechner stellt sie als Systembibliotheken bereit.

| Komponente | Rolle in BRUCE Core | Beim Abnahmebuild festgestellte Version | Primärnachweis auf dem Buildsystem |
| --- | --- | --- | --- |
| GMP / GMPXX | Exakte Ganzzahl- und rationale Arithmetik | GMPXX 6.3 | `/usr/share/doc/libgmp-dev/copyright` |
| OpenSSL Crypto | Kryptographische Prüfsummen- und Digestfunktionen | OpenSSL 3.5.7 | `/usr/share/doc/libssl-dev/copyright` |

Die Debian-Paketnachweise nennen für GMP den Lizenzrahmen `GPL-2+ or LGPL-3+` und für OpenSSL `Apache-2.0`. Diese Angaben beschreiben die verwendeten Systempakete. Sie erweitern oder ersetzen weder die Lizenzdateien der jeweiligen Pakete noch die Pflicht, die konkrete Art der Weitergabe vor einer Veröffentlichung zu prüfen.

Die vollständige Buildidentität einschließlich der tatsächlich aufgelösten Bibliotheksversionen liegt zusätzlich in `share/bruce/build-info.json`. Dieses Dokument ist kein Ersatz für eine vollständige Herkunftsprüfung aller zukünftigen Buildumgebungen.
