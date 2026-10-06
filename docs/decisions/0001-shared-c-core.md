# 0001: One shared C core for the PC and the board

**Status:** accepted

## Context

The classifier is trained on the PC but runs on the STM32. If the features
are computed by different code in each place, the board can get different
inputs than the model was trained on, and nothing would warn us.

## Decision

All signal processing, features and the classifier live in `core/` as
portable C (no STM32 headers). The PC build compiles them into tests and a
feature tool that produces the training data; the firmware compiles the same
files. Results are compared between the two.

## Consequences

- Most logic can be tested on the PC, quickly, with sanitizers.
- `core/` must avoid anything chip-specific; hardware access stays in `firmware/`.
