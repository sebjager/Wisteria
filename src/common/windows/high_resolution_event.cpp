// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <windows.h>
#include "common/windows/high_resolution_event.h"

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

namespace Common::Windows {

HighResolutionEvent::HighResolutionEvent(bool enabled) {
    if (!enabled) {
        return;
    }
    timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                   TIMER_MODIFY_STATE | SYNCHRONIZE);
    if (!timer) {
        return;
    }
    wake_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!wake_event) {
        CloseHandle(timer);
        timer = nullptr;
    }
}

HighResolutionEvent::~HighResolutionEvent() {
    if (wake_event) {
        CloseHandle(wake_event);
        CloseHandle(timer);
    }
}

void HighResolutionEvent::Set() {
    fallback.Set();
    if (wake_event) {
        SetEvent(wake_event);
    }
}

void HighResolutionEvent::Wait() {
    fallback.Wait();
}

bool HighResolutionEvent::WaitFor(std::chrono::nanoseconds duration) {
    using namespace std::chrono_literals;
    // Leave the last part to the existing precise wait: Windows can round even high-resolution
    // timer wakes by several hundred microseconds. Short guest deadlines must not pay that cost.
    constexpr auto guard = 600us;
    if (!wake_event || duration < 800us) {
        return fallback.WaitFor(duration);
    }
    const auto deadline = std::chrono::steady_clock::now() + duration;
    // The user-space event owns the signal. Reset the kernel notification before testing it,
    // so a Set racing this check either remains in fallback or wakes the kernel wait.
    ResetEvent(wake_event);
    if (fallback.IsSet()) {
        fallback.Wait();
        return true;
    }
    // Round up to 100 ns and use a relative deadline so wall-clock changes cannot affect it.
    const auto coarse_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(duration - guard).count();
    const LARGE_INTEGER due{.QuadPart = -(coarse_ns / 100 + (coarse_ns % 100 != 0))};
    if (SetWaitableTimerEx(timer, &due, 0, nullptr, nullptr, nullptr, 0)) {
        const HANDLE handles[]{wake_event, timer};
        const DWORD result = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
        if (result != WAIT_OBJECT_0 + 1) {
            CancelWaitableTimer(timer);
        }
    }
    do {
        const auto remaining = std::chrono::duration_cast<std::chrono::nanoseconds>(
            deadline - std::chrono::steady_clock::now());
        if (fallback.WaitFor((std::max)(remaining, 0ns))) {
            return true;
        }
    } while (std::chrono::steady_clock::now() < deadline);
    return false;
}

} // namespace Common::Windows
