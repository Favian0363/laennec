#include "laennec/features.h"

#include "laennec/bandpass_coeffs.h"
#include "laennec/fft.h"

#include <math.h>

/* Optional profiling. on laptop, MARK() does nothing. 
 * firmware defines LAENNEC_PROFILING and provides function
 * which records cycle counter so each stage can be measured */
#ifdef LAENNEC_PROFILING
void laennec_profile_mark(int stage);
#define MARK(stage) laennec_profile_mark(stage)
#else
#define MARK(stage) ((void)0)
#endif

#define N LAENNEC_FRAME_LEN
#define HALF (N / 2)
#define BIN_HZ ((float)LAENNEC_SAMPLE_RATE_HZ / (float)N) /* 11.72 Hz per FFT bin */
#define PI_F 3.14159265358979f
#define TINY 1e-20f

/* Periodic Hann window: fades the frame's edges to zero so the FFT doesn't
 * see a jump where the end wraps around to the start (spectral leakage). */
static float hann(size_t i, size_t len) {
    return 0.5f - 0.5f * cosf(2.0f * PI_F * (float)i / (float)len);
}

static size_t bin_of(float hz) {
    return (size_t)(hz / BIN_HZ + 0.5f); /* nearest bin */
}

/* Sum of power in bins [first, last]. */
static float power_sum(const float* p, size_t first, size_t last) {
    float s = 0.0f;
    for (size_t k = first; k <= last; ++k) {
        s += p[k];
    }
    return s;
}

/* ---- Features of the raw signal (time domain) ---- */
static void time_features(const float* x, float out[LAENNEC_NUM_FEATURES]) {
    float s2 = 0.0f, s4 = 0.0f, peak = 0.0f;
    for (size_t i = 0; i < N; ++i) {
        const float x2 = x[i] * x[i];
        s2 += x2; // sum of squares -> RMS
        s4 += x2 * x2; // sum of 4th powers -> kurtosis
        if (fabsf(x[i]) > peak) { // -> crest
            peak = fabsf(x[i]);
        }
    }
    const float mean2 = s2 / (float)N;
    const float rms = sqrtf(mean2);
    out[LAENNEC_F_LOG_RMS] = log10f(rms + 1e-9f);
    out[LAENNEC_F_KURTOSIS] = mean2 > TINY ? (s4 / (float)N) / (mean2 * mean2) : 0.0f;
    out[LAENNEC_F_CREST] = rms > TINY ? peak / rms : 0.0f;
}

/* ---- Features of the spectrum ---- */
static void spectrum_features(const float* x, laennec_workspace* ws,
                              float out[LAENNEC_NUM_FEATURES]) {
    for (size_t i = 0; i < N; ++i) {
        ws->re[i] = x[i] * hann(i, N);
        ws->im[i] = 0.0f;
    }
    MARK(3);
    laennec_fft(ws->re, ws->im, N);
    MARK(4);

    /* Power per bin, bins 1..HALF (bin 0 is the average, already removed). */
    float* p = ws->work;
    for (size_t k = 1; k <= HALF; ++k) {
        p[k] = ws->re[k] * ws->re[k] + ws->im[k] * ws->im[k];
    }
    float total = 0.0f, weighted = 0.0f;
    for (size_t k = 1; k <= HALF; ++k) {
        total += p[k];
        weighted += p[k] * (float)k * BIN_HZ;
    }
    if (total <= TINY) {
        for (int f = LAENNEC_F_CENTROID_KHZ; f <= LAENNEC_F_BAND_4K_6K; ++f) out[f] = 0.0f;
        return;
    }
    out[LAENNEC_F_CENTROID_KHZ] = weighted / total / 1000.0f;
    /* Band edges: bin 85 = 996 Hz, 213 = 2496 Hz, 341 = 3996 Hz. */
    const size_t b1k = bin_of(1000.0f), b2k5 = bin_of(2500.0f), b4k = bin_of(4000.0f);
    out[LAENNEC_F_BAND_0_1K] = power_sum(p, 1, b1k - 1) / total;
    out[LAENNEC_F_BAND_1_2K5] = power_sum(p, b1k, b2k5 - 1) / total;
    out[LAENNEC_F_BAND_2K5_4K] = power_sum(p, b2k5, b4k - 1) / total;
    out[LAENNEC_F_BAND_4K_6K] = power_sum(p, b4k, HALF) / total;
}

/* ---- Envelope features ----
 * 1. Band-pass 2-5 kHz: keep the "ringing" that each impact causes.
 * 2. Take the absolute value: this turns "ringing that switches on at every
 *    impact" into a signal that rises at every impact (the envelope).
 * 3. FFT of the envelope: the impact RATE now shows up as a peak, at the
 *    outer-race, ball or inner-race fault frequency.
 *
 * Only the filter outputs that use samples from inside this frame are kept
 * ("valid" part: the first TAPS-1 outputs would need samples from before the
 * frame). That way the result is the same as a filter running continuously
 * on the board, e.g. on the FMAC. */
#define VALID (N - LAENNEC_BANDPASS_TAPS + 1) /* 960 samples */

static void envelope_features(const float* x, laennec_workspace* ws,
                              float out[LAENNEC_NUM_FEATURES]) {
    float mean = 0.0f;
    for (size_t i = 0; i < VALID; ++i) {
        const size_t n = i + LAENNEC_BANDPASS_TAPS - 1; /* newest sample used */
        float y = 0.0f;
        for (size_t k = 0; k < LAENNEC_BANDPASS_TAPS; ++k) {
            y += LAENNEC_BANDPASS[k] * x[n - k];
        }
        ws->work[i] = fabsf(y);
        mean += ws->work[i];
    }
    mean /= (float)VALID;
    MARK(6);

    for (size_t i = 0; i < N; ++i) {
        /* Remove the envelope's average, taper it, and pad with zeros to 1024. */
        ws->re[i] = i < VALID ? (ws->work[i] - mean) * hann(i, VALID) : 0.0f;
        ws->im[i] = 0.0f;
    }
    MARK(7);
    laennec_fft(ws->re, ws->im, N);
    MARK(8);

    /* Energy in each fault band, as a fraction of the envelope energy up to 500 Hz. */
    float* p = ws->work;
    const size_t top = bin_of(500.0f);
    for (size_t k = 1; k <= top; ++k) {
        p[k] = ws->re[k] * ws->re[k] + ws->im[k] * ws->im[k];
    }
    const float total = power_sum(p, 1, top);
    const float per_hz[3] = {LAENNEC_BPFO_PER_HZ, LAENNEC_BSF_PER_HZ, LAENNEC_BPFI_PER_HZ};
    const int index[3] = {LAENNEC_F_ENV_OUTER, LAENNEC_F_ENV_BALL, LAENNEC_F_ENV_INNER};
    for (int f = 0; f < 3; ++f) {
        const size_t lo = bin_of(per_hz[f] * LAENNEC_SHAFT_MIN_HZ);
        const size_t hi = bin_of(per_hz[f] * LAENNEC_SHAFT_MAX_HZ);
        out[index[f]] = total > TINY ? power_sum(p, lo, hi) / total : 0.0f;
    }
}

void laennec_compute_features(const float frame[LAENNEC_FRAME_LEN], laennec_workspace* ws,
                              float out[LAENNEC_NUM_FEATURES]) {
    /* Remove the average (DC offset) into a copy, so the input stays untouched. */
    MARK(0);
    float mean = 0.0f;
    for (size_t i = 0; i < N; ++i) mean += frame[i];
    mean /= (float)N;
    for (size_t i = 0; i < N; ++i) ws->x[i] = frame[i] - mean;
    MARK(1);

    time_features(ws->x, out);
    MARK(2);
    spectrum_features(ws->x, ws, out);
    MARK(5);
    envelope_features(ws->x, ws, out);
    MARK(9);
}
