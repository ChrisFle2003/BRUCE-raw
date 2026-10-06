# Sicherheitsrichtlinie

## Unterstützte Versionen

Der aktive Entwicklungsstand ist Core 0.102 auf dem Standardbranch. Für ältere
Stände und historische Dokumentationspakete sind keine Sicherheitsupdates oder
Backports zugesagt.

## Sicherheitslücken melden

Bitte veröffentliche Sicherheitslücken, mögliche Geheimnisse oder
Angriffsdetails nicht in einem öffentlichen Issue, Pull Request oder
Diskussionsbeitrag.

1. Nutze in der Repository-Ansicht unter **Security** die Funktion **Report a
   vulnerability**, sofern sie verfügbar ist.
2. Falls diese Funktion nicht verfügbar ist, kontaktiere einen Maintainer nur
   über einen von ihm oder ihr bereits öffentlich angegebenen privaten
   Kontaktweg. Dieses Repository erfindet keinen E-Mail- oder Chat-Kanal.
3. Gib eine minimale Reproduktion, betroffene Version oder Commit, mögliche
   Auswirkungen und sichere Schritte zur Verifikation an. Entferne Tokens,
   personenbezogene Daten und private Pfade.

Für diese Richtlinie ist kein Reaktionszeit-SLA zugesagt. Repository-Eigentümer
sollten vor einer öffentlichen Veröffentlichung die private
Vulnerability-Reporting-Funktion von GitHub aktivieren oder einen belastbaren
privaten Meldeweg veröffentlichen.

## Umfang

Relevant sind insbesondere der Build- und Installationspfad, Parser für lokale
Eingabedateien, gespeicherte Replay- und Exportdateien sowie die GitHub-Actions-
Konfiguration. Die Prüfung eines gemeldeten Problems garantiert weder eine
sofortige Korrektur noch eine Veröffentlichung als Security Advisory.
