# Architecture

```mermaid
flowchart LR
    A[CWRU recordings] --> B[prepare_cwru.py]
    B --> C[features tool on PC<br/>shared C core]
    C --> D[train classifier<br/>Python]
    D --> E[weights as C header]
    F[DAC playback] --> G[ADC 12 kHz<br/>timer + DMA]
    G --> H[features on chip<br/>same shared C core]
    E --> H
    H --> I[result over USB serial]
    H --> J[fault log in FRAM]
```

## Why one shared C core

Features are written once, in C. On the PC they produce the training data; on
the chip they run live. Training and the device cannot disagree.

## Data

The healthy recordings were sampled at 48 kHz and the fault recordings at
12 kHz. All recordings are converted to 12 kHz before anything else, so the
classifier cannot learn the sample rate instead of the fault.
