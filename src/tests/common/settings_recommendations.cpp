// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cstdlib>

#include <catch2/catch_test_macros.hpp>

#include "common/settings_enums.h"
#include "common/settings_recommendations.h"

namespace {
using namespace Settings::Recommendations;

constexpr u64 GiB = u64{1} << 30;

HostHardware Pc(u64 vram, bool integrated = false, u64 ram = 32 * GiB) {
    return HostHardware{
        .gpu_name = "Test GPU",
        .gpu_integrated = integrated,
        .gpu_memory = vram,
        .cpu_name = "Test CPU",
        .cpu_threads = 16,
        .system_memory = ram,
    };
}

const Recommendation* Find(const Result& result, const std::string& setting) {
    for (const auto& recommendation : result.settings) {
        if (recommendation.setting == setting) {
            return &recommendation;
        }
    }
    return nullptr;
}
} // Anonymous namespace

TEST_CASE("Recommendations: VRAM usage mode follows the card size", "[common]") {
    const auto big = Recommend(Pc(16 * GiB));
    const Recommendation* vram = Find(big, "vram_usage_mode");
    REQUIRE(vram != nullptr);
    REQUIRE(vram->value == std::to_string(static_cast<u32>(Settings::VramUsageMode::Aggressive)));
    REQUIRE(vram->value_name == "Aggressive");

    // 8 GiB: Conservative and Aggressive come to the same 7 GiB.
    REQUIRE(Find(Recommend(Pc(8 * GiB)), "vram_usage_mode") == nullptr);
    // Integrated GPUs share system memory: no recommendation.
    REQUIRE(Find(Recommend(Pc(16 * GiB, true)), "vram_usage_mode") == nullptr);
    // Unknown GPU: nothing.
    REQUIRE(Find(Recommend(Pc(0)), "vram_usage_mode") == nullptr);
}

TEST_CASE("Recommendations: untested rules are not offered", "[common]") {
#ifdef _WIN32
    _putenv_s("EDEN_RECOMMEND_UNTESTED", "");
#else
    unsetenv("EDEN_RECOMMEND_UNTESTED");
#endif
    const auto all = Recommend(Pc(16 * GiB));
    const auto offered = RecommendOffered(Pc(16 * GiB));
    for (const auto& recommendation : all.settings) {
        const bool listed = Find(offered, recommendation.setting) != nullptr;
        REQUIRE(listed == (recommendation.status == Status::Verified));
    }
    REQUIRE(offered.notices.size() == all.notices.size());
}

TEST_CASE("Recommendations: notices for small RAM and small cards", "[common]") {
    REQUIRE(Recommend(Pc(16 * GiB)).notices.empty());
    REQUIRE(Recommend(Pc(16 * GiB, false, 8 * GiB)).notices.size() == 1);
    // 16 GB machines report a little under 16 GiB.
    REQUIRE(Recommend(Pc(16 * GiB, false, 15 * GiB + GiB / 2)).notices.empty());
    // 12 GB card: Switch 2 titles fall back to BC3.
    REQUIRE(Recommend(Pc(12 * GiB)).notices.size() == 1);
    REQUIRE(Recommend(Pc(8 * GiB, false, 8 * GiB)).notices.size() == 2);
}

TEST_CASE("Recommendations: hardware description", "[common]") {
    REQUIRE(DescribeHardware(Pc(16 * GiB)) ==
            "Test GPU (16.0 GiB VRAM), Test CPU (16 threads), 32.0 GiB RAM");
    REQUIRE(DescribeHardware(HostHardware{}) == "Unknown GPU, unknown CPU");
}
