# Exakte Zahlendomänen – Core 0.3

Fortsetzung der freigegebenen Roadmap im bestehenden C++-Kern. GMP/GMPXX 6.3 ist lokal installiert. Keine zusätzliche Datenbank, kein Netzwerkdienst.

## Umfang und Verträge

`math` erhält begrenzte GMP-Ganzzahlen und kanonische rationale Zahlen. Dezimale Integerstrings bzw. `Zähler/Nenner`, keine Floats. Nenner null wird vor GMP-Aufrufen abgewiesen; Vorzeichen/Kürzung werden normalisiert. Die Bitgrenze gilt für rohe Eingabekomponenten und die gekürzten Ergebnisse jedes Rechenschritts. Standard 4096, zulässig 1–16384 Bits pro Zähler/Nenner. Vorübergehende Produkte sind durch die Eingabegrenzen auf wenige Vielfache davon begrenzt.

`calculate` stellt add/sub/mul/div/pow, ganzzahliges divmod und modpow zur Verfügung. Exponenten sind nichtnegative uint64-Steuerparameter. Ganzzahliges div verlangt Rest null; divmod verwendet `a=q*b+r` mit `0 <= r < |b|`. Rationale Division wird exakt gekürzt.

`research --domain bigint|rational` erweitert den vorhandenen Nutzerpfad. Der bisherige int64-Pfad bleibt einschließlich seines Replayformats unverändert. Neue Aufgaben verwenden eine eigene explizite Engine-/Paketversion. Das ist eine numerische Erweiterung der affinen Strategie, keine allgemeine Formelsynthese.

BigInt: a,b sind Ganzzahlen aus einem frei positionierbaren, höchstens 4096 Werte breiten Intervall. Rational: reduzierte a=n/d und b=n/d, n aus demselben Intervall, d von 1 bis `--max-denominator` (maximal 16). Brüche werden nur in gekürzter Form erzeugt; 0 nur als 0/1. Parameter werden lazy besucht. Kandidatenidentität folgt dem Rohslotpaar, nicht dem Wert.

Neue Lern-/Prüfwerte dürfen groß oder rational sein. Kanonisch gleiche x-Werte zählen als doppelt; Hold-out darf keine Lern-x wiederverwenden. Vorwärtsprüfung und Rückrechnung `(y-b)/a` (für a=0 direkt) müssen stimmen. Eine überschrittene Zahlengrenze während der Forschung stoppt als `INCOMPLETE / IntegerBitLimit`; daraus wird kein negativer Suchbeweis.

Die bestehenden Kandidaten-/Operations-/Trace-/Ergebnisbudgets bleiben, mit höchstens 128 behaltenen Ergebnissen in der neuen Domäne. Eine Sampleprüfung reserviert vier Rechenschritte. Zusätzlich gilt `--max-trace-bytes`, Standard 1 MiB, maximal 4 MiB. Eingabedateien und kanonische Samples insgesamt sind begrenzt. Abbruch ist an Kandidatengrenzen replaybar.

Z-Kompatibilität kann neue vollständige Exporte importieren, sofern der Projektionswert exakt als int64 darstellbar ist. Rationale/nicht darstellbare Werte werden ausdrücklich zurückgewiesen; Z-Prädikate und Adressen werden in diesem Paket nicht still auf neue Zahlentypen umgestellt.

## Schritte

- [x] Neue CLI-Abnahmen rot nachweisen; numerischen Backendvertrag implementieren.
- [x] Neue Domänen in `research` mit Gegenprüfung, Grenzen und kanonischem Replay ergänzen.
- [x] Replay-Dispatcher und Forschungsimport der Z-Kompatibilität erweitern, alte Exporte regressionsprüfen.
- [x] Unabhängige Zahlenvergleiche, Grenzfälle, Review und UBSan durchführen.
- [x] Demo, Status und Benutzungsdokumentation aktualisieren.
