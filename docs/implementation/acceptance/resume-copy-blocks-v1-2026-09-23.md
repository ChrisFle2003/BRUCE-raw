# Abnahme blockweise Fortsetzungs-Elternkopie / Core 0.67 – 23.09.2026

## Lieferung

Der Elterninhalt im Grid-Fortsetzungs-Payload wird in höchstens 65.536 Bytes großen Abschnitten kopiert. Der bestehende Records-Callback gibt den ersten Abschnitt frei; weitere Abschnitte erhalten zusätzliche Kontrollpunkte. Vorhandene Serialisierungsabbruchmodi und das gemeinsame Sitzungsbudget erreichen sie automatisch. Keine neue Bundleversion, keine Mathematikänderung und kein neuer CLI-Modus.

## Prüfungen

Der neue Unit-Test scheiterte vor Umsetzung erwartungsgemäß mit `Large parent copy has no inner checkpoints`. Nach Umsetzung bestehen die gezielten Gruppen.

Die Unit verwendet einen tatsächlichen replaybaren Vorgänger mit großem, gültigem Pfad und prüft jeden zusätzlichen Kopierpunkt auf Exception-Weitergabe vor Beginn des äußeren Hashens. Sitzungsbudget und Abbruchcallback müssen in SERIALIZING scheitern. Zusätzlich werden Elternlängen 0, 65.535, 65.536, 65.537, 131.072 und 131.073 sowie eingebettete NUL-Bytes getestet. Der serialisierte Elterninhalt wird anhand seines Längenheaders unabhängig extrahiert und mit der Eingabe verglichen. Synthetische Eltern dienen nur dem Serializer-Grenztest, nicht als gültige Forschungsquellen.

Der neue CLI-Test prüft bytegleiche Exporte, Replay und einen Budgetfehler in der Kopierphase ohne Export. Eine bereits vorhandene Zieldatei bleibt unverändert.

Unabhängige Vergleiche mit der gesicherten Core-0.66-CLI: drei Elterngrößen, sechs bytegleiche Exporte einschließlich des bestehenden Records-Signalmodus. Mit aktivem Sitzungsbudget steigt ausschließlich der SERIALIZING-Anteil, exakt um die zusätzlichen Kopierpunkte. Alle 26 historischen Fixtures bleiben unverändert.

Unabhängiges Review: keine blockierenden Fehler. Der Hinweis zur unabhängigen Byteprüfung wurde durch direkte Elternextraktion und Versionsvergleich umgesetzt. Keine Produktionsänderung nach diesem Review.

Vollständige Release-Suite: **105/105 Gruppen**, 49,76 Sekunden. Acht gezielte UBSan-Gruppen einschließlich beider neuer Tests, Serialisierung, Sitzungsbudget und Installation bestehen. Keine erneute vollständige UBSan-Suite in dieser Lieferung. Ein zuvor nicht vorhandenes `build-release` wurde konfiguriert und gebaut.

## Lokale Auslieferung

`runs/releases/core-0.67/bruce-core-0.67.0-Linux-x86_64.tar.gz` und `runs/releases/core-0.67/installation-report.json` wurden mit dem bestehenden Installations-/Archivprüfer erzeugt. Verschobene Installation und entpacktes Archiv prüfen jeweils Forschung, Export/Replay, Budgetfehler und alle 26 historischen Fixtures. Binär-, Quellenindex- und Archivprüfsummen stehen im Bericht.

## Grenzen

Reservierung, abschließender Payload-/Bundlezusammenbau und andere große Kopien bleiben teilweise atomar. Keine maximale Signallatenz oder Prozess-RAM-Grenze. Ein früher ausreichendes Prüfpunktlimit kann bei großen Eltern jetzt zu klein sein. Kein vollständiger Architektur-/M0–M2-Abschluss und keine öffentliche Releasefreigabe.

[Vertrag](../139-resume-copy-blocks-contract.md) · [Benutzung](../140-resume-copy-blocks-usage.md)
