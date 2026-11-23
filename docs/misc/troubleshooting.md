# Troubleshooting

## "In-source builds are not supported"

CMake was pointed at the source directory as its build directory. Delete the `CMakeCache.txt` file and `CMakeFiles/` directory it created in the source tree, then configure into a separate directory:

```bash
nix develop "path:$PWD" --command cmake -S . -B build -G Ninja
```

## "unknown option" or "unknown log level"

ld-creator rejects options it does not recognise. Run `ld-creator --help` for the supported list; see [Command-line options](../reference/cli-options.md).

## "RF modulation is not implemented yet"

Expected at this stage of development: only `--help` and `--version` do anything useful so far.

## "cannot open log output"

The path given to `--log-file` could not be created. Check that the directory exists and is writable.
