# Runtime data-mod V1 contract fixtures

**IMPLEMENTIERT UND BELEGT:** `runtime_package_contracts` verifies these bytes
against the codec and mounts the verified rational-power routing fragment.
The package only identifies a template; it does not supply or execute a solver.
`math-rational-power-ambiguous-v1.brucemod` uses the same published test key
and contains two matching child nodes. Its golden case requires `AMBIGUOUS`;
the real CLI integration verifies that no template is selected and replay agrees.

## Test keys (public and deliberately non-secret)

`test-truststore-v1.pub` contains the public RFC 8032 Ed25519 test-vector key.
The matching **published test-only seed** is in `runtime_package_tests.cpp` so
tests can sign malformed schema vectors. Anybody can reproduce these signatures.
Never trust this key in a production installation. The runtime loads no fixture
key automatically and has no implicit trust-store defaults.

## Canonical container

All lengths count bytes, represented by unsigned decimal integers without
leading zeros. A field is `length:value`. After the ASCII header and newline,
exactly four name/content field pairs appear in this order:

1. `manifest.json`
2. `tree.json`
3. `tests/golden-cases.json`
4. `signature.ed25519`

One final newline follows the signature field. No other members, trailing bytes,
executables, archives or worker files are supported. The entire container is at
most 1 MiB. The signature is exactly 128 lowercase hexadecimal characters.
Ed25519 signs the header and the first three member pairs, excluding the
signature member and final newline. `payload_sha256` hashes the complete encoded
tree and golden-case member pairs; `package_digest()` hashes every package byte.
Changing an unsigned member invalidates both its relevant digest and signature.

JSON is UTF-8, without whitespace outside strings. Object keys are sorted
lexicographically. The supported values are objects, arrays, strings and uint32
integers; only quote/backslash escapes and canonical lowercase `\u00xx` control
escapes are supported. Duplicate keys, extra fields, invalid UTF-8, alternate
number spellings, nesting over 32 levels, over 8192 values, and strings over
65536 bytes are rejected.

## Strict schemas

The manifest requires exactly: `capabilities`, `host_abi_version`, `mod_id`,
`owner_scope`, `payload_sha256`, `schema_version`, `signing_key_id`,
`token_normalization_version`, `tree_schema_version`, `version`.
All four version integers equal 1. `owner_scope` is `user`; capabilities are
exactly `["dataset.read","tree.extend"]` in canonical order. `mod_id` must not
be `core`. IDs/version strings have 1–128 ASCII letters, digits, dots, hyphens
or underscores and cannot start with a dot. A payload digest has 64 lowercase
hexadecimal characters. This V1 is a declarative tree routing schema; executable
logic and standalone dataset/template file members are outside this codec.

The tree object has exactly `mount` and `nodes`. Each node has exactly `aliases`,
`claim_scope`, `id`, `key`, `route_pattern`, `template_id`. Addresses have 1–32
positive uint32 segments, except the logical Core mount `[0]`. Ownership comes
from the verified mod ID, never from the node. Claim scopes use the eight host
enum names, without conferring execution authority. Alias and pattern arrays
contain nonempty strings of at most 4096 bytes. Structural nodes may have an
empty template/pattern. Mount/ancestry/conflict validation belongs to the registry.

Golden cases are a nonempty array of objects with exactly `input`, `status`,
`template_id`. Inputs have at most 4096 bytes. `MATCHED` needs a nonempty template
ID; `UNKNOWN` and `AMBIGUOUS` require an empty template ID. Installation must
execute the cases against its candidate tree before activation.

Trust-store content starts with `BRUCE_RUNTIME_MOD_TRUSTSTORE_V1` and newline;
each subsequent line is `key-id:64-lowercase-hex-public-key` plus newline. IDs
are strictly increasing and unique. Empty explicit trust stores are valid and
trust no packages. At most 65536 bytes are accepted. The parser accepts content,
not a path; the caller is responsible for bounded file reading.

## Verification order

`unpack_runtime_data_mod_package_v1` bounds the envelope before allocations,
validates canonical member names/lengths, parses only the manifest, then verifies
the tree/golden payload digest. Tree bytes remain opaque. Signature verification
against the explicit trust store precedes parsing the tree and golden cases.
The immutable package exposes `tree_fragment(truststore)` and
`golden_cases(truststore)` only through the same verification gate. Candidate
mount checks and execution of golden cases are a separate registry responsibility.
