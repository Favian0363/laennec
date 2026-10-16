// Feature tests with signals whose answers are known by hand.

#include "laennec/features.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace {

constexpr double kFs = LAENNEC_SAMPLE_RATE_HZ;
using Frame = std::array<float, LAENNEC_FRAME_LEN>;
using Features = std::array<float, LAENNEC_NUM_FEATURES>;

Features features_of(const Frame& x) {
    static laennec_workspace ws;  // 16 KB: static keeps it off the stack
    Features f{};
    laennec_compute_features(x.data(), &ws, f.data());
    return f;
}

Frame sine(double hz, double amplitude = 1.0) {
    Frame x{};
    for (std::size_t i = 0; i < x.size(); ++i) {
        x[i] = static_cast<float>(amplitude * std::sin(2.0 * M_PI * hz * static_cast<double>(i) / kFs));
    }
    return x;
}

// A 3.5 kHz "ringing" whose loudness pulses `rate_hz` times per second: what a
// bearing defect looks like after each impact.
Frame pulsing_ring(double rate_hz) {
    Frame x{};
    for (std::size_t i = 0; i < x.size(); ++i) {
        const double t = static_cast<double>(i) / kFs;
        const double loudness = 1.0 + 0.9 * std::cos(2.0 * M_PI * rate_hz * t);
        x[i] = static_cast<float>(loudness * std::sin(2.0 * M_PI * 3500.0 * t));
    }
    return x;
}

int strongest_envelope_band(const Features& f) {
    const float o = f[LAENNEC_F_ENV_OUTER], b = f[LAENNEC_F_ENV_BALL], in = f[LAENNEC_F_ENV_INNER];
    if (o >= b && o >= in) return LAENNEC_F_ENV_OUTER;
    return b >= in ? LAENNEC_F_ENV_BALL : LAENNEC_F_ENV_INNER;
}

}  // namespace

TEST_CASE("a 1.5 kHz sine: known RMS, kurtosis, crest, centroid and band", "[features]") {
    // 1500 Hz is exactly FFT bin 128 (1500 / 11.71875), so no leakage at all.
    const Features f = features_of(sine(1500.0));
    CHECK_THAT(f[LAENNEC_F_LOG_RMS], WithinAbs(std::log10(1.0 / std::sqrt(2.0)), 1e-4));
    CHECK_THAT(f[LAENNEC_F_KURTOSIS], WithinAbs(1.5, 1e-3));
    CHECK_THAT(f[LAENNEC_F_CREST], WithinAbs(std::sqrt(2.0), 1e-3));
    CHECK_THAT(f[LAENNEC_F_CENTROID_KHZ], WithinAbs(1.5, 1e-3));
    CHECK_THAT(f[LAENNEC_F_BAND_1_2K5], WithinAbs(1.0, 1e-4));
    CHECK_THAT(f[LAENNEC_F_BAND_0_1K] + f[LAENNEC_F_BAND_2K5_4K] + f[LAENNEC_F_BAND_4K_6K],
               WithinAbs(0.0, 1e-4));
}

TEST_CASE("the band fractions always add up to 1", "[features]") {
    std::mt19937 rng(1);
    std::normal_distribution<float> noise(0.0f, 1.0f);
    Frame x{};
    for (auto& v : x) v = noise(rng);
    const Features f = features_of(x);
    const float sum = f[LAENNEC_F_BAND_0_1K] + f[LAENNEC_F_BAND_1_2K5] + f[LAENNEC_F_BAND_2K5_4K] +
                      f[LAENNEC_F_BAND_4K_6K];
    CHECK_THAT(sum, WithinAbs(1.0, 1e-4));
    CHECK_THAT(f[LAENNEC_F_KURTOSIS], WithinAbs(3.0, 0.5));  // noise: about 3
}

TEST_CASE("a DC offset doesn't change any feature", "[features]") {
    // The signal must have energy in the 2-5 kHz envelope band. With a plain
    // 700 Hz tone, the band-pass removes 99.94% of it and the envelope features
    // become ratios of float rounding noise, which no tolerance can pin down.
    Frame x = pulsing_ring(105.0), shifted = x;
    for (auto& v : x) v *= 0.3f;
    for (std::size_t i = 0; i < x.size(); ++i) shifted[i] = x[i] + 1.65f;  // ADC biased to mid-rail
    const Features a = features_of(x), b = features_of(shifted);
    for (std::size_t k = 0; k < a.size(); ++k) CHECK_THAT(b[k], WithinAbs(a[k], 1e-3));
}

TEST_CASE("the input frame is not modified", "[features]") {
    const Frame x = sine(900.0);
    Frame copy = x;
    features_of(copy);
    CHECK(std::equal(x.begin(), x.end(), copy.begin()));
}

TEST_CASE("impacts are spiky: high kurtosis and crest factor", "[features]") {
    Frame x{};
    for (std::size_t i = 0; i < x.size(); i += 112) x[i] = 1.0f;  // ~107 impacts/s
    const Features f = features_of(x);
    CHECK(f[LAENNEC_F_KURTOSIS] > 50.0f);
    CHECK(f[LAENNEC_F_CREST] > 7.0f);
}

TEST_CASE("silence gives finite features, no NaN", "[features]") {
    const Features f = features_of(Frame{});
    for (float v : f) CHECK(std::isfinite(v));
    CHECK(f[LAENNEC_F_KURTOSIS] == 0.0f);
}

TEST_CASE("envelope: the impact rate picks the matching fault band", "[features]") {
    // Fault rates for a shaft at ~29.3 Hz (1760 rpm): outer 105, ball 138, inner 159 Hz.
    const Features outer = features_of(pulsing_ring(105.0));
    const Features ball = features_of(pulsing_ring(138.0));
    const Features inner = features_of(pulsing_ring(159.0));
    CHECK(strongest_envelope_band(outer) == LAENNEC_F_ENV_OUTER);
    CHECK(strongest_envelope_band(ball) == LAENNEC_F_ENV_BALL);
    CHECK(strongest_envelope_band(inner) == LAENNEC_F_ENV_INNER);
    CHECK(outer[LAENNEC_F_ENV_OUTER] > 0.3f);
    CHECK(inner[LAENNEC_F_ENV_INNER] > 0.3f);
}

TEST_CASE("envelope ignores slow shaking outside the 2-5 kHz band", "[features]") {
    // A strong 105 Hz vibration with no ringing: the band-pass removes it,
    // so it must NOT look like an outer-race fault.
    const Features f = features_of(sine(105.0, 5.0));
    CHECK(f[LAENNEC_F_ENV_OUTER] < 0.3f);
}
