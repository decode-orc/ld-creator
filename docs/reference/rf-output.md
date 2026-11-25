# RF Output

!!! warning "Partly implemented"
    The RF carries the NTSC video signal only. Audio, EFM and PAL are planned.

## File formats

ld-creator writes a format that ld-decode reads directly:

| Extension | Format | Status |
|-----------|--------|--------|
| `.s16` | Signed 16-bit little-endian samples, mono, no header | Written |
| `.lds` | 10-bit packed samples, the Domesday Duplicator's capture format | Planned |
| `.ldf` | FLAC-compressed `.lds` | Planned |

The sample rate is **40 MHz**, which is what ld-decode expects unless told otherwise with `--inputfreq`. Sample *n* is at *n* / 40 MHz from 0H of line 1 of the first frame read, and the file holds every sample before the end of the last frame: ⌈*F* × 4 004 000 / 3⌉ samples for *F* frames (1 334 667 for one frame).

### Sample words

The [`rf_capture.profile`](project-file.md#rf_capture) sets what a word holds:

| Profile | Converter code | Stored word |
|---------|----------------|-------------|
| `domesday_duplicator` | 10-bit, 0–1023, mid-scale 512 | (code − 512) × 64, as an uncompressed Domesday Duplicator capture (`.ddd.s16`) |
| `ideal` | signed 16-bit | the code itself |

The converter input *u* runs from −1 to +1 across its range and is quantised to round(512 + 512·*u*) or round(32768·*u*), clamped to the range. Clamped samples are counted, and the run warns if there are any.

## Disc RF

With `output.mode: disc` (the default), *u* is the capture device's input: the player RF after the capture profile's filters, scaled so that the whole run's peak-to-peak is `level_fraction` of full scale (75 % by default: 768 codes with the Domesday Duplicator profile), measured in a first pass over the run. The rest of the range is headroom, so the RF does not clip. See [Disc model](../user-manual/disc-model.md).

## Carrier-diagnostic RF

With `output.mode: carrier_diagnostic`, *u* = `level_fraction` × cos φ(*t*), where φ is the FM modulator's carrier phase. The carrier's peak-to-peak amplitude is therefore `level_fraction` of full scale (75 % by default: codes 128–896 with the Domesday Duplicator profile). No disc stage is applied: there is no readout loss, noise or capture filter. The file is **not a disc**.

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
