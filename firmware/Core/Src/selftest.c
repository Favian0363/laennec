/* On-target check: does the STM32 compute the same thing as the PC, and how fast?
 *
 * For every stored test frame:
 *   1. compute the 11 features and classify (the same core/ code as the PC),
 *   2. count CPU cycles for each step with the DWT cycle counter,
 *   3. compare the features with the PC's, and the class with the truth and
 *      with Python's prediction.
 */
#include "selftest.h"

#include <math.h>
#include <stdio.h>

#include "main.h" /* pulls in the CMSIS registers (DWT, CoreDebug) and SystemCoreClock */

#include "laennec/classifier.h"
#include "laennec/features.h"
#include "laennec/model_weights.h"

#if __has_include("laennec_test_frames.h")
#include "laennec_test_frames.h"
#define HAVE_TEST_FRAMES 1
#else
#define HAVE_TEST_FRAMES 0
#endif

/* 16 KB of scratch memory for the features. Static, not a local variable:
 * the stack is only 1 KB, so putting this on it would crash the board. */
static laennec_workspace ws;

/* ---- Per-stage profiling (needs LAENNEC_PROFILING, set in firmware/CMakeLists.txt) ----
 * features.c calls laennec_profile_mark(n) after each stage; we store the
 * cycle counter at that moment. stamp[n] - stamp[n-1] = cost of stage n. */
#define PROFILE_MARKS 10
static const char* const STAGE_NAMES[PROFILE_MARKS] = {
    "",                                  /* mark 0 = start */
    "remove mean",
    "time features (RMS, kurtosis, crest)",
    "Hann window, spectrum",
    "FFT, spectrum",
    "power + band fractions",
    "FIR band-pass + abs, envelope",
    "Hann window, envelope",
    "FFT, envelope",
    "envelope fault bands",
};
static volatile uint32_t stamp[PROFILE_MARKS];
static uint64_t stage_total[PROFILE_MARKS];

void laennec_profile_mark(int stage) {
    if (stage >= 0 && stage < PROFILE_MARKS) stamp[stage] = DWT->CYCCNT;
}

/* The DWT cycle counter: a 32-bit register that counts every CPU clock cycle
 * (170 million per second here). Turning it on is two register writes; HAL
 * has no driver for it, so this is done at the register level. */
static void cycle_counter_start(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; /* enable the debug/trace block */
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;            /* start counting */
}

#if HAVE_TEST_FRAMES
/* How far the board's features are from the PC's, as a fraction of the same
 * tolerance tools/check_features.py uses (1 = right at the limit). */
static float feature_error(const float* board, const float* pc) {
    float worst = 0.0f;
    for (int i = 0; i < LAENNEC_NUM_FEATURES; ++i) {
        const float err = fabsf(board[i] - pc[i]) / (1e-6f + 1e-4f * fabsf(pc[i]));
        if (err > worst) worst = err;
    }
    return worst;
}
#endif

void laennec_selftest(void) {
#if !HAVE_TEST_FRAMES
    printf("\r\nselftest: no test frames (run tools/export_test_frames.py, then rebuild)\r\n");
#else
    cycle_counter_start();
    const uint32_t mhz = SystemCoreClock / 1000000u;

    /* __DATE__ / __TIME__ are filled in by the compiler: if this line doesn't
     * change after a rebuild, the board is not running the new code. */
    printf("\r\n=== laennec self-test (built %s %s): %d frames from the unseen 3 hp load, CPU at %lu MHz ===\r\n",
           __DATE__, __TIME__, LAENNEC_TEST_FRAME_COUNT, (unsigned long)mhz);
    printf("file frame  true    board   feature cycles  classify cycles  feat err (%% of tol)\r\n");

    int correct = 0, agree = 0;
    uint32_t worst_cycles = 0, worst_err_pct = 0;
    uint64_t total_cycles = 0;

    for (int k = 0; k < LAENNEC_TEST_FRAME_COUNT; ++k) {
        const laennec_test_frame* t = &LAENNEC_TEST_FRAMES[k];
        float f[LAENNEC_NUM_FEATURES];

        const uint32_t t0 = DWT->CYCCNT;
        laennec_compute_features(t->samples, &ws, f);
        const uint32_t t1 = DWT->CYCCNT;
        const int c = laennec_classify(&LAENNEC_TRAINED_MODEL, f, NULL);
        const uint32_t t2 = DWT->CYCCNT;

        /* Unsigned subtraction gives the right answer even if the counter
         * wrapped around past 2^32 in between (it does every ~25 s). */
        const uint32_t feat_cycles = t1 - t0;
        const uint32_t class_cycles = t2 - t1;
        const uint32_t err_pct = (uint32_t)ceilf(feature_error(f, t->pc_features) * 100.0f);

        for (int m = 1; m < PROFILE_MARKS; ++m) stage_total[m] += stamp[m] - stamp[m - 1];

        correct += (c == t->truth);
        agree += (c == t->expected);
        total_cycles += feat_cycles + class_cycles;
        if (feat_cycles + class_cycles > worst_cycles) worst_cycles = feat_cycles + class_cycles;
        if (err_pct > worst_err_pct) worst_err_pct = err_pct;

        printf("%4d %5d  %-6s  %-6s  %13lu  %15lu  %18lu\r\n", t->file, t->frame,
               laennec_class_name(t->truth), laennec_class_name(c), (unsigned long)feat_cycles,
               (unsigned long)class_cycles, (unsigned long)err_pct);
    }

    /* Integer math only: printf on this board has no float support (nano libc). */
    const uint32_t avg_cycles = (uint32_t)(total_cycles / LAENNEC_TEST_FRAME_COUNT);
    const uint32_t worst_us = worst_cycles / mhz;
    const uint32_t budget_us = (uint32_t)((uint64_t)LAENNEC_FRAME_LEN * 1000000u / LAENNEC_SAMPLE_RATE_HZ);
    printf("\r\ncorrect vs truth:   %d / %d\r\n", correct, LAENNEC_TEST_FRAME_COUNT);
    printf("agrees with Python: %d / %d\r\n", agree, LAENNEC_TEST_FRAME_COUNT);
    printf("features vs PC:     worst %lu%% of the tolerance (100%% = at the limit)\r\n",
           (unsigned long)worst_err_pct);
    printf("time per frame:     average %lu cycles = %lu us, worst %lu us\r\n",
           (unsigned long)avg_cycles, (unsigned long)(avg_cycles / mhz), (unsigned long)worst_us);
    printf("CPU load:           %lu.%02lu%% of the %lu us frame budget (worst case)\r\n",
           (unsigned long)(worst_us * 100u / budget_us),
           (unsigned long)((worst_us * 10000u / budget_us) % 100u), (unsigned long)budget_us);

#ifdef LAENNEC_PROFILING
    /* Where the feature time goes, averaged over all frames. */
    uint64_t feat_total = 0;
    for (int m = 1; m < PROFILE_MARKS; ++m) feat_total += stage_total[m];
    if (feat_total == 0) feat_total = 1; /* avoid dividing by zero if the counter isn't running */
    printf("\r\nfeature stages (average per frame):\r\n");
    printf("%-38s %10s %8s %6s\r\n", "stage", "cycles", "us", "share");
    for (int m = 1; m < PROFILE_MARKS; ++m) {
        const uint32_t avg = (uint32_t)(stage_total[m] / LAENNEC_TEST_FRAME_COUNT);
        printf("%-38s %10lu %8lu %5lu%%\r\n", STAGE_NAMES[m], (unsigned long)avg,
               (unsigned long)(avg / mhz), (unsigned long)(stage_total[m] * 100u / feat_total));
    }
#else
    printf("\r\n(per-stage profile off: add LAENNEC_PROFILING in firmware/CMakeLists.txt)\r\n");
#endif
#endif
}
