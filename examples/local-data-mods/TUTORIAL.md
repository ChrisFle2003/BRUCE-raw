# Tutorial: Den ersten lokalen BRUCE Data-Mod verwenden

Dieses Tutorial baut und aktiviert den vorhandenen Mathematik-Beispiel-Mod.
Dabei wird kein Programmcode aus dem Paket ausgeführt.

## 1. Projekt bauen

Im Projekt-Root:

```sh
cmake -G Ninja -S . -B build-cli -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build-cli --target bruce bruce-mod-builder -j2
```

## 2. Privaten Schlüssel vorbereiten

Die Datei `.env` liegt im Projekt-Root, wird nicht versioniert und enthält
genau eine Zeile:

```text
BRUCE_MOD_ED25519_SEED_HEX=<64-kleinbuchstabige-Hex-Zeichen>
```

Sie muss nur für dich lesbar sein:

```sh
chmod 600 .env
```

Den Inhalt niemals ausgeben, kopieren oder committen.

## 3. Privaten Arbeitsordner anlegen

```sh
umask 077
mkdir -p runs/local-data-mods
chmod 700 runs runs/local-data-mods
```

`runs/` ist ein lokaler, ignorierter Arbeitsbereich.

## 4. Paket bauen

```sh
./build-cli/bruce-mod-builder \
  --manifest examples/local-data-mods/math-rational-power/manifest.json \
  --tree examples/local-data-mods/math-rational-power/tree.json \
  --golden examples/local-data-mods/math-rational-power/golden-cases.json \
  --output runs/local-data-mods/math-rational-power-v1.brucemod \
  --trust-store-output runs/local-data-mods/local-math-v1.pub
```

Der Builder schreibt ein signiertes Paket und den passenden öffentlichen
Truststore. Vorhandene Zieldateien werden nicht überschrieben.

## 5. Prüfen und installieren

```sh
./build-cli/bruce mods verify runs/local-data-mods/math-rational-power-v1.brucemod \
  --trust-store runs/local-data-mods/local-math-v1.pub
./build-cli/bruce mods install runs/local-data-mods/math-rational-power-v1.brucemod \
  --mod-root runs/local-data-mods/mods \
  --trust-store runs/local-data-mods/local-math-v1.pub
./build-cli/bruce mods list --mod-root runs/local-data-mods/mods
```

Das Paket ist jetzt gespeichert, aber noch inaktiv. Daher ist die folgende
Route weiterhin `UNKNOWN`.

## 6. Aktivieren und routen

```sh
./build-cli/bruce mods activate user.local-math-rational-power 1.0.0 \
  --mod-root runs/local-data-mods/mods
./build-cli/bruce route --text '3/4 hoch 2' \
  --mod-root runs/local-data-mods/mods \
  --output runs/local-data-mods/power.route.json
```

Du erhältst `MATCHED`, die Adresse `[1,1,5]` und die Vorlage
`math.rational.power.v1`. Das ist eine Auswahl der Vorlage, nicht die
Berechnung von `9/16`.

## 7. Report erneut prüfen

```sh
./build-cli/bruce route replay runs/local-data-mods/power.route.json \
  --mod-root runs/local-data-mods/mods
```

Replay bestätigt nur genau denselben aktiven Tree und dieselben Paketversionen.

## 8. Wieder deaktivieren

```sh
./build-cli/bruce mods disable user.local-math-rational-power \
  --mod-root runs/local-data-mods/mods
```

Danach ist die Route wieder `UNKNOWN`; ein alter Report wird bewusst mit
`RouteRegistryDigestMismatch` abgelehnt.

Für das Datenformat, die Sicherheitsgrenzen und alle Befehle siehe
[README.md](README.md).
