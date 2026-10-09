# Understand a result

This English adaptation presents the common result statuses and control
boundaries from the source reference. Specialized evidence and mod result
fields remain documented in the original.

## Exit codes and mathematical claims

Exit code `0` means domain processing succeeded. It does not imply a completed
search or a proof. Parser, file, and control errors use JSON on stderr and exit
code `2`. Read the overall status, `stop_reason`, and individual findings together.
Individual `hypotheses[].status` values can differ from the overall result.

| Status | Meaning |
| --- | --- |
| `CANDIDATE` | A fitting training hypothesis without additional holdout confirmation. |
| `VERIFIED_ON_SAMPLES` | The supplied training and holdout pairs passed; no general formula proof. |
| `AMBIGUOUS` | Multiple equally ranked domain results. Stable ordering is not a decision. |
| `REFUTED` | Holdout refuted the training hypotheses. |
| `NO_CANDIDATE` | No fitting candidate in the completed declared search space. |
| `INCOMPLETE` | Processing stopped early, for example because of a budget or signal. |
| `SELECTED` | Compatibility selected one eligible candidate. |
| `NO_COMPATIBLE_CANDIDATE` | Complete rule evaluation admits no candidate. |
| `EVALUATED` | One Grid path was evaluated. |
| `STOPPED` | A Grid sentinel ended a branch. |
| `COMPLETED` | A Grid batch finished; intentionally stopped leaves can be included. |
| `PROVEN_EQUIVALENT` | The supplied rational functions have the same values and rational domain. |
| `PROVEN_DIFFERENT` | The comparison proves a difference in values or domains. |
| `UNDECIDED` | The bounded comparison methods did not decide. |

## Replay preserves the finding

A successful replay reproduces the original result. An `INCOMPLETE` run remains
`INCOMPLETE`. A digest, score, candidate, or sample verification is not a general
mathematical proof. The evidence chain does not open learning or publication
gates, activate rules, or promote a strategy.

## Budgets have distinct units

Research candidate and operation budgets bound the selected search. Session
checkpoints, transfer checkpoints, trace limits, and byte limits control different
parts of the workflow. They are not interchangeable units or universal time and
memory limits. Every command invocation starts its own control account.

`research`, `grid`, `grid-batch`, `grid-resume`, `compare`, and `compatibility`
support shared session options such as `--verification replay`. Options are
command-specific; do not assume they apply to calculation or mod commands.

Signals are observed at implemented checkpoints. There is no guaranteed maximum
cancellation latency. File-system calls and individual domain operations can
still block.

## Keep outputs inspectable

Most successful commands return one JSON object and a final newline. Help is
plain text; the rule approval/activation dialogue is interactive. When using a
JSON formatter in a shell pipeline, enable `set -o pipefail` so a successful
formatter does not hide a failed CLI command.

See the [control and publication contract](../contracts/06-control-and-publication.md)
and [local-file rules](local-files.md) for the complete control boundary.
