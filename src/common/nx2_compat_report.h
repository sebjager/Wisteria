// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string_view>
#include "common/common_types.h"

// EDEN_NX2_COMPAT_REPORT=<file>: a small text report for unattended compatibility runs
// (tools/nx2/compat_run.py). It is written outside the log, so it survives the log's 100 MiB cap
// and a crash leaves the last heartbeat behind. Nothing is recorded when the variable is unset.
namespace Common::Nx2Compat {

bool Enabled();

/// Seconds since the first call into this report.
double Elapsed();

void NoteAutoStub(std::string_view service, u32 command, std::string_view name);
void NoteShaderReject(std::string_view kind, std::string_view reason);
void NoteRequest(std::string_view service, u32 command);

/// Called on every guest present: frame times after EDEN_NX2_COMPAT_WARMUP seconds (default 20)
/// go into a histogram, reported as mean and percentiles.
void NotePresent();

/// Records the present count and rewrites the file at most every 5 s (always when final).
void Tick(u64 presents, std::string_view status, bool final);

} // namespace Common::Nx2Compat
