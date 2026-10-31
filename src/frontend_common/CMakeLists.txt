# SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: 2023 yuzu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

add_library(frontend_common STATIC
    config.cpp
    config.h
    content_manager.h
    firmware_manager.h
    firmware_manager.cpp
    data_manager.h data_manager.cpp
    play_time_manager.cpp
    play_time_manager.h
    settings_generator.h settings_generator.cpp
    mod_manager.h mod_manager.cpp
    game_profile.h game_profile.cpp)

set(BUNDLED_GAME_PROFILE_DIR ${CMAKE_SOURCE_DIR}/dist/game_profiles)
set(BUNDLED_GAME_PROFILE_HEADER ${CMAKE_CURRENT_BINARY_DIR}/frontend_common/bundled_game_profiles.h)
file(GLOB BUNDLED_GAME_PROFILE_FILES CONFIGURE_DEPENDS ${BUNDLED_GAME_PROFILE_DIR}/*.ini)
add_custom_command(
    OUTPUT ${BUNDLED_GAME_PROFILE_HEADER}
    COMMAND ${CMAKE_COMMAND} -P
        ${CMAKE_CURRENT_SOURCE_DIR}/GenerateBundledGameProfiles.cmake
        ${BUNDLED_GAME_PROFILE_DIR} ${BUNDLED_GAME_PROFILE_HEADER}
    DEPENDS ${BUNDLED_GAME_PROFILE_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/GenerateBundledGameProfiles.cmake
)
target_sources(frontend_common PRIVATE ${BUNDLED_GAME_PROFILE_HEADER})
target_include_directories(frontend_common PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

if (ENABLE_UPDATE_CHECKER)
    target_link_libraries(frontend_common PRIVATE httplib::httplib)
    target_link_libraries(frontend_common PRIVATE nlohmann_json::nlohmann_json)
    target_sources(frontend_common PRIVATE
        update_checker.cpp
        update_checker.h)
    target_link_libraries(frontend_common PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()

create_target_directory_groups(frontend_common)
target_link_libraries(frontend_common
    PUBLIC core SimpleIni::SimpleIni frozen::frozen-headers
    PRIVATE common Boost::headers)
