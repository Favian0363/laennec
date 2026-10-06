#include "vigil/frames.h"

size_t vigil_frame_count(size_t n_samples, size_t frame_len, size_t hop) {
    if (frame_len == 0 || hop == 0 || n_samples < frame_len) {
        return 0;
    }
    return (n_samples - frame_len) / hop + 1;
}

float vigil_remove_mean(float* x, size_t n) {
    if (n == 0) {
        return 0.0f;
    }
    float sum = 0.0f;
    for (size_t i = 0; i < n; ++i) {
        sum += x[i];
    }
    const float mean = sum / (float)n;
    for (size_t i = 0; i < n; ++i) {
        x[i] -= mean;
    }
    return mean;
}
