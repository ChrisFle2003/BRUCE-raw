# Lieferabnahme: Core 0.72 auf Bruce-AI

## Ergebnis

Der per SSH/SFTP erreichbare Remote-Quellbaum `/home/chris/Desktop/Bruce_raw` wurde mit dem hostbezogenen Dependency-Wrapper frisch als Release konfiguriert und gebaut.

## Nachweis

- C++20, GNU 14.2.0, CMake 3.31.6, OpenSSL 3.5.7 und GMPXX 6.3.0.
- 144 Buildziele erfolgreich gebaut.
- `bruce-install-audit-v1` bestand für die verschobene Installation und das CPack-TGZ.
- Die installierte und die entpackte CLI führten Forschung, Export, Replay, Budgetfehler-Schutz und die 26 historischen Fixtures erfolgreich aus.
- Paket: `bruce-core-0.72.0-Linux-x86_64.tar.gz`, SHA-256 `3cc4480d022f188503e8641f01a713171c46630eb5f50d1cc7446bbd1c2d52f2`.

Die Abnahme bestätigt die getestete Remote-Linux-Umgebung. Sie ist keine öffentliche Veröffentlichung, Lizenzfreigabe oder bitidentische Reproduzierbarkeitsgarantie.
