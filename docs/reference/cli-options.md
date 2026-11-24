# Command-line Options

```bash
ld-creator --project <path> [options]
```

| Option | Value | Default | Description |
|--------|-------|---------|-------------|
| `--project` | path | — | The [project file](project-file.md). Required unless `--help` or `--version` is given |
| `--validate` | — | — | Check the project and its input, then exit. Nothing is written |
| `--output-root` | directory | the project file's directory | The directory that `{output}` stands for in project paths. A relative directory is relative to the project file's directory |
| `--threads` | `auto` or 1–4096 | `auto` | Worker threads; `auto` uses one per hardware thread. The output is identical for every value |
| `--log-level` | `info`, `debug` or `trace` | `info` | Logging verbosity. `debug` adds configuration and resolved paths; `trace` adds per-field and per-line detail |
| `--log-file` | path | none | Also write log output to this file. The file is overwritten |
| `-h`, `--help` | — | — | Print usage and exit with status 0 |
| `--version` | — | — | Print the build version (git commit hash) and exit with status 0 |

No option changes the RF: everything that does is in the project file.

Logging goes to standard error. A missing `--project`, an unrecognised option, a missing option value, an unknown log level or an invalid thread count is an error: ld-creator prints the problem and the usage text and exits with a non-zero status.

!!! warning "Partly implemented"
    Without `--validate`, ld-creator checks the project and its input and then runs it. Only projects with `output.mode: carrier_diagnostic` can be run; for a disc project ld-creator reports that disc modulation is not implemented and exits with a non-zero status.
