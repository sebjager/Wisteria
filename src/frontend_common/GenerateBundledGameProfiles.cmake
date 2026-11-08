# SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

set(PROFILE_DIR ${CMAKE_ARGV3})
set(HEADER_FILE ${CMAKE_ARGV4})

file(GLOB PROFILE_FILES ${PROFILE_DIR}/*.ini)
list(SORT PROFILE_FILES)
list(LENGTH PROFILE_FILES PROFILE_COUNT)

set(ENTRIES "")
foreach(PROFILE_FILE IN LISTS PROFILE_FILES)
    get_filename_component(PROFILE_NAME ${PROFILE_FILE} NAME)
    file(READ ${PROFILE_FILE} PROFILE_BODY)

    string(REGEX REPLACE ";" "{{SEMICOLON}}" PROFILE_BODY "${PROFILE_BODY}")
    string(REGEX REPLACE "\n" ";" PROFILE_BODY "${PROFILE_BODY}")

    set(PROFILE_TEXT "")
    foreach(LINE IN LISTS PROFILE_BODY)
        string(CONCAT PROFILE_TEXT "${PROFILE_TEXT}" "        R\"(${LINE}\n)\"\n")
    endforeach()
    string(REGEX REPLACE "{{SEMICOLON}}" ";" PROFILE_TEXT "${PROFILE_TEXT}")

    string(CONCAT ENTRIES "${ENTRIES}"
        "    BundledGameProfile{\n        \"${PROFILE_NAME}\",\n${PROFILE_TEXT}    },\n")
endforeach()

get_filename_component(OUTPUT_DIR ${HEADER_FILE} DIRECTORY)
make_directory(${OUTPUT_DIR})

file(WRITE ${HEADER_FILE}
"// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <string_view>

namespace GameProfile {

struct BundledGameProfile {
    std::string_view name;
    std::string_view source;
};

inline constexpr std::array<BundledGameProfile, ${PROFILE_COUNT}> BUNDLED_GAME_PROFILES{{
${ENTRIES}}};

} // namespace GameProfile
")
