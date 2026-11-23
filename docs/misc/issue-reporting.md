# Issue Reporting

Please report problems and suggestions on the [ld-creator issue tracker](https://github.com/decode-orc/ld-creator/issues){target="_blank"}.

A helpful report includes:

- The output of `ld-creator --version`.
- The exact command line you ran.
- A log captured with `--log-level debug --log-file ld-creator.log`.
- Your operating system and how you built ld-creator.

If the problem shows up in a round trip, also give the videosynth project file you used and the videosynth and ld-decode versions (`videosynth --version`, `ld-decode --version`).

!!! note
    If ld-decode decodes ld-creator's RF incorrectly, first check whether ld-creator's output matches the LaserVision standard. If it does, the problem may belong on the [ld-decode issue tracker](https://github.com/happycube/ld-decode/issues){target="_blank"} instead.
