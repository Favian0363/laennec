#!/usr/bin/env python3
"""Prepare the CWRU bearing recordings for training and for the board.

Reads 16 drive-end recordings from data/cwru/<number>.mat, converts the
healthy recordings from 48 kHz to 12 kHz so every recording has the same
sample rate, and writes:
    data/prepared/<number>.f32   raw little-endian float32 samples (easy to read from C)
    data/prepared/manifest.csv   one row per recording: condition, load, rpm, length...

Why the conversion matters: the healthy ("normal baseline") recordings were
sampled at 48 kHz, the fault recordings at 12 kHz. Mixing them unconverted lets
a classifier tell them apart by sample rate instead of by bearing damage.

Usage:  python3 tools/prepare_cwru.py
"""
import csv
import math
import os
import sys

import numpy as np
from scipy.io import loadmat
from scipy.signal import resample_poly

TARGET_FS = 12000
RAW_DIR = "data/cwru"
OUT_DIR = "data/prepared"

# file number -> (condition, motor load in hp, sample rate of the original recording)
RECORDINGS = {
    97: ("normal", 0, 48000), 98: ("normal", 1, 48000), 99: ("normal", 2, 48000), 100: ("normal", 3, 48000),
    105: ("inner", 0, 12000), 106: ("inner", 1, 12000), 107: ("inner", 2, 12000), 108: ("inner", 3, 12000),
    118: ("ball", 0, 12000), 119: ("ball", 1, 12000), 120: ("ball", 2, 12000), 121: ("ball", 3, 12000),
    130: ("outer", 0, 12000), 131: ("outer", 1, 12000), 132: ("outer", 2, 12000), 133: ("outer", 3, 12000),
}


def drive_end_signal(mat, number):
    """The drive-end accelerometer signal. Some CWRU files hold more than one
    recording, so prefer the one whose name matches the file number."""
    keys = [k for k in mat if k.endswith("_DE_time")]
    preferred = f"X{number:03d}_DE_time"
    if preferred in keys:
        return np.asarray(mat[preferred], dtype=np.float64).ravel(), preferred
    if len(keys) == 1:
        return np.asarray(mat[keys[0]], dtype=np.float64).ravel(), keys[0]
    sys.exit(f"{number}.mat: can't tell which drive-end signal to use (found {keys})")


def recorded_rpm(mat):
    for k in mat:
        if k.endswith("RPM"):
            return float(np.asarray(mat[k]).ravel()[0])
    return math.nan  # a few files don't store it


def main():
    missing = [n for n in RECORDINGS if not os.path.exists(f"{RAW_DIR}/{n}.mat")]
    if missing:
        sys.exit(f"missing in {RAW_DIR}/: " + ", ".join(f"{n}.mat" for n in missing))
    os.makedirs(OUT_DIR, exist_ok=True)

    rows = []
    for number, (condition, load_hp, native_fs) in RECORDINGS.items():
        mat = loadmat(f"{RAW_DIR}/{number}.mat")
        x, key = drive_end_signal(mat, number)
        if native_fs != TARGET_FS:
            factor = native_fs // TARGET_FS
            # resample_poly low-pass filters before dropping samples, so
            # frequencies above the new 6 kHz limit can't fold back (alias).
            x = resample_poly(x, up=1, down=factor)
        path = f"{OUT_DIR}/{number}.f32"
        x.astype("<f4").tofile(path)
        rows.append({
            "file": number, "condition": condition, "load_hp": load_hp,
            "rpm": recorded_rpm(mat), "native_fs": native_fs, "fs": TARGET_FS,
            "samples": len(x), "seconds": round(len(x) / TARGET_FS, 2),
            "rms_g": round(float(np.sqrt(np.mean(x ** 2))), 4), "signal": key, "path": path,
        })

    with open(f"{OUT_DIR}/manifest.csv", "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)

    print(f"{'file':>5} {'condition':>9} {'load':>4} {'rpm':>6} {'native':>7} {'seconds':>7} {'rms (g)':>8}  signal")
    for r in rows:
        print(f"{r['file']:>5} {r['condition']:>9} {r['load_hp']:>3}hp {r['rpm']:>6.0f} {r['native_fs']:>7} "
              f"{r['seconds']:>7} {r['rms_g']:>8}  {r['signal']}")
    print(f"\nwrote {len(rows)} recordings and {OUT_DIR}/manifest.csv")


if __name__ == "__main__":
    main()
