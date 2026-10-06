# Core 0.102: Steuerungsvertragsmatrix und Kompatibilitätsprüfung

Dieses Paket führt keine neue Controller-Runtime ein. Es macht den bereits
vorhandenen Unterschied zwischen Sitzung, Gate und Transfer ausführbar
nachweisbar und ordnet ihn einem stabilen Vertrag zu.

## Gelieferter Umfang

- [ADR-015](../adr/015-control-contract-compatibility.md) erhält die
  Entscheidung, die drei Verträge zunächst kompatibel zu bewahren statt sie
  stillschweigend zu normalisieren.
- Die [Steuerungsvertragsmatrix](../contracts/06-control-and-publication.md#kompatibilitätsmatrix-der-steuerungswege)
  benennt Budgeteinheit, Priorität, Fehlerhülle, Veröffentlichungsgrenze und
  CTest-Nachweis pro Familie.
- `scripts/verify_control_contract_matrix.py` führt temporäre echte
  CLI-Aufrufe aus. Es prüft die verschiedenen Fehlerhüllen bei einem
  Nullbudget, fehlende Ziel-/Tempdateien nach einem Abbruch und einen
  replaybaren nativen `INCOMPLETE`-Befund.
- CTest registriert diesen Prüfer als `control_contract_matrix` und seinen
  Fehlermaskierungs-Negativtest als `control_contract_matrix_unit`.

Es wurden keine C++-Controller, CLI-Argumente, Bundleversionen,
Ergebnisfelder oder mathematischen, Gate- und Replaysemantiken geändert.
Die Matrix behauptet weder eine universelle Zeit- oder Speichergrenze noch
eine gemeinsame Budgeteinheit.

Im aktuellen Core-0.102-Quellstand verwenden auch `experience revoke` und
`experience replace` bei Angabe mindestens einer Transferoption den
bestehenden Transfercontroller. Ohne diese Optionen bleiben sie direkte
native Verwaltungsaufrufe. Diese Zuordnung ist Bestand; die Matrix führt
keinen zusätzlichen Controller für sie ein.

## Lokale Prüfung

Die Prüfung benötigt die zum lokalen Dependency-Bundle passende Buildumgebung:

```bash
env LD_LIBRARY_PATH=/home/chris/Desktop/Bruce_raw/build-host-deps/root/usr/lib/x86_64-linux-gnu \
  cmake --build build
env LD_LIBRARY_PATH=/home/chris/Desktop/Bruce_raw/build-host-deps/root/usr/lib/x86_64-linux-gnu \
  ctest --test-dir build --output-on-failure
env LD_LIBRARY_PATH=/home/chris/Desktop/Bruce_raw/build-host-deps/root/usr/lib/x86_64-linux-gnu \
  python3 docs/cli/verify_examples.py build/bruce
```

Der neue Prüfer kann separat laufen:

```bash
env LD_LIBRARY_PATH=/home/chris/Desktop/Bruce_raw/build-host-deps/root/usr/lib/x86_64-linux-gnu \
  ctest --test-dir build --output-on-failure -R '^control_contract_matrix$'
```

## Verifikationsbefund

Am 28. September 2026 wurde der lokale Release-Build mit dem oben genannten
Dependency-Bundle erneut aufgebaut. Die vollständige CTest-Suite bestand mit
**128/128** Tests in **200,02 Sekunden**. Die neuen Tests
`control_contract_matrix` und `control_contract_matrix_unit` waren enthalten
und bestanden. Der separate
CLI-Dokumentationsprüfer bestätigte **135** Beispiele, **77** dokumentierte
Optionen, **458** Diagnosen und gültige Links.

Die Prüfung ist lokal. Historische oder Remote-Nachweise wurden nicht als
Ersatz verwendet.

## Bewusst offen

`experience inspect` besitzt weiterhin keinen Transfer- oder
Sitzungscontroller. Ein möglicher Adapter für diesen oder einen anderen neuen
Verwaltungsweg, seine Budgeteinheit und seine Abbruchreihenfolge sind ein
separates Entwurfs- und Implementierungspaket. Die Matrix ändert deshalb
keine bestehenden Verwaltungsaufrufe.
