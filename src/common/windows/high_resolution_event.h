// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <chrono>
#include "common/thread.h"

namespace Common::Windows {

// A single-consumer event that blocks for long waits and preserves short-deadline precision.
class HighResolutionEvent {
public:
    explicit HighResolutionEvent(bool enabled = true);
    ~HighResolutionEvent();

    HighResolutionEvent(const HighResolutionEvent&) = delete;
    HighResolutionEvent& operator=(const HighResolutionEvent&) = delete;

    void Set();
    void Wait();
    bool WaitFor(std::chrono::nanoseconds duration);

    bool IsHighResolution() const {
        return wake_event != nullptr;
    }

private:
    void* wake_event{};
    void* timer{};
    Common::Event fallback;
};

} // namespace Common::Windows
