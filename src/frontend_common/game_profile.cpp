// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <optional>
#include <string>

#include <fmt/format.h>

#ifndef SI_NO_CONVERSION
#define SI_NO_CONVERSION 1
#endif
#include <SimpleIni.h>

#include "common/fs/file.h"
#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "common/logging.h"
#include "common/settings.h"
#include "frontend_common/bundled_game_profiles.h"
#include "frontend_common/game_profile.h"

namespace GameProfile {
namespace {

namespace FS = Common::FS;

std::string ToLower(std::string text) {
    std::ranges::transform(text, text.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return text;
}

bool IsNumber(const std::string& text) {
    return !text.empty() && std::ranges::all_of(text, [](unsigned char c) { return std::isdigit(c) != 0; });
}

// Enum settings only parse their index; accept the canonical name too ("High", "Bc3").
bool LoadEnumByName(Settings::BasicSetting& setting, const std::string& name) {
    const std::string wanted = ToLower(name);
    const std::string previous = setting.ToString();
    for (u32 i = 0; i < 256; ++i) {
        const std::string index = std::to_string(i);
        setting.LoadString(index);
        if (setting.ToString() != index) {
            break; // clamped: past the last value
        }
        if (ToLower(setting.Canonicalize()) == wanted) {
            return true;
        }
    }
    setting.LoadString(previous);
    return false;
}

bool LoadValue(Settings::BasicSetting& setting, const std::string& value) {
    if (setting.IsEnum() && !IsNumber(value)) {
        return LoadEnumByName(setting, value);
    }
    if (setting.TypeId() == "bool") {
        const std::string lower = ToLower(value);
        if (lower == "true" || lower == "1") {
            setting.LoadString("true");
        } else if (lower == "false" || lower == "0") {
            setting.LoadString("false");
        } else {
            return false;
        }
        return true;
    }
    setting.LoadString(value);
    return true;
}

void SetEnvironment(const char* key, const char* value) {
#ifdef _WIN32
    _putenv_s(key, value);
#else
    setenv(key, value, 0);
#endif
}

struct Source {
    std::string text;
    std::string origin;
};

std::optional<Source> FindProfile(u64 program_id) {
    const std::string file_name = fmt::format("{:016X}.ini", program_id);
    if (const char* forced = std::getenv("EDEN_NX2_GAME_PROFILE"); forced && *forced) {
        if (std::string_view{forced} == "0") {
            return std::nullopt;
        }
        std::string text = FS::ReadStringFromFile(std::string{forced}, FS::FileType::TextFile);
        if (text.empty()) {
            LOG_WARNING(Config, "Game profile {} is missing or empty", forced);
            return std::nullopt;
        }
        return Source{std::move(text), forced};
    }

    const auto user_path = FS::GetEdenPath(FS::EdenPath::ConfigDir) / "game_profiles" / file_name;
    if (FS::Exists(user_path)) {
        return Source{FS::ReadStringFromFile(user_path, FS::FileType::TextFile),
                      FS::PathToUTF8String(user_path)};
    }

    const std::string wanted = ToLower(file_name);
    for (const auto& profile : BUNDLED_GAME_PROFILES) {
        if (ToLower(std::string{profile.name}) == wanted) {
            return Source{std::string{profile.source}, fmt::format("bundled {}", profile.name)};
        }
    }
    return std::nullopt;
}

} // namespace

void Apply(u64 program_id) {
    if (program_id == 0) {
        return;
    }
    const auto source = FindProfile(program_id);
    if (!source) {
        return;
    }

    CSimpleIniA ini;
    ini.SetUnicode(true);
    if (ini.LoadData(source->text) < 0) {
        LOG_ERROR(Config, "Game profile {} could not be parsed", source->origin);
        return;
    }
    LOG_INFO(Config, "Game profile for {:016X}: {} ({})", program_id,
             ini.GetValue("Profile", "name", "unnamed"), source->origin);

    CSimpleIniA::TNamesDepend keys;
    ini.GetAllKeys("Settings", keys);
    keys.sort(CSimpleIniA::Entry::LoadOrder());
    for (const auto& key : keys) {
        const std::string name = key.pItem;
        const std::string value = ini.GetValue("Settings", key.pItem, "");
        const auto it = Settings::values.linkage.by_key.find(name);
        if (it == Settings::values.linkage.by_key.end()) {
            LOG_WARNING(Config, "  {}: unknown setting, ignored", name);
            continue;
        }
        Settings::BasicSetting& setting = *it->second;
        if (!setting.Switchable()) {
            LOG_WARNING(Config, "  {}: not a per-game setting, ignored", name);
            continue;
        }
        if (!setting.UsingGlobal()) {
            LOG_INFO(Config, "  {}: kept per-game config value {}", name, setting.Canonicalize());
            continue;
        }
        setting.SetGlobal(false);
        if (!LoadValue(setting, value)) {
            setting.SetGlobal(true);
            LOG_WARNING(Config, "  {}: invalid value '{}', ignored", name, value);
            continue;
        }
        LOG_INFO(Config, "  {} = {}", name, setting.Canonicalize());
    }

    // Toggles that are read from the environment (EDEN_NX2_*). Set ones win over the profile.
    keys.clear();
    ini.GetAllKeys("Environment", keys);
    keys.sort(CSimpleIniA::Entry::LoadOrder());
    for (const auto& key : keys) {
        const std::string name = key.pItem;
        const char* value = ini.GetValue("Environment", key.pItem, "");
        if (!name.starts_with("EDEN_")) {
            LOG_WARNING(Config, "  {}: only EDEN_* variables can be set, ignored", name);
            continue;
        }
        if (const char* current = std::getenv(name.c_str())) {
            LOG_INFO(Config, "  {}: kept environment value {}", name, current);
            continue;
        }
        SetEnvironment(name.c_str(), value);
        LOG_INFO(Config, "  {} = {}", name, value);
    }
}

} // namespace GameProfile
