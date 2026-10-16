// FFT tests: known answers, and a slow textbook DFT as an independent reference.

#include "laennec/fft.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <random>
#include <vector>

using Catch::Matchers::WithinAbs;

TEST_CASE("fft rejects sizes that are not a power of two", "[fft]") {
    std::vector<float> re(12, 1.0f), im(12, 0.0f);
    CHECK(laennec_fft(re.data(), im.data(), 12) == -1);
    CHECK(re[0] == 1.0f);  // untouched
    CHECK(laennec_fft(re.data(), im.data(), 1) == -1);
    // Larger than the 1024-entry twiddle table in tables.h.
    std::vector<float> big(2048, 0.0f), big_im(2048, 0.0f);
    CHECK(laennec_fft(big.data(), big_im.data(), 2048) == -1);
}

TEST_CASE("an impulse contains every frequency equally", "[fft]") {
    std::vector<float> re(16, 0.0f), im(16, 0.0f);
    re[0] = 1.0f;
    REQUIRE(laennec_fft(re.data(), im.data(), 16) == 0);
    for (std::size_t k = 0; k < 16; ++k) {
        CHECK_THAT(re[k], WithinAbs(1.0, 1e-6));
        CHECK_THAT(im[k], WithinAbs(0.0, 1e-6));
    }
}

TEST_CASE("a cosine lands in exactly one bin (and its mirror)", "[fft]") {
    const std::size_t n = 1024, bin = 5;
    std::vector<float> re(n), im(n, 0.0f);
    for (std::size_t i = 0; i < n; ++i) {
        re[i] = static_cast<float>(std::cos(2.0 * M_PI * static_cast<double>(bin * i) / static_cast<double>(n)));
    }
    REQUIRE(laennec_fft(re.data(), im.data(), n) == 0);
    for (std::size_t k = 0; k < n; ++k) {
        const double mag = std::hypot(re[k], im[k]);
        const double expected = (k == bin || k == n - bin) ? n / 2.0 : 0.0;
        CHECK_THAT(mag, WithinAbs(expected, 2e-2));
    }
}

TEST_CASE("fft matches a slow textbook DFT on random input", "[fft]") {
    const std::size_t n = 256;
    std::mt19937 rng(42);
    std::normal_distribution<float> noise(0.0f, 1.0f);
    std::vector<float> re(n), im(n);
    for (std::size_t i = 0; i < n; ++i) {
        re[i] = noise(rng);
        im[i] = noise(rng);
    }
    const auto in_re = re, in_im = im;
    REQUIRE(laennec_fft(re.data(), im.data(), n) == 0);

    double worst = 0.0;
    for (std::size_t k = 0; k < n; ++k) {
        double sr = 0.0, si = 0.0;  // X[k] = sum x[i] * e^(-2*pi*j*k*i/n), in double precision
        for (std::size_t i = 0; i < n; ++i) {
            const double a = -2.0 * M_PI * static_cast<double>(k * i % n) / static_cast<double>(n);
            const double xr = static_cast<double>(in_re[i]), xi = static_cast<double>(in_im[i]);
            sr += xr * std::cos(a) - xi * std::sin(a);
            si += xr * std::sin(a) + xi * std::cos(a);
        }
        worst = std::max(worst, std::hypot(sr - static_cast<double>(re[k]), si - static_cast<double>(im[k])));
    }
    CHECK(worst < 1e-3);  // float rounding on values of size ~16
}
