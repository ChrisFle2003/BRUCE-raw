#!/usr/bin/env bash
# Aufruf vom Projektstamm: bash docs/cli/tutorial.sh ./build/bruce
set -euo pipefail
BRUCE_BIN=$(realpath "${1:-./build/bruce}")
BRUCE_DEMO=$(mktemp -d "${TMPDIR:-/tmp}/bruce-cli-tutorial-XXXXXX")
cd "$BRUCE_DEMO"

# Eigene Eingaben: y=2*x+3, mit disjunktem Holdout.
printf 'x,y\n0,3\n1,5\n4,11\n' > train.csv
printf 'x,y\n2,7\n10,23\n' > hold.csv
"$BRUCE_BIN" --version > version.json
"$BRUCE_BIN" contracts > contracts.json
"$BRUCE_BIN" task-contracts > task-contracts.json
"$BRUCE_BIN" coordinate --x 2 --y 6 > coordinate.json
"$BRUCE_BIN" coordinate --z 42 > unpair.json
"$BRUCE_BIN" calculate --domain rational --op add --left 1/3 --right 1/6 > calculate.json
"$BRUCE_BIN" research --train train.csv --holdout hold.csv --min -3 --max 3 \
  --verification replay --output affine.bruce > affine.json
"$BRUCE_BIN" replay affine.bruce > affine-replay.json
"$BRUCE_BIN" inspect-run affine.bruce > affine-inspect.json

# Gleiche gekürzte Formel, aber verschiedene Definitionsbereiche.
"$BRUCE_BIN" compare --left-numerator '-1,0,1' --left-denominator '-1,1' \
  --right-numerator '1,1' --right-denominator 1 --verification replay \
  --output comparison.bruce > comparison.json

"$BRUCE_BIN" grid --path '2,6/4,3' --output leaf.bruce > grid.json
"$BRUCE_BIN" grid-batch --paths '2,6;4,3;2,6' --max-query-steps 1 \
  --verification replay --output partial.bruce > partial.json
"$BRUCE_BIN" grid-resume --from-run partial.bruce --additional-queries 2 \
  --verification replay --output complete.bruce > complete.json

# Ein fester Grid-Schnitt mit y=2 liefert Daten für x².
"$BRUCE_BIN" grid-batch --paths '0,2;1,2;2,2' --output grid-train.bruce > grid-train.json
"$BRUCE_BIN" grid-batch --paths '3,2' --output grid-hold.bruce > grid-hold.json
"$BRUCE_BIN" research --grid-train grid-train.bruce --grid-holdout grid-hold.bruce \
  --strategy polynomial --max-degree 2 --min -2 --max 2 \
  --verification replay --output grid-research.bruce > grid-research.json

printf 'BRUCE_RULES_V1\nsnapshot demo\nrule good positive value eq 64 0 10 manual -\nrule bad reject value lt 0 0 0 manual -\n' > demo.rules
printf 'id,domain,layer,x,y,value\n1,demo,0,2,6,64\n2,demo,0,4,3,64\n3,demo,0,1,1,-1\n' > candidates.csv
"$BRUCE_BIN" compatibility --rules demo.rules --candidates candidates.csv \
  --verification replay --output selection.bruce > selection.json

# Ein unabhängiges Portfolio als Quelle, danach eine korrigierte Holdout-Prüfung.
printf 'x,y\n0,0\n1,1\n2,4\n' > square.csv
printf 'x,y\n3,9\n' > square-hold.csv
printf 'x,y\n4,16\n' > square-new-hold.csv
for BRUCE_HOLD in square-hold square-new-hold; do
  "$BRUCE_BIN" research --strategy portfolio-rational --train square.csv \
    --holdout "$BRUCE_HOLD.csv" --max-degree 2 --max-power 2 --min -2 --max 2 \
    --max-denominator 1 --output "$BRUCE_HOLD.bruce" > "$BRUCE_HOLD.json"
done
"$BRUCE_BIN" experience add --from-run square-hold.bruce --output first.experience > first.json
BRUCE_RECORD=$(python3 -c 'import json; print(json.load(open("first.json"))["records"][0]["id"])')
"$BRUCE_BIN" experience replace --store first.experience --id "$BRUCE_RECORD" \
  --reason 'Zusätzlicher Holdout' --from-run square-new-hold.bruce \
  --output replaced.experience > replaced.json
"$BRUCE_BIN" experience inspect replaced.experience > experience-inspect.json
BRUCE_RECORD=$(python3 -c 'import json; print(json.load(open("replaced.json"))["records"][-1]["id"])')
"$BRUCE_BIN" experience revoke --store replaced.experience --id "$BRUCE_RECORD" \
  --reason 'Demonstration des Widerrufs' --output revoked.experience > revoked.json

# Absichtlicher Sitzungsfehler: keine Ergebnisdatei darf entstehen.
set +e
"$BRUCE_BIN" grid-batch --paths '2,6' --verification replay \
  --max-session-checkpoints 0 --output must-not-exist.bruce > failure.stdout 2> failure.json
BRUCE_EXIT=$?
set -e
test "$BRUCE_EXIT" -eq 2
test ! -e must-not-exist.bruce
python3 - <<'PY'
import json
def load(name):
    with open(name + '.json') as stream:
        return json.load(stream)
assert load('calculate')['value'] == '1/2'
assert load('affine')['execution']['replay_verified']
assert load('affine')['result']['status'] == 'VERIFIED_ON_SAMPLES'
assert load('affine')['result'] == load('affine-replay')
assert load('comparison')['result']['status'] == 'PROVEN_DIFFERENT'
assert load('grid')['value'] == '64'
assert load('partial')['result']['status'] == 'INCOMPLETE'
assert load('complete')['result']['status'] == 'COMPLETED'
assert load('selection')['result']['status'] == 'AMBIGUOUS'
assert load('replaced') == load('experience-inspect')
assert load('revoked')['active_records'] == 0
assert load('failure')['execution']['failed_at'] == 'EXECUTING'
PY
printf 'Tutorial erfolgreich. Eingaben und Ergebnisse: %s\n' "$BRUCE_DEMO"
