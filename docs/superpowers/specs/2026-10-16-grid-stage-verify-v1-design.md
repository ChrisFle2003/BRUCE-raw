# Grid Stage Verify V1 — Design

**Status: IMPLEMENTIERT UND BELEGT** für vollständige Exact-/Metric-Quellen,
Quellbindung, kanonisches Replay, CLI und endlichen Assessment-Scope.
Frische native Abnahme am 2026-10-16: vollständiger Build und 11/11 relevante
CTests bestanden; [Core-Verträge](../../../tests/unit/grid_stage_verify_tests.cpp)
und [CLI-Workflows](../../../tests/integration/test_cli.py). Diese Abnahme ist
kein Erfolg der gesamten CTest-Suite; Cockpit-Sockettests bleiben durch die
Sandbox eingeschränkt, Git-Metadaten fehlen. Generierte CLI-Beispiele führen
Verify nicht aus.

**IMPLEMENTIERT, ABER NICHT AKTUELL VERIFIZIERT:** erfolgreiche
`REFUTED`-Auswertung und deren Replay sind aus zulässigen aktuellen Sweep-V1-
Quellen nicht erreichbar. Deren Pflicht-Replay regeneriert dieselbe feste
Regel. Die defensive Darstellung bleibt erhalten; künstliche Änderungen
werden abgewiesen. Die folgenden Abnahmekriterien berücksichtigen diese
nach der Implementierungsprüfung ausdrücklich festgehaltene Grenze.

## Auftrag

BRUCE prüft die bekannte feste Regel `100*(2^stage-1)` vollständig über die direkt unterstützte Stage-Domäne `0..57` gegen einen replaybaren Grid Stage Sweep V1. Die Route sucht keine Formel und behauptet keinen symbolischen Beweis über unbegrenzte natürliche Zahlen.

## Öffentlicher Vertrag

```text
bruce grid-stage-verify --from-sweep SWEEP.bruce [--output VERIFY.bruce]
```

Die Eingabe muss ein replay-valider `BRUCE_GRID_STAGE_SWEEP_BUNDLE_V1` mit fester Regel `scaled-exponential-v1`, vollständiger Domain `0..57`, Status `COMPLETED`, Exact- oder Metric-Modus, 58 lückenlosen aufsteigenden expliziten Stages und gültiger Sweep-Evidence sein. Modular, unvollständige, fremde oder replay-ungültige Sweeps werden mit `InvalidSweepVerificationSource` abgewiesen.

## Prüfergebnis

Der Lauf prüft jeden Eintrag erneut gegen die kanonische Formel und liefert nur `VERIFIED_ON_DOMAIN` oder `REFUTED`.

Ein Erfolg enthält:

```text
verified_domain = 0..57
checked_entries = 58
formula = 100*(2^stage-1)
claim_scope = finite-uint64-scaled-exponential-stage-domain-0..57
sweep_digest = SHA-256 des gebundenen Sweep-Bundles
status = VERIFIED_ON_DOMAIN
```

Die defensive Darstellung `REFUTED` enthält den ersten Unterschied mit Stage,
erwartetem und beobachtetem Wert, alle 58 Vergleiche und
`verified_domain: null`. Ein erfolgreicher solcher Lauf ist in der aktuellen
öffentlichen V1-Eingabedomäne nicht nachgewiesen. Exact vergleicht Werte,
Metric ausschließlich Bitlängen. Ungültige Quellen erzeugen keinen Teilbericht.

## Evidence und Replay

Header: `BRUCE_GRID_STAGE_VERIFY_BUNDLE_V1`. Seine Payload bindet den vollständigen Sweep-Quellbundle unverändert, dessen SHA-256, Formel, Claim-Scope, alle 58 kanonischen Prüfergebnisse und den Endstatus.

Replay rekonstruiert zuerst den eingebetteten Sweep, prüft danach alle Regelvergleiche und vergleicht die vollständige Verifikations-Payload. Neu gehashte Manipulationen an Sweep, Digest, Formel, Prüfergebnis oder Status führen zu `ReplayMismatch`.

`inspect-run` und zentrale Anwendungsauswertung zeigen Status und endlichen Claim-Scope. Sie behaupten keinen allgemeinen mathematischen Beweis.

## Nichtziele

- Keine freie Formel, freie Regelwahl oder Formelentdeckung.
- Kein allgemeiner Theorem-Prover und keine Aussage über Stufen außerhalb `0..57`.
- Keine erneute Ausführung oder Veränderung der Sweep-Debug-Traces.
- Kein Modular-Sweep, Teil-Sweep oder Sweep-Resume als Quelle.
- Keine NanoCell-, Worker-, GPU-, Netzwerk- oder physikalische Modellierung.

## Abnahmekriterien

1. Vollständige Exact- und Metric-Sweeps `0..57` ergeben `VERIFIED_ON_DOMAIN` mit 58 Prüfungen und exaktem Scope.
2. `REFUTED` mit erstem Unterschied ist defensiv darstellbar. Ein replay-valider, inhaltlich abweichender Sweep ist mit aktuellen Sweep-V1-Semantiken nicht konstruierbar; kein erfolgreicher Refutationslauf wird als abgenommen behauptet.
3. Modular, unvollständig, falsch begrenzt oder replay-ungültig wird ohne Verifikationsbundle abgewiesen.
4. Verifikations-Replay reproduziert Exact-/Metric-Erfolg und weist rehashed Manipulationen sowie künstliche Refutationsberichte ab. Erfolgreiches Refutations-Replay bleibt aus der aktuellen zulässigen V1-Eingabedomäne unerreichbar.
5. Zentraler `replay`, `inspect-run` und Assessment zeigen den endlichen Scope.
6. Bestehende Grid-, Sweep-, Batch- und Resume-Ausgaben bleiben bytegleich.
7. CLI-Hilfe und Handbuch erläutern die endliche Domain korrekt.
