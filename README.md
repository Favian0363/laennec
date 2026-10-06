# laennec

**Real-time bearing-fault detection on STM32.**

laennec listens to machine vibration at 12,000 samples per second, analyzes each
85 ms chunk on chip, and classifies the bearing as healthy or as having an
inner-race, ball or outer-race fault. Its fault log survives sudden power loss.

> Status: needs benchmarking

## How it is built

- **core** (`core/`): signal processing, features and classifier in
  portable C. The same files run in the firmware and in PC tests.
- **Firmware** (`firmware/`): STM32G474, timer-triggered ADC
  with DMA, FFT on the chip, FMAC hardware filter, FRAM fault log.
- **Tools** (`tools/`): Python to prepare the dataset and train
  the classifier.

## Results

| Metric | Value |
|---|---|
| Accuracy on an unseen motor load | TBD |
| Decision latency per frame | TBD |
| CPU load | TBD |
| Power-cut recovery | TBD |

## Build

```bash
# PC: shared code + tests
cmake -S . -B build && cmake --build build -j && ctest --test-dir build

# Firmware (needs arm-none-eabi-gcc and ninja)
cd firmware && cmake --preset Debug && cmake --build --preset Debug
```

## Data

Case Western Reserve University Bearing Data Center. See [data/README.md](data/README.md).
