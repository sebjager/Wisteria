// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>

#include <fmt/format.h>

#include "common/nx2_compat_report.h"

namespace Common::Nx2Compat {
namespace {

struct Entry {
    u64 count = 0;
    double first = 0.0;
};

// Frame times in 0.25 ms buckets up to 250 ms; the last bucket holds everything longer.
constexpr size_t FrameBuckets = 1001;
constexpr double FrameBucketMs = 0.25;

struct Report {
    std::mutex mutex;
    std::map<std::tuple<std::string, u32, std::string>, Entry> stubs;
    std::map<std::pair<std::string, std::string>, Entry> rejects;
    std::array<std::tuple<double, std::string, u32>, 16> last_requests{};
    size_t next_request = 0;
    std::vector<std::pair<double, u64>> heartbeats;
    double first_present = -1.0;
    double last_present = -1.0;
    u64 presents = 0;
    double last_write = -10.0;
    std::array<u64, FrameBuckets> frame_histogram{};
    u64 frame_count = 0;
    double frame_sum_ms = 0.0;
    double frame_max_ms = 0.0;
    u64 frames_over_50ms = 0;
    double last_present_time = -1.0;
};

double Warmup() {
    static const double warmup = [] {
        const char* value = std::getenv("EDEN_NX2_COMPAT_WARMUP");
        return value != nullptr ? std::atof(value) : 20.0;
    }();
    return warmup;
}

double Percentile(const Report& report, double fraction) {
    if (report.frame_count == 0) {
        return 0.0;
    }
    const u64 target = static_cast<u64>(fraction * static_cast<double>(report.frame_count - 1)) + 1;
    u64 seen = 0;
    for (size_t i = 0; i < FrameBuckets; ++i) {
        seen += report.frame_histogram[i];
        if (seen >= target) {
            return i + 1 == FrameBuckets ? report.frame_max_ms : (i + 0.5) * FrameBucketMs;
        }
    }
    return report.frame_max_ms;
}

const char* Path() {
    static const char* const path = std::getenv("EDEN_NX2_COMPAT_REPORT");
    return path;
}

Report& GetReport() {
    static Report report;
    return report;
}

std::string Clean(std::string_view text) {
    std::string out{text.substr(0, 160)};
    for (char& c : out) {
        if (c == '\n' || c == '\r' || c == '\t') {
            c = ' ';
        }
    }
    return out;
}

void Write(Report& report, std::string_view status, double now) {
    std::FILE* file = std::fopen(Path(), "wb");
    if (file == nullptr) {
        return;
    }
    std::string text = "# eden nx2 compat report\n";
    text += fmt::format("status {}\nelapsed {:.1f}\npresents {}\n", status, now, report.presents);
    text += fmt::format("first_present {:.1f}\nlast_present {:.1f}\n", report.first_present,
                        report.last_present);
    if (report.frame_count != 0) {
        text += fmt::format("frametime {} {:.2f} {:.2f} {:.2f} {:.2f} {:.2f} {:.2f} {}\n",
                            report.frame_count, report.frame_sum_ms / report.frame_count,
                            Percentile(report, 0.5), Percentile(report, 0.95),
                            Percentile(report, 0.99), Percentile(report, 0.999),
                            report.frame_max_ms, report.frames_over_50ms);
    }
    for (const auto& [time, presents] : report.heartbeats) {
        text += fmt::format("heartbeat {:.1f} {}\n", time, presents);
    }
    for (const auto& [key, entry] : report.stubs) {
        text += fmt::format("stub {} {:.1f} {} {} {}\n", entry.count, entry.first, std::get<0>(key),
                            std::get<1>(key), std::get<2>(key));
    }
    for (const auto& [key, entry] : report.rejects) {
        text += fmt::format("reject {} {:.1f} {} {}\n", entry.count, entry.first, key.first,
                            key.second);
    }
    for (size_t i = 0; i < report.last_requests.size(); ++i) {
        const auto& [time, service, command] =
            report.last_requests[(report.next_request + i) % report.last_requests.size()];
        if (!service.empty()) {
            text += fmt::format("last_ipc {:.1f} {} {}\n", time, service, command);
        }
    }
    std::fwrite(text.data(), 1, text.size(), file);
    std::fclose(file);
}

} // Anonymous namespace

bool Enabled() {
    return Path() != nullptr;
}

double Elapsed() {
    static const auto begin = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
}

void NoteAutoStub(std::string_view service, u32 command, std::string_view name) {
    if (!Enabled()) {
        return;
    }
    auto& report = GetReport();
    const double now = Elapsed();
    std::scoped_lock lock{report.mutex};
    auto& entry = report.stubs[{std::string{service}, command, Clean(name)}];
    if (entry.count++ == 0) {
        entry.first = now;
    }
}

void NoteShaderReject(std::string_view kind, std::string_view reason) {
    if (!Enabled()) {
        return;
    }
    auto& report = GetReport();
    const double now = Elapsed();
    std::scoped_lock lock{report.mutex};
    if (report.rejects.size() >= 1000) {
        return;
    }
    auto& entry = report.rejects[{std::string{kind}, Clean(reason)}];
    if (entry.count++ == 0) {
        entry.first = now;
    }
}

void NoteRequest(std::string_view service, u32 command) {
    if (!Enabled()) {
        return;
    }
    auto& report = GetReport();
    const double now = Elapsed();
    std::scoped_lock lock{report.mutex};
    report.last_requests[report.next_request] = {now, std::string{service}, command};
    report.next_request = (report.next_request + 1) % report.last_requests.size();
}

void NotePresent() {
    if (!Enabled()) {
        return;
    }
    auto& report = GetReport();
    const double now = Elapsed();
    std::scoped_lock lock{report.mutex};
    const double previous = report.last_present_time;
    report.last_present_time = now;
    if (previous < 0.0 || previous < Warmup()) {
        return;
    }
    const double ms = (now - previous) * 1000.0;
    const size_t bucket = std::min(static_cast<size_t>(ms / FrameBucketMs), FrameBuckets - 1);
    ++report.frame_histogram[bucket];
    ++report.frame_count;
    report.frame_sum_ms += ms;
    report.frame_max_ms = std::max(report.frame_max_ms, ms);
    if (ms > 50.0) {
        ++report.frames_over_50ms;
    }
}

void Tick(u64 presents, std::string_view status, bool final) {
    if (!Enabled()) {
        return;
    }
    auto& report = GetReport();
    const double now = Elapsed();
    std::scoped_lock lock{report.mutex};
    if (presents != report.presents) {
        if (report.first_present < 0.0) {
            report.first_present = now;
        }
        report.last_present = now;
        report.presents = presents;
    }
    if (!final && now - report.last_write < 5.0) {
        return;
    }
    report.last_write = now;
    report.heartbeats.emplace_back(now, presents);
    Write(report, status, now);
}

} // namespace Common::Nx2Compat
