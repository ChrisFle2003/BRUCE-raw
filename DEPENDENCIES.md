# BRUCE: lokale CLI-Auslieferung und Abhängigkeiten

Das lokale Paket enthält die CLI, Buildinformationen und eine Prüfsummenliste der berücksichtigten Build-, Quell- und Testdateien. Es enthält kein C++-SDK und bündelt keine fremden Bibliotheksdateien.

## Systemvoraussetzungen

Die derzeitige Abnahme gilt für Linux auf der im Buildbericht genannten Architektur. Die CLI nutzt die C++-Laufzeit, GMP/GMPXX und OpenSSL Crypto als Systembibliotheken. Auf einem anderen Rechner müssen kompatible Bibliotheken vorhanden sein; das Archiv ist kein statisch verknüpftes Universalpaket.

Für den Build: C++20-Compiler, CMake >=3.20, Ninja oder ein anderer passender CMake-Generator, pkg-config sowie Entwicklungsdateien für GMP/GMPXX und OpenSSL. Tests und die Installationsprüfung benötigen Python 3; das Archiv wird mit CPack/TGZ erzeugt.

## Lizenzstatus

Der Rechteinhaber des eigenen BRUCE-Quellcodes ist Christopher Flegel. Der
eigene Quellcode steht unter der [BRUCE Proprietary License](LICENSE). Es
werden keine Nutzungs-, Vervielfältigungs-, Weitergabe- oder Verkaufsrechte
erteilt; insbesondere benötigen Verkauf und Weiterverkauf eine vorherige
schriftliche Vereinbarung mit Christopher Flegel. Die Hinweise in
[NOTICE](NOTICE) und die Lizenzbedingungen der Abhängigkeiten bleiben bei
jeder Weitergabe zu beachten.

GMP/GMPXX, OpenSSL sowie Compiler- und C++-Laufzeit unterliegen jeweils ihren eigenen Lizenzen. Die beim Abnahmebuild festgestellten Laufzeitbibliotheken, ihre Herkunft und die zugehörigen Primärnachweise stehen in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Maßgeblich bleiben die Lizenz-/Copyrightdateien der tatsächlich installierten Systempakete. Für eine öffentliche Auslieferung sind verwendete Bibliotheksversionen und zugehörige Lizenzpflichten gesondert zu prüfen. Die Bibliotheksversionen des Builds stehen in `share/bruce/build-info.json`.

## Identität und Grenzen

`bruce --version` liefert die CMake-Projektversion. `source-files.sha256` listet Eingabedateien; dessen SHA-256 steht im Buildbericht. Binär- und Archivprüfsummen stehen im separat erzeugten Installationsprüfbericht. Diese Angaben sind Integritäts-/Identitätsnachweise, keine Signaturen und kein Nachweis bitidentischer Builds.
