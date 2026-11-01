// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <vector>

#include "common/common_types.h"

// Recommended global settings for the host's hardware (GPU, video memory, CPU, RAM).
//
// Only settings whose effect has been measured on real hardware are offered: every rule carries
// a status, and rules still waiting for a test run are kept out of the frontends (they are only
// logged). Per-game profiles are applied on top of the global settings, so they still win.
namespace Settings::Recommendations {

struct HostHardware {
    std::string gpu_name;
    bool gpu_integrated{};
    /// Size of the largest device-local heap in bytes (the card's VRAM), 0 when unknown.
    u64 gpu_memory{};
    std::string cpu_name;
    u32 cpu_threads{};
    /// Physical system memory in bytes, 0 when unknown.
    u64 system_memory{};
};

enum class Status {
    Verified,     ///< Measured on real hardware: offered to the user.
    NeedsTesting, ///< Not measured yet: only logged, never offered.
};

struct Recommendation {
    std::string id;         ///< Stable rule name, e.g. "vram_usage_mode.large_vram".
    std::string setting;    ///< Config key of the setting (Settings::values linkage key).
    std::string value;      ///< Value in the form BasicSetting::LoadString() takes.
    std::string value_name; ///< Human-readable value, e.g. "Aggressive".
    std::string reason;     ///< One or two sentences on why, for the user.
    Status status{Status::NeedsTesting};
};

struct Result {
    std::vector<Recommendation> settings;
    /// Things the user should know that no setting changes (e.g. too little RAM).
    std::vector<std::string> notices;
};

/// Bumped whenever a rule is added or changes status, so frontends can offer the
/// recommendations again once.
constexpr u32 RulesVersion = 1;

/// All rules that match the hardware, verified or not.
[[nodiscard]] Result Recommend(const HostHardware& hardware);

/// Only the rules a frontend may offer: verified ones, plus the untested ones when the
/// environment variable EDEN_RECOMMEND_UNTESTED=1 is set (for test runs).
[[nodiscard]] Result RecommendOffered(const HostHardware& hardware);

/// One line describing the hardware, for logs and dialogs.
[[nodiscard]] std::string DescribeHardware(const HostHardware& hardware);

[[nodiscard]] const char* StatusName(Status status);

/// Fills in the CPU and system memory fields of a HostHardware (the GPU fields come from the
/// renderer or the frontend's device list). Defined in settings_recommendations_host.cpp.
void DetectHostCpuAndMemory(HostHardware& hardware);

} // namespace Settings::Recommendations
