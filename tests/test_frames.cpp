#include "vigil/frames.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cstddef>

using Catch::Matchers::WithinAbs;

TEST_CASE("frame count: only full frames count", "[frames]") {
    CHECK(vigil_frame_count(1024, 1024, 1024) == std::size_t{1});
    CHECK(vigil_frame_count(2047, 1024, 1024) == std::size_t{1});  // the leftover 1023 don't count
    CHECK(vigil_frame_count(2048, 1024, 1024) == std::size_t{2});
    CHECK(vigil_frame_count(2048, 1024, 512) == std::size_t{3});   // 50% overlap: starts at 0, 512, 1024
    CHECK(vigil_frame_count(100, 1024, 1024) == std::size_t{0});   // shorter than one frame
}

TEST_CASE("frame count: zero sizes give zero frames", "[frames]") {
    CHECK(vigil_frame_count(4096, 0, 512) == std::size_t{0});
    CHECK(vigil_frame_count(4096, 1024, 0) == std::size_t{0});
}

TEST_CASE("remove_mean centers the signal on zero", "[frames]") {
    float x[] = {1.0f, 2.0f, 3.0f, 4.0f};
    CHECK_THAT(vigil_remove_mean(x, 4), WithinAbs(2.5, 1e-6));
    CHECK_THAT(x[0], WithinAbs(-1.5, 1e-6));
    CHECK_THAT(x[1], WithinAbs(-0.5, 1e-6));
    CHECK_THAT(x[2], WithinAbs(0.5, 1e-6));
    CHECK_THAT(x[3], WithinAbs(1.5, 1e-6));
}

TEST_CASE("remove_mean on an empty signal does nothing", "[frames]") {
    CHECK_THAT(vigil_remove_mean(nullptr, 0), WithinAbs(0.0, 0.0));
}
