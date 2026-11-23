# S14 — RF sample writer (`rf_sample_writer`)

## Physical process

The capture is written to a file that ld-decode reads.

## Sources

- ld-decode's external interface: it reads `.s16` (signed 16-bit
  little-endian, mono, no header) at 40 MHz by default (`-f` to change).
- DomesdayDuplicator `capture-files.md`: a DdD capture is 16-bit "carrying the
  converter's 10-bit codes centred about zero"; its uncompressed form,
  `.ddd.s16`, is signed 16-bit little-endian mono with no header.
- DomesdayDuplicator `ddd-gui/tests/unit/test_sample_format.cpp`: the 16-bit
  value of 10-bit code *c* is `(c − 512) × 64`.

## Model and equations

| Profile | Stored word |
|---------|-------------|
| `domesday_duplicator` | `w = (c − 512) × 64`, as a real DdD `.ddd.s16` holds |
| `ideal` | `w` = the signed 16-bit code from S13 |

So a `domesday_duplicator` file is byte-for-byte in the format of an
uncompressed DdD capture.

Alongside the RF, S14 writes the **resolved project**: the effective
configuration with every key, as described in
[project-file-schema.md](project-file-schema.md#the-resolved-project).

## Parameters

None beyond the output paths.

## Assumptions and approximations

None: the mapping is exact.

## Input/output contract

- **In:** D10. **Out:** the `.s16` file named by `output.rf_path`, and the
  resolved project beside it.

## Project YAML block

`output` (see [project-file-schema.md](project-file-schema.md#output)):
`rf_path` and `mode`.

## Ideal setting

Not applicable.

## Verification

Unit tests: codes 0, 512 and 1023 map to −32768, 0 and 32704. Functional
test: the file length is 2 bytes × samples; re-running from the resolved
project gives the same bytes.

## Diagnostic tap

None: the output is the file.

## Open items

- Compressed `.ddd.flac` output (DdD's native format) is possible later; not
  needed for ld-decode.
