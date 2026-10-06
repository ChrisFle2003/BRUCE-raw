# Release-Identität und Signierung

Ein BRUCE-Release besteht aus einem versionierten TGZ-Archiv, dessen SHA-256-Prüfsumme, einer von der Projektverantwortung kontrollierten detached Signatur und dem zugehörigen öffentlichen Signaturschlüssel.

Der Rechteinhaber des eigenen BRUCE-Core-Quellcodes ist Christopher Flegel; die Copyrightzeile steht im mitgelieferten `NOTICE`.

Vor dem Signieren muss die Release-Abnahme bestehen: frischer Build, Installation in ein leeres Ziel, Export und Replay, Budgetfehler-Schutz sowie historische Fixtures. Die Signatur wird über das geprüfte Archiv erstellt, nicht über ein nachträglich neu erzeugtes Paket.

Der private Schlüssel wird nicht im Quellbaum, in Buildverzeichnissen oder im Archiv abgelegt. Eine Signatur bestätigt Ursprung und Unverändertheit des konkreten Archivs; sie ersetzt weder die Apache-2.0-Lizenz noch eine externe Sicherheits- oder Mathematikprüfung.
