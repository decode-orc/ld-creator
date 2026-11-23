# Command-line Options

| Option | Value | Default | Description |
|--------|-------|---------|-------------|
| `-h`, `--help` | — | — | Print usage and exit with status 0 |
| `--version` | — | — | Print the build version (git commit hash) and exit with status 0 |
| `--log-level` | `info`, `debug` or `trace` | `info` | Logging verbosity. `debug` adds configuration and resolved paths; `trace` adds per-field and per-line detail |
| `--log-file` | path | none | Also write log output to this file. The file is overwritten |

Logging goes to standard error. Any unrecognised option, a missing option value or an unknown log level is an error: ld-creator prints the problem and the usage text and exits with a non-zero status.

!!! warning "Planned"
    Input, output, output-format and sample-rate options will be documented here when they are implemented.
