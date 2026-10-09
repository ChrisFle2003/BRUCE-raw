# Local files and publication

This is an English adaptation of the local-file reference for Core 0.102.0.

## Explicit paths, explicit effects

New exports use a private temporary file in the same directory and an exclusive
`link` operation. Existing destinations are not overwritten. Parent directories
must already exist. File reads retain their existing byte limits; transfer
control uses the existing 8 KiB read and 64 KiB write checkpoints.

Active rule changes require concrete human confirmation in the CLI. The store is
locked, reloaded, and checked under that lock before state and journal are
replaced together. Direct `experience add`, `experience revoke`, and
`experience replace` also accept the existing transfer options.

## Failure before publication

When a callback cancels before publication, the temporary file is cleaned up.
Existing export targets are preserved. A write error before publication creates
no new destination. A rule-store failure before `rename` preserves the previous
store. A forcibly terminated process or a cleanup error can leave temporary
files; the rule store's `.lock` file remains a reusable lock path.

## Durability can be uncertain

> [!IMPORTANT]
> Exit code `2` does not always mean nothing was published. Inspect the named
> destination and its contents before retrying.

For a new export, the CLI syncs the temporary file, publishes it with `link`,
and removes the temporary name. If directory synchronization fails afterward,
`ExportCommitDurabilityUnknown` reports exit code `2`. The destination may
already contain the full bytes, while crash durability remains uncertain.
Retrying the same filename can fail because it exists; another filename can
create a duplicate.

`RuleStoreCommitDurabilityUnknown` is separate: it occurs after a successful
`rename` when the active rule store's directory synchronization fails. The new
state may already be visible.

## Confirmation boundaries

The file adapter does not contain a terminal dialogue; the CLI requests
confirmation first. Confirmation binds content, base state, and destination,
but does not authenticate a person against programs running as the same user.
There is no generic overwrite command, universal time/RAM limit, or guaranteed
maximum cancellation latency.

Read the [control and publication contract](../contracts/06-control-and-publication.md)
for the distinct session and transfer paths.
