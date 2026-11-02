// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <chrono>
#include <future>
#include <thread>
#include <catch2/catch_test_macros.hpp>
#include "common/windows/high_resolution_event.h"

using namespace std::chrono_literals;

TEST_CASE("Windows timer preserves event signals and timeout deadlines", "[common][windows]") {
    for (bool enabled : {false, true}) {
        Common::Windows::HighResolutionEvent event(enabled);
        if (enabled && !event.IsHighResolution()) {
            SKIP("High-resolution waitable timers unavailable");
        }
        event.Set();
        event.Set();
        REQUIRE(event.WaitFor(0ns));
        REQUIRE_FALSE(event.WaitFor(0ns));
        const auto start = std::chrono::steady_clock::now();
        REQUIRE_FALSE(event.WaitFor(2ms));
        if (event.IsHighResolution()) {
            REQUIRE(std::chrono::steady_clock::now() - start >= 2ms);
        }
    }
}

TEST_CASE("Windows timer can be interrupted and rearmed", "[common][windows]") {
    Common::Windows::HighResolutionEvent event;
    if (!event.IsHighResolution()) {
        SKIP("High-resolution waitable timers unavailable");
    }
    for (int i = 0; i < 20; ++i) {
        std::promise<void> ready;
        std::promise<bool> result;
        auto completion = result.get_future();
        std::jthread waiter([&] {
            ready.set_value();
            result.set_value(event.WaitFor(5s));
        });
        ready.get_future().wait();
        std::this_thread::sleep_for(1ms);
        event.Set();
        const auto status = completion.wait_for(500ms);
        event.Set();
        waiter.join();
        REQUIRE(status == std::future_status::ready);
        REQUIRE(completion.get());
        // A cleanup signal may remain if the first one already woke the waiter.
        event.WaitFor(0ns);
        const auto start = std::chrono::steady_clock::now();
        REQUIRE_FALSE(event.WaitFor(2ms));
        REQUIRE(std::chrono::steady_clock::now() - start >= 2ms);
    }
}

TEST_CASE("Windows timer wakes an indefinite wait", "[common][windows]") {
    Common::Windows::HighResolutionEvent event;
    std::promise<void> completed;
    auto result = completed.get_future();
    std::jthread waiter([&] {
        event.Wait();
        completed.set_value();
    });
    event.Set();
    const auto status = result.wait_for(500ms);
    event.Set();
    waiter.join();
    REQUIRE(status == std::future_status::ready);
}
