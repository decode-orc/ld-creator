# Command Line

ld-creator is a command-line program:

```bash
ld-creator [options]
```

## Available today

```bash
ld-creator --help                 # usage
ld-creator --version              # build version (git commit hash)
ld-creator --log-level debug      # more detailed logging
ld-creator --log-file run.log     # also write the log to a file
```

The full list, with defaults and accepted values, is in [Command-line options](../reference/cli-options.md).

!!! warning "Planned"
    Options to select the input CVBS file, the output RF file and its format and sample rate will be added with the modulation pipeline.

## Exit status

| Status | Meaning |
|--------|---------|
| `0` | `--help` or `--version` completed |
| non-zero | Invalid arguments, a log file could not be opened, or (currently) a modulation run was requested |
