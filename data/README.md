# Data

Not committed. To recreate:

1. From the Case Western Reserve University Bearing Data Center download page,
   save these drive-end recordings into `data/cwru/` as `<number>.mat`:
   - Normal baseline: 97, 98, 99, 100 (motor load 0-3 hp, recorded at 48 kHz)
   - 12 kHz drive-end, 0.007" faults: inner race 105-108, ball 118-121,
     outer race (6 o'clock) 130-133
2. Run `python3 tools/prepare_cwru.py`. It writes `data/prepared/`.
