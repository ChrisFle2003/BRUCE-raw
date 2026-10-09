# Commands at a glance

This is an English adaptation of selected sections of the command reference.
It covers the first research workflow, exact calculation, Grid, and runtime data
mods. The source reference contains the exhaustive option and planner catalog.

## Help and contracts

| Command | Purpose |
| --- | --- |
| `bruce --help` | Show the complete text help. Subcommands do not have their own `--help`. |
| `bruce --version` | Return the version and the `bruce-version-v1` contract. |
| `bruce contracts` | List research strategies and their supported planners. |
| `bruce task-contracts` | List the eight application types and architecture boundaries. |
| `bruce gate-contracts` | Show the versioned research gate contract, without assessing a run. |

Each option takes a separate value. Unknown or repeated options are errors.
Use `--min -3`, not `--min=-3`. Quote shell metacharacters and paths with spaces.

## Research with training and holdout

Run from the repository root after building `bruce`. The output path must be new.

```sh
./build/bruce research \
  --train examples/affine-training.csv \
  --holdout examples/affine-holdout.csv \
  --min -3 --max 3 \
  --verification replay \
  --output affine.bruce
./build/bruce replay affine.bruce
./build/bruce inspect-run affine.bruce
```

The CSV files contain training and holdout pairs for `y = 2*x + 3`.
Training determines candidates; holdout checks additional, disjoint input values.
Successful sample checks do not establish a general formula proof.

| Option | Default | Scope |
| --- | --- | --- |
| `--strategy` | `affine` | Choose an implemented mathematical family. |
| `--domain` | `int64` for CSV affine research | Exact paths support `bigint` or `rational` according to the selected family. |
| `--min`, `--max` | `-8`, `8` for affine research | Inclusive coefficient bounds, not sample input bounds. |
| `--max-candidates` | `100000` | At most `1000000` started candidates. |
| `--max-operations` | `1000000` | At most `10000000` strategy-specific logical operations. |
| `--max-trace` | `100002` | Between `2` and `100002` trace events. |
| `--output` | None | Write a new replay bundle. |

These defaults do not apply universally to every family. Expression families,
exact arithmetic, and portfolios have their own constraints. Consult the
original command reference before changing the family or planner.

## Exact calculation

```sh
./build/bruce calculate --domain rational --op add --left 1/3 --right 1/6
```

`calculate` accepts `bigint` (the default) or `rational`. Operations are `add`,
`sub`, `mul`, `div`, `divmod`, `pow`, and `modpow`, subject to domain restrictions.
The default `--max-bits` is `4096`, with a range of `1..16384`.

BigInt division requires a zero remainder. `divmod` and `modpow` are BigInt-only;
`modpow` requires a positive integer `--modulus`. Powers use nonnegative UInt64
exponents. This command has no `--output` or replay/session options.

## Grid and resume

```sh
./build/bruce grid --path '2,6/4,3'
./build/bruce grid-batch --paths '2,6;4,3;2,6' \
  --max-query-steps 1 --verification replay --output partial.bruce
./build/bruce grid-resume --from-run partial.bruce \
  --additional-queries 2 --verification replay --output complete.bruce
```

Grid evaluates only the final coordinate pair as `x^y`. The path `2,6/4,3`
therefore evaluates `4^3`, which is `64`. It is an address path, not a power tower.
Each coordinate is `0..7`, with `1..64` pairs per path. Batch queries accept
`1..1024` paths. Resume reconstructs a replayable incomplete batch and processes
additional queries; it does not restore a memory snapshot.

## Signed data mods

| Command | Effect |
| --- | --- |
| `bruce mods verify PACKAGE --trust-store PATH` | Check package format, hash, Ed25519 trust, and schema. |
| `bruce mods install PACKAGE --mod-root PATH --trust-store PATH` | Install into an explicit private registry, initially inactive. |
| `bruce mods list --mod-root PATH` | Inspect package versions and registry state. |
| `bruce mods activate MOD_ID VERSION --mod-root PATH` | Recheck signature, mount conflicts, and golden cases before activation. |
| `bruce mods disable MOD_ID --mod-root PATH` | Deactivate a version while retaining files. |
| `bruce route --text TEXT --mod-root PATH --output REPORT` | Route text against the active Tree and export a new report. |
| `bruce route replay REPORT --mod-root PATH` | Recheck a report against the same active Tree and package identities. |

V1 packages contain declarative routing data, not executable workers. `route`
selects a template; it does not perform the selected calculation. There are no
implicit trusted keys or installation paths. `UNKNOWN` and `AMBIGUOUS` are valid
results with exit code `0`.

## Continue reading

Read the [CLI manual](README.md), [input formats](formats.md), and
[result semantics](results-and-control.md). The optional Language and NanoCell
mods have separate command and replay contracts in the original reference.
