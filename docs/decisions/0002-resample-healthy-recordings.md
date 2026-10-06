# 0002: Convert the healthy recordings from 48 kHz to 12 kHz

**Status:** accepted

## Context

In the CWRU dataset the normal-baseline (healthy) recordings were sampled at
48 kHz, while the 12 kHz drive-end fault recordings were sampled at 12 kHz.

## Decision

Resample the healthy recordings to 12 kHz with a polyphase filter
(`scipy.signal.resample_poly`), which removes content above 6 kHz before
dropping samples so nothing aliases.

## Consequences

- Every recording has the same sample rate and frequency range.
- A classifier can no longer separate classes by sample rate alone, which
  would inflate accuracy without detecting faults.
