# Local Data-Mod Builder Design

## Goal

Provide a local-only builder that creates a canonical, signed BRUCE V1 data-mod
package from explicit tree and golden-case inputs. It is the authoring counterpart
to the existing runtime verifier; it never installs or activates a package.

## Key and trust boundary

The project-root `.env` is ignored by Git and stores only the private Ed25519
signing material for the local operator. It is read only by the builder, never
rendered, logged, copied into reports, or accepted as a CLI option. A separately
written public-key truststore is the only input accepted by runtime verification.
Existing key files and output packages are never overwritten.

## Builder contract

The builder receives explicit paths for a canonical declarative tree source,
golden cases, manifest metadata, output package and public truststore output.
It validates the same V1 schema/canonicalization constraints as the runtime,
computes the payload digest, emits the four-member canonical container, signs
the unsigned bytes with Ed25519, and writes the package atomically without
replacement. It writes the matching public key in the existing
`BRUCE_RUNTIME_MOD_TRUSTSTORE_V1` format.

## First example

Ship a small user-scoped mathematical routing package as an example: it mounts
only beneath the approved Core tree, has controlled route patterns and golden
cases, and grants only `dataset.read` and `tree.extend`. It adds no executable
code, worker, network, GPU or model capability.

## Failure behavior and verification

Missing/malformed `.env`, invalid source data, invalid output locations,
unwritable files and existing outputs fail closed without creating a package or
truststore. Tests prove deterministic output for the same fixed test key,
signature/runtime verification, no overwrite, secret-free diagnostics, and
successful install/activation/routing of the example package.
