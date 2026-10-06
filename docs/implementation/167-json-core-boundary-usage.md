# Core 0.87: gemeinsamer JSON-Stringhelfer im Core

`json_string` liegt jetzt unverändert in `core/json.cpp` und wird durch `core/json.hpp` deklariert. Die Funktion besitzt ausschließlich Standardbibliotheksabhängigkeiten. Der Forschungsheader bindet den neuen Header weiterhin ein, damit bestehende Aufrufer kompatibel bleiben.

Store und Gate-Renderer binden nun direkt den Core-Helfer ein. Sie benötigen den Forschungsheader nicht mehr allein für ihre JSON-Strings. Der Quellenaudit bleibt als explizite Anwendungsabhängigkeit des Stores erhalten; dies ist keine vollständige Bibliotheksentkopplung.

Die Kodierungslogik wurde ohne Verhaltensänderung verschoben: gültiges UTF-8 bleibt erhalten; Steuerzeichen, Anführungszeichen und Backslashes werden escaped, ungültige Bytes erhalten den bisherigen Unicode-Escape. Der neue isolierte Test baut nur den Core-Helfer und prüft leeren Text, alle 32 ASCII-Steuerzeichen, Anführungszeichen/Backslash, gültiges UTF-8 sowie überlange, abgeschnittene und Surrogat-Bytefolgen.

Es wurden keine fachlichen Regeln, Freigaben, Gewichte, Bundleverträge oder Ausgabeschemas geändert. Die menschliche Regelhoheit aus ADR-013 bleibt unverändert. Das Paket entfernt eine Darstellungsabhängigkeit; allgemeine Aktivierungs- und Übergabeverträge bleiben offen.

## Prüfnachweis am 25.09.2026

Lokal und remote bestanden 111/111 Testgruppen. Der Nutzerpfad `./build/bruce` auf Bruce-AI wurde als 0.87.0 gebaut und geprüft. Der Gatekatalog war vor und nach dem Update bytegleich. Installation und TGZ-Paketprüfung bestanden; die CLI-Prüfung bestand lokal und remote mit 92 Beispielaufrufen und gültigen internen Links. Quellen und Dokumentation wurden per SCP übertragen.
