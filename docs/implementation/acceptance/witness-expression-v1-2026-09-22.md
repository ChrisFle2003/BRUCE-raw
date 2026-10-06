# Abnahme: Rationale Gegenbeispiele in der Ausdruckssuche – 2026-09-22

Stand: Core 0.34. Implementiert und aktuell verifiziert.

[Vertrag](../71-witness-expression-contract.md) · [Benutzung](../72-witness-expression-usage.md)

## Gelieferter Ablauf

Begrenzte Ausdrucksgrammatik → Training/Gegenrechnung/Hold-out → Normalisierung mit ursprünglichen Ausschlüssen → modularer Bereichsvergleich mit optionaler rationaler Zeugensuche → erhaltene Gruppen und Originale samt Zeugen-Trace → V10-Export und Replay → Z-Auswahl aus vollständiger Forschung.

Der Modus `rational-q-witness` unterstützt alle drei Ausdrucksplaner. Portfolio und Bereichscache akzeptieren ihn noch nicht. Nur dieser neue Modus erlaubt ausdrücklich gewählte Ergebnisgrenzen bis 256 Originale; der Standard bleibt 128.

## Verifikation

| Prüfung | Ergebnis |
| --- | --- |
| Debug-Build | C++20 mit `-Wall -Wextra -Werror`; erfolgreich |
| Vollständige Debug-Suite | 65/65 bestanden |
| Vollständige UBSan-Suite | 65/65 bestanden; `-fsanitize=undefined -fno-sanitize-recover=all`, keine Sanitizer-Meldung |
| Gesamtdemo | Alle bisherigen Abläufe und neuer Zeugen-/Auswahl-/Replaypfad erfolgreich |
| Neue Integration | Drei Methoden: tatsächliche Gegenzeugen, unabhängiges Einsetzen in Originalbäume, Replay/Manipulation, vollständige Forschung und Auswahl |
| Historische Exporte | 24 vorhandene `.bruce`-Fixtures unverändert replaybar |
| Altvergleich mit Core 0.33 | 45 Ausdrucksläufe über drei Planer, bisherige Gleichheits-/Cachemodi und drei Arbeitsbudgets byteidentisch |
| Unabhängiges Review | Keine handlungsbedürftigen Befunde; damalige zwei Integrationsmethoden unabhängig bestanden, vier alte Modi byteidentisch und Trace-Reservierung geprüft |

## Tatsächlicher Suchlauf

Der Beispielauftrag verwendet `cached-grammar`, Konstante 3, Operatoren `-^/`, höchstens neun Baumknoten und eine ausdrücklich gewählte Grenze von 256 Originaltreffern. Die neuen Nullfunktionsbeispiele lassen die Gegenstelle 0 außerhalb der Samples.

| Merkmal | Ergebnis |
| --- | --- |
| Originaltreffer / Gruppen | 256 / 6 |
| Logische Gesamtarbeit | 558976 |
| Abgeschlossene ausgewählte begrenzte Gegenzeugen | 4 |
| Vollständig geprüfte Suchstellen abgeschlossener Vergleiche | 4 |
| Begonnene Bereichsvergleiche | 430 |
| Abgeschlossene Bereichsunterschiede einschließlich analytischer Entscheidungen | 180 |
| Offene abgeschlossene Bereichsvergleiche | 0 |
| Forschungsstatus | INCOMPLETE / ResultBudgetExceeded |

Die vier Zeilen betreffen die Kandidaten 8978, 9004, 9248 und 9274 gegenüber Vertreter 2, jeweils bei `x=0`. Python `Fraction` setzt den ausgegebenen Zeugen unabhängig in die gespeicherten Originalbäume beider Seiten ein. Genau die angegebene Seite ist undefiniert. Die Kandidaten können anschließend zur bestehenden Gruppe mit derselben rationalen Lücke gehören; `DIFFERENT` gegenüber einem Vertreter ist kein pauschales Verbot weiterer Gleichheitsprüfung.

Dieser Lauf wird weder als vollständig noch als allgemeiner Funktionsbeweis aus Samples ausgegeben. Das zuvor verwendete kubische Nullfunktionsbeispiel enthält 0 in den Trainingsdaten und filtert diese Kandidaten aus; es bleibt als Vergleich unveränderter Gruppen/Originale bei Standardgrenze 128 im Test erhalten. Für den tatsächlichen Zeugenpfad werden eigene Eingaben und die explizite größere Grenze verwendet.

## Grenzen, Abbrüche und Auswahl

- Alle drei Ausdrucksplaner: jede positive Arbeitsgrenze vor Abschluss eines kleinen vollständigen Falls, exakte Abschlussgrenze, sämtliche Checkpoints, Callback-Abbruch, Original-, Trace- und Tracebytegrenzen.
- Tatsächliche begrenzte Zeugensuche: erste abgeschlossene Zeugenveröffentlichung je Planer lokalisiert und Abbrüche unmittelbar davor und danach geprüft. Keine halbfertigen Zeugen, keine verlorenen oder doppelten Originale; sämtliche Teilbefunde replaybar.
- Gegenprüfung der Arbeitsabrechnung: Planungs-, Normalisierungs- und Bereichskosten sind im Gesamtbudget enthalten. Trace-Ereignisse und Bytes bleiben innerhalb ihrer Grenzen.
- Neue Ergebnisgrenze 257 wird zurückgewiesen; 256 bleibt in alten Modi ungültig. Portfolio-, fachfremde Strategie- und Bereichscachekombinationen mit dem neuen Modus werden zurückgewiesen.
- Vollständige kleine Forschung in allen drei Planern: `VERIFIED_ON_SAMPLES`, Auswahl bei 0 liefert 0, Polstelle −1 bleibt gesperrt. Auswahl-Replay funktioniert nach Löschen der ursprünglichen Forschungsdatei.
- Unvollständige Zeugenforschung ist als Auswahlquelle gesperrt. Veränderte Zeugen-Tracewerte, Zähler, Engine und Vergleichspolitik werden auch mit neu berechnetem Dateihash abgelehnt.

## Offene Arbeit

Portfolio-/Experience-Einbindung und dazu passende Bereichscaches sind das nächste Integrationspaket. Die Suche ist weiterhin auf reduzierte Brüche mit Zählerbetrag und positivem Nenner bis 16 beschränkt; erfolglose Suche beweist keine Nullstellenfreiheit. Weitere Nullstellenverfahren, Suchoptimierungen und Experience-Korrekturketten bleiben offen. Kein vollständiger M2-/M4-/Gesamtprojektabschluss.
