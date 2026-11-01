# Facts

## Device profile measurements

Offline test of `DeviceProfileChain`, no audio device or host involved. Results match within
0.1 dB at 44100 and 48000 Hz.

Pink noise at -14 dBFS RMS, identical in both channels, after the `trimDb` values in
`DeviceProfile.h`: every profile lands within 0.08 dB of input RMS.

Gain in dB with a -40 dBFS sine at 48000 Hz, which stays below every compressor threshold. It
includes `trimDb`.

| Profile | 60 Hz | 200 Hz | 400 Hz | 1 kHz | 3 kHz | 10 kHz |
|---|---|---|---|---|---|---|
| Phone speaker | -58.7 | -16.9 | +4.3 | +7.8 | +12.2 | +7.1 |
| Laptop speakers | -38.2 | +0.6 | +3.7 | +4.3 | +7.1 | +3.6 |
| Stock earbuds | -4.7 | +0.2 | +0.6 | +0.9 | +5.6 | +0.8 |
| Car, left channel | +8.8 | +4.7 | +3.1 | +5.5 | +5.9 | +5.9 |

Flat output is bit-identical to its input. Mono check cancels an out-of-phase signal completely.
The Car right channel peaks 96 samples late at 48000 Hz, which is the intended 2 ms.
