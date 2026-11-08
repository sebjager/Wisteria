// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "common/common_types.h"

namespace GameProfile {

/// Applies the optimization profile for program_id, if there is one: the user's
/// <config>/game_profiles/<id>.ini, else the one bundled from dist/game_profiles.
/// Call after the global and per-game configs are read: settings the per-game config sets keep
/// their value. EDEN_NX2_GAME_PROFILE=0 skips profiles, =<path> loads that file instead.
void Apply(u64 program_id);

} // namespace GameProfile
