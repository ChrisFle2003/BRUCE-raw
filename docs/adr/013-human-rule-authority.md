# ADR-013: Regelhoheit ausschließlich beim Menschen

Stand: Core 0.93, 26.09.2026. [Historischer Entwurfsstand](013-human-rule-authority-history.md).

## Entscheidung

Regeln dürfen nur nach ausdrücklicher menschlicher Freigabe der konkret vorgelegten Änderung wirksam geändert werden. Ein allgemeines „weiter“, ein positiver Score, eine Erfahrung oder ein bestandener Test ist keine solche Freigabe. Das umfasst Einführung, Änderung, Entfernung und Rückwechsel. Die Vorgabe bleibt auch für künftige Regelarten maßgeblich.

Der Nutzer hat den lokalen Betrieb ohne getrennte Konten, Signierschlüssel und externen Aktivierungsdienst gewählt. Diese frühere technische Voraussetzung ist abgelöst. Die Entscheidung führt keine Behauptung einer menschlichen Identitätsprüfung ein.

## Implementierter lokaler Vertrag

Für numerische Kompatibilitätssnapshots existiert ein verwalteter Weg mit konkreter Terminalbestätigung. Der Vorschlag bindet alte/neue Fassung, Zielpfad, Generation und Journalbasis. Vor dem Schreiben wird die aktuelle Basis unter kooperativer Schreibsperre erneut geprüft. Bestätigte Wechsel werden mit beiden Fassungen und lokaler Zeit dauerhaft in derselben Datei wie der ableitbare aktive Zustand bewahrt. Initialisierung und Rückwechsel erfordern ebenfalls neue konkrete Bestätigung.

Ein separater Exportweg erzeugt nach Bestätigung nur eine neue Regeldatei. Er aktiviert sie nicht. Die [Freigabeverträge](../contracts/05-human-rule-approval.md) unterscheiden beide Wege und ihre Grenzen.

Erfahrungsrouting verwendet vorhandene festgelegte Semantik zur Auswertung von Stimmen. Es ändert keine Prädikate, festen Regelgewichte oder Freigabegrenzen. Forschungs-/Erfahrungsübergaben aktivieren keine Regeln. Historischer Replay benutzt eingebettete alte Regeln nur zur Wiederherstellung des ursprünglichen Laufs.

## Reichweite und offene Durchsetzung

Das Quellenfeld `manual`, ein Digest, ein Terminal oder ein JSON-Feld beweisen keine menschliche Identität. Programme desselben Benutzers können den Dialog nachbilden oder Dateien direkt ändern. Diese lokale Vertrauensgrenze ist akzeptiert. Direkte `--rules`-Aufrufe bleiben verfügbar und verlangen keinen Freigabenachweis; die Verwaltung erzwingt daher nicht global sämtliche Regelverwendungen.

Die Runtime verwaltet gegenwärtig numerische Kompatibilitätssnapshots, nicht sämtliche eingebauten oder künftigen Regelarten. Identifizierte Akteure, Ablehnungsjournale, expliziter Vorschlagswiderruf, Migration und Rotation sind nicht implementiert. Diese Grenzen werden nicht durch den Status einer bestandenen lokalen Testsuite aufgehoben.

## Nachweise

`rule_proposal_contracts`, `rule_review_workflows`, `rule_history_contracts` und `rule_store_workflows` prüfen Paketbindung, Dialog, Ablehnung, konkurrierende Änderungen, Rückwechsel, Journalgrenzen und Speicherübernahme. Die Tests verwenden ausschließlich temporäre Regeln. Das [lokale Abnahmeprofil](../implementation/174-local-cli-acceptance-profile.md) bewertet genau diesen begrenzten Vertrag und keine allgemeine Identitäts- oder Sicherheitsarchitektur.
