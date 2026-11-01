// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <string_view>
#include <thread>

#include "common/cpu_features.h"
#include "common/memory_detect.h"
#include "common/settings_recommendations.h"

namespace Settings::Recommendations {

void DetectHostCpuAndMemory(HostHardware& hardware) {
#ifdef ARCHITECTURE_x86_64
    std::string_view name{Common::g_cpu_caps.cpu_string, sizeof(Common::g_cpu_caps.cpu_string)};
    name = name.substr(0, name.find('\0'));
    while (!name.empty() && name.back() == ' ') {
        name.remove_suffix(1);
    }
    while (!name.empty() && name.front() == ' ') {
        name.remove_prefix(1);
    }
    hardware.cpu_name = std::string{name};
#endif
    hardware.cpu_threads = std::thread::hardware_concurrency();
    hardware.system_memory = Common::GetMemInfo().TotalPhysicalMemory;
}

} // namespace Settings::Recommendations
