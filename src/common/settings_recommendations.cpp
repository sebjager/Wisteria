// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cstdio>
#include <cstdlib>
#include <vector>

#include "common/settings_enums.h"
#include "common/settings_recommendations.h"

namespace Settings::Recommendations {
namespace {

constexpr u64 GiB = u64{1} << 30;

std::string FormatGiB(u64 bytes) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.1f GiB", static_cast<double>(bytes) / GiB);
    return buffer;
}

template <typename Enum>
std::string EnumValue(Enum value) {
    return std::to_string(static_cast<u32>(value));
}

// Conservative caps the video memory Eden plans for at 6 GiB + 1 GiB per resolution step (and
// keeps 1/8 of the card, at most 1 GiB, in reserve; vulkan_device.cpp). Aggressive uses the
// driver's budget minus that reserve. On an 8 GiB card both come to 7 GiB, so the rule starts
// where Aggressive gives a real margin. Switch 2 titles already follow the driver's budget in
// the texture cache, so this mainly helps Switch 1 titles and the buffer cache.
void VramUsageRule(const HostHardware& hardware, Result& result) {
    if (hardware.gpu_integrated || hardware.gpu_memory < 10 * GiB) {
        return;
    }
    result.settings.push_back(Recommendation{
        .id = "vram_usage_mode.large_vram",
        .setting = "vram_usage_mode",
        .value = EnumValue(VramUsageMode::Aggressive),
        .value_name = std::string{CanonicalizeEnum(VramUsageMode::Aggressive)},
        .reason = "On Conservative, Eden plans for at most about 7 GiB of video memory. Your GPU "
                  "has " +
                  FormatGiB(hardware.gpu_memory) +
                  ", so Aggressive lets games keep more textures resident instead of evicting "
                  "and reloading them.",
        // Waiting for the measurement on a 16 GB card (Switch 1 and Switch 2 title, budget
        // shrinking while another program uses video memory).
        .status = Status::NeedsTesting,
    });
}

void MemoryNotices(const HostHardware& hardware, Result& result) {
    if (hardware.system_memory != 0 && hardware.system_memory < 13 * GiB) {
        result.notices.push_back(
            "Your PC has " + FormatGiB(hardware.system_memory) +
            " of RAM. Switch 2 games emulate 12 GB of console memory on top of what Eden itself "
            "uses, so they may stutter or fail to start. 16 GB or more is a better fit.");
    }
    // UseBc3ForSwitch2OnSmallCards (vk_texture_cache.cpp) switches to BC3 below a 12 GiB budget,
    // which every card up to 12 GB has once Windows takes its share.
    if (!hardware.gpu_integrated && hardware.gpu_memory != 0 &&
        hardware.gpu_memory < 12 * GiB + GiB / 2) {
        result.notices.push_back(
            "Your GPU has " + FormatGiB(hardware.gpu_memory) +
            " of video memory. Switch 2 games re-compress ASTC textures to BC3 on it "
            "automatically (uncompressed they need a 12 GiB budget); set EDEN_NX2_KEEP_ASTC=1 "
            "to keep them uncompressed.");
    }
}

bool OfferUntested() {
    const char* value = std::getenv("EDEN_RECOMMEND_UNTESTED");
    return value != nullptr && value[0] == '1';
}

} // Anonymous namespace

Result Recommend(const HostHardware& hardware) {
    Result result;
    VramUsageRule(hardware, result);
    MemoryNotices(hardware, result);
    return result;
}

Result RecommendOffered(const HostHardware& hardware) {
    Result result = Recommend(hardware);
    if (!OfferUntested()) {
        std::erase_if(result.settings, [](const Recommendation& recommendation) {
            return recommendation.status != Status::Verified;
        });
    }
    return result;
}

std::string DescribeHardware(const HostHardware& hardware) {
    std::string text = hardware.gpu_name.empty() ? "Unknown GPU" : hardware.gpu_name;
    if (hardware.gpu_memory != 0) {
        text += " (" + FormatGiB(hardware.gpu_memory) +
                (hardware.gpu_integrated ? " shared)" : " VRAM)");
    } else if (hardware.gpu_integrated) {
        text += " (integrated)";
    }
    text += ", " + (hardware.cpu_name.empty() ? std::string{"unknown CPU"} : hardware.cpu_name);
    if (hardware.cpu_threads != 0) {
        text += " (" + std::to_string(hardware.cpu_threads) + " threads)";
    }
    if (hardware.system_memory != 0) {
        text += ", " + FormatGiB(hardware.system_memory) + " RAM";
    }
    return text;
}

const char* StatusName(Status status) {
    switch (status) {
    case Status::Verified:
        return "verified";
    case Status::NeedsTesting:
        return "needs testing";
    }
    return "unknown";
}

} // namespace Settings::Recommendations
