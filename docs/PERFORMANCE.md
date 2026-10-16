# Performance on the STM32G474 (170 MHz)

All numbers measured on the board with the DWT cycle counter, averaged over 16
frames from the 3 hp load, Release build (`-Os`). Features and classification
checked against the PC each time (16/16 correct, features within a few % of
the tolerance).

## Feature extraction, per frame (budget: 85,333 us)

| Change | Cycles | Time | CPU load | Notes |
|---|---|---|---|---|
| Baseline: `cosf`/`sinf` per sample, FIR taps in flash | 2,438,145 | 14.3 ms | 16.8% | |
| Lookup tables for Hann windows + FFT twiddles | 1,300,557 | 7.65 ms | 9.0% | FFT 679k -> 244k cycles: ~2/3 of the FFT was trig |
| FIR taps in RAM + folded symmetric FIR | TBD | TBD | TBD | |

Classification (logistic regression, 48 numbers): ~1,060 cycles (6 us).

## Measured facts

- One `cosf` costs about 175 cycles on this chip (Hann window stage: 178,566
  cycles for 1,024 calls, before tables).

## FIR microbenchmark (65 taps x 960 outputs)

| Version | Cycles | Speedup | Cycles per tap |
|---|---|---|---|
| Taps in flash | 774,916 | 1.0x | 12.4 |
| Taps in RAM | 576,144 | 1.34x | 9.2 |
| Taps in RAM + folded (33 multiplies instead of 65) | 382,164 | 2.03x | 6.1 |

- RAM vs flash: the loop rereads 260 bytes of taps per output; at 170 MHz
  flash needs wait states and the ART cache is small.
- Folding gives 1.51x, not 2x: each step still does two loads, an add and a
  multiply-add, plus loop overhead. Loads and loop overhead dominate, not
  multiplies.
