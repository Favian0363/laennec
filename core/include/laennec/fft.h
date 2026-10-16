#ifndef LAENNEC_FFT_H
#define LAENNEC_FFT_H
/* A small in-place FFT (radix-2, Cooley-Tukey). Portable C, float only.
 *
 * An FFT turns a list of samples (signal over time) into a list of
 * frequencies (how much of each pitch is in the signal). Bin k of an
 * n-point FFT is the frequency k * sample_rate / n. */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Transforms (re, im) in place. n must be a power of two from 2 to 1024
 * (the size of the precomputed twiddle table, tables.h).
 * Returns 0 on success, -1 otherwise (arrays untouched). */
int laennec_fft(float* re, float* im, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* LAENNEC_FFT_H */
