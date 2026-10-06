# Core 0.72: Systembibliotheks-Hinweise im Auslieferungsarchiv

## Ziel

Die Installations- und Archivprüfung soll nicht nur den eigenen Lizenztext und `NOTICE`, sondern auch die nachvollziehbare Zuordnung der beim Abnahmebuild verwendeten Systembibliotheken enthalten.

## Umsetzung

- [THIRD_PARTY_NOTICES.md](../../THIRD_PARTY_NOTICES.md) führt GMP/GMPXX und OpenSSL Crypto mit Rolle, festgestellter Version und dem Lizenz-/Copyrightnachweis der verwendeten Debian-Pakete auf.
- `DEPENDENCIES.md` verlinkt auf diesen Nachweis und begrenzt seine Aussage auf die konkrete Buildumgebung.
- CMake installiert den Nachweis nach `share/bruce/THIRD_PARTY_NOTICES.md`; der Inhalt fließt zudem in den Quellenindex ein.
- `scripts/verify_install.py` bricht ab, wenn der Nachweis in einer verschobenen Installation oder im CPack-Archiv fehlt.

## Abnahme

Der frische Release-Build auf Bruce-AI vom 25. September 2026 verwendete OpenSSL 3.5.7 und GMPXX 6.3.0. Alle 106 CTest-Tests bestanden. Danach bestanden Installation, Archiventpackung, Forschungsexport mit Replay, Budgetfehlerschutz und das Replay der 26 historischen `.bruce`-Fixtures. Das geprüfte Archiv hieß `bruce-core-0.72.0-Linux-x86_64.tar.gz` und hatte SHA-256 `3b06ac750a2e694107a06be12e7d110bb8925ea01378d1ab709caaf7d6521e0c`.

## Grenzen

Dieser Nachweis bündelt keine Fremdbibliotheken und ist keine vollständige Herkunfts- oder Rechtsprüfung für jede zukünftige Buildumgebung. Der Rechteinhaber des eigenen BRUCE-Core-Quellcodes, Christopher Flegel, ist im installierten `NOTICE` benannt. Vor einer öffentlichen Veröffentlichung bleibt eine kontrollierte Release-Signatur erforderlich.
