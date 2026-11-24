# Overview

ld-creator turns a CVBS recording into LaserDisc RF in three parallel paths that are combined into one signal, following IEC 60856 (PAL) and IEC 60857 (NTSC):

| Path | Input | What happens | Status |
|------|-------|--------------|--------|
| Video | `.cvbs` + `.meta` | Group-delay pre-distorted, pre-emphasised and frequency-modulated onto the main carrier (IEC 60857 §9.1.7, §9.2) | NTSC carrier implemented ([carrier diagnostic](carrier-diagnostic.md)); disc stages planned |
| Analogue audio | `_audio_<pair>.wav` | Two FM subcarriers, pulse-width modulated onto the main carrier (§7.2, §8) | Planned |
| EFM digital audio | `.efm` + `.efm.meta` | Filtered EFM signal, pulse-width modulated onto the main carrier (Amendment 2 §13) | Planned |

The result is written as an RF sample file that ld-decode can read directly. See [RF output](../reference/rf-output.md).

!!! note
    ld-creator does not add noise, dropouts or other disc defects. Generate those with videosynth so that the original signal — the reference you compare against — already contains them.
