// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>

#include <QDialog>

#include "common/settings_recommendations.h"

class QCheckBox;

// Shows the detected hardware and the recommended global settings that differ from the current
// ones, each with a checkbox, and applies the checked ones. Only verified rules are listed
// (Settings::Recommendations::RecommendOffered).
class RecommendedSettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit RecommendedSettingsDialog(QWidget* parent,
                                       const Settings::Recommendations::HostHardware& hardware);
    ~RecommendedSettingsDialog() override;

    /// Number of recommendations that differ from the current settings.
    [[nodiscard]] size_t PendingCount() const {
        return rows.size();
    }

    /// Writes the checked recommendations to the global settings. Returns how many changed.
    size_t ApplySelected();

    /// Recommendations for this hardware that differ from the current global settings.
    [[nodiscard]] static std::vector<Settings::Recommendations::Recommendation> PendingFor(
        const Settings::Recommendations::Result& result);

private:
    struct Row {
        Settings::Recommendations::Recommendation recommendation;
        QCheckBox* check_box;
    };
    std::vector<Row> rows;
};
