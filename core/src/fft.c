#include "laennec/fft.h"

#include "laennec/tables.h"

int laennec_fft(float* re, float* im, size_t n) {
    if (n < 2 || (n & (n - 1)) != 0 || n > LAENNEC_TABLE_FFT_SIZE) {
        return -1;
    }

    /* 1. Reorder the samples into "bit-reversed" order: sample 1 (binary 0001)
     *    swaps with sample 8 (binary 1000) when n = 16, and so on. After this,
     *    the butterflies below can work in place. */
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            const float tr = re[i], ti = im[i];
            re[i] = re[j];
            im[i] = im[j];
            re[j] = tr;
            im[j] = ti;
        }
    }

    /* 2. Combine pairs into 2-point FFTs, those into 4-point FFTs, and so on
     *    ("butterflies"). Twiddle factor w = e^(-i * 2*pi*k/len). Instead of
     *    calling cosf/sinf (~175 cycles each on the STM32), read it from the
     *    table of e^(i * 2*pi*j/1024): angle 2*pi*k/len = 2*pi*(k*stride)/1024. */
    for (size_t len = 2; len <= n; len <<= 1) {
        const size_t half = len >> 1;
        const size_t stride = LAENNEC_TABLE_FFT_SIZE / len;
        for (size_t k = 0; k < half; ++k) {
            const float wr = LAENNEC_TWIDDLE_COS[k * stride];
            const float wi = -LAENNEC_TWIDDLE_SIN[k * stride]; /* minus: e^(-i*angle) */
            for (size_t start = 0; start < n; start += len) {
                const size_t a = start + k;
                const size_t b = a + half;
                const float tr = wr * re[b] - wi * im[b];
                const float ti = wr * im[b] + wi * re[b];
                re[b] = re[a] - tr;
                im[b] = im[a] - ti;
                re[a] += tr;
                im[a] += ti;
            }
        }
    }
    return 0;
}
