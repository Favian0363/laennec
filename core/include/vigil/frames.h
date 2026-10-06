#ifndef VIGIL_FRAMES_H
#define VIGIL_FRAMES_H
/* Cutting a long recording into frames (short chunks) for analysis.
 *
 * Portable C: no STM32 code here. These same files are compiled into the
 * firmware AND into the PC tests, so the PC and the board always run
 * identical code. */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* How many full frames of `frame_len` samples fit in `n_samples`, when each
 * new frame starts `hop` samples after the previous one. A partial frame at
 * the end does not count. Returns 0 if frame_len or hop is 0. */
size_t vigil_frame_count(size_t n_samples, size_t frame_len, size_t hop);

/* Subtracts the average from every sample (removes the DC offset) and
 * returns the average that was removed. Does nothing for n = 0. */
float vigil_remove_mean(float* x, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* VIGIL_FRAMES_H */
