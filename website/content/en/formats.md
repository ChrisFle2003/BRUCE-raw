# Input files and numbers

This English adaptation covers numeric inputs and research CSVs from the source
format reference. The original also defines the byte layouts of evidence,
Language, NanoCell, experience, and other replay bundles.

## Numbers

CLI integers are decimal, without whitespace, a leading `+`, thousands
separators, hexadecimal prefixes, or exponent notation. Negative values are
accepted only by signed fields. UInt64 ranges from `0` to
`18446744073709551615`; Int64 from `-9223372036854775808` to
`9223372036854775807`. Address calculations may overflow below the input limit.

`bigint` accepts decimal integers. `rational` also accepts numerator/denominator
pairs such as `-3/4` or `6/-8`. Zero denominators are invalid. Fractions are
reduced and signs normalized; `2/2` is output as `1`. Use `1/2`, not `0.5`.
Bit limits apply to the original components before reduction too.

Large and exact values are generally JSON strings, while Int64 research
coefficients are JSON numbers. Consumers must not assume one numeric JSON type
or convert every value to floating point.

## Research CSV pairs

```csv
x,y
0,3
1,5
4,11
```

`x` is the input and `y` is the expected output. The CLI requires exactly the
header `x,y`, then two numeric fields per line. Quotes, comments, blank lines,
translated headers, and a byte-order mark are not accepted. LF and CRLF are
supported; one final line ending is allowed.

Each supplied file contains `1..4096` pairs and at most `1048576` bytes. If no
holdout exists, omit the option rather than supplying an empty file. Exact
research also limits the combined normalized sample text to `1048576` bytes.

All input `x` values must be unique within and across training and holdout files.
Exact rational values are normalized first: `1/2` and `2/4` are the same input.
Disjoint input values alone do not establish independent data provenance.

## Rational samples

```csv
x,y
0,1/2
1/2,3/4
1,1
```

The website's data viewer supports ordinary quoted CSV for presentation, but
that does not broaden the stricter research CSV format accepted by `bruce`.
Downloads preserve the original bytes.

## Bundles and JSON

For research commands, `--output` produces a replay bundle. Redirecting stdout
saves a JSON result instead; these are not interchangeable. For `route`, stdout
and `--output` contain the same replayable JSON report.

The exact-affine evidence chain preserves the original training and holdout CSV
bytes, embeds an exact native bundle, and binds the body with SHA-256. Digests
and replay check integrity and reproduction; they do not prove provenance or a
general mathematical formula.

See [commands](commands.md), [result semantics](results-and-control.md), and the
[local publication rules](local-files.md) before creating exports.
