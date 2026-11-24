# Command Line

ld-creator is a command-line program. A run is described by a [project file](../reference/project-file.md); the command line only says where it is and how to run:

```bash
ld-creator --project disc.yaml                       # run the project
ld-creator --project disc.yaml --validate            # check it, write nothing
ld-creator --project disc.yaml --output-root build   # {output} = build/
ld-creator --project disc.yaml --threads 1           # single-threaded; same output
ld-creator --help
ld-creator --version
```

The full list, with defaults and accepted values, is in [Command-line options](../reference/cli-options.md).

## Checking a project

`--validate` loads the project and its input and runs every check:

- the project's structure, key names and types;
- each value's range and, where IEC 60857 sets one, its tolerance (unless listed in `allow_out_of_tolerance`);
- the input recording: NTSC, composite, 4fsc, whole frames;
- the frame ranges of the input and of each diagnostic tap;
- the radius at the end of the run against IEC 60857 §4.12 (145 mm);
- for a resolved project, the input files' hashes.

Every problem is reported, each naming its key path, for example:

```text
Project validation error: video_pre_emphasis.t1_ns = 340 is outside the IEC 60857 §9.2.4 tolerance 312–328 (list video_pre_emphasis.t1_ns in allow_out_of_tolerance to allow it)
```

On success ld-creator logs the input, the disc's radius range and the output path, and exits with status 0.

## Running a project

Without `--validate`, ld-creator runs the project after the same checks. It writes the RF to `output.rf_path`, every declared diagnostic tap, and the [resolved project](../reference/project-file.md#the-resolved-project) beside the RF, then logs the number of samples written.

`--threads` sets how many threads the stages share; the RF is byte-identical for any value.

## Exit status

| Status | Meaning |
|--------|---------|
| `0` | `--help` or `--version` completed, `--validate` found no errors, or a run completed |
| non-zero | Invalid arguments, a log file could not be opened, a project or input error, or a read or write failure during the run |
