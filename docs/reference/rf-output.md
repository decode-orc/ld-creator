# RF Output

!!! warning "Planned"
    ld-creator does not write RF yet. This page describes the intended output.

## File formats

ld-creator writes formats that ld-decode reads directly:

| Extension | Format | Notes |
|-----------|--------|-------|
| `.s16` | Signed 16-bit little-endian samples, mono | First format to be implemented |
| `.lds` | 10-bit packed samples | Capture format used by the Domesday Duplicator |
| `.ldf` | FLAC-compressed `.lds` | Much smaller; ld-decode decompresses on the fly |

The default sample rate is **40 MHz**, which is what ld-decode expects unless told otherwise with `--inputfreq`.

## Modulation parameters

The RF follows the LaserVision standards:

| Parameter | PAL (IEC 60856) | NTSC (IEC 60857) |
|-----------|-----------------|------------------|
| Video carrier at blanking | 7.1 MHz (§9.2.2) | 8.1 MHz (§9.2.2) |
| Video deviation | 800 kHz, blanking to white (§9.2.3) | 1.7 MHz, sync tip to white (§9.2.3) |
| Audio channel I | 683.59375 kHz (§8) | 2.301136 MHz (§8) |
| Audio channel II | 1.06640625 MHz (§8) | 2.812499 MHz (§8) |
| Audio deviation | ±100 kHz at 100 % (§8.1) | ±100 kHz at 100 % (§8.1) |
| Audio subcarrier level | −26 dB re main carrier (§7.2.2) | −26 dB re main carrier (§7.2.2) |
| EFM low-pass | −3 dB at 1.75 MHz (Amendment 2 §13.1.2) | −3 dB at 1.75 MHz (Amendment 2 §13.1.2) |
