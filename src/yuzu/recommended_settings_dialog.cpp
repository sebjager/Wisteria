// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "common/logging.h"
#include "common/settings.h"
#include "qt_common/config/shared_translation.h"
#include "yuzu/recommended_settings_dialog.h"

namespace Recommendations = Settings::Recommendations;

namespace {

Settings::BasicSetting* FindSetting(const std::string& key) {
    const auto& by_key = Settings::values.linkage.by_key;
    const auto it = by_key.find(key);
    return it != by_key.end() ? it->second : nullptr;
}

} // Anonymous namespace

std::vector<Recommendations::Recommendation> RecommendedSettingsDialog::PendingFor(
    const Recommendations::Result& result) {
    std::vector<Recommendations::Recommendation> pending;
    for (const auto& recommendation : result.settings) {
        const Settings::BasicSetting* setting = FindSetting(recommendation.setting);
        if (setting != nullptr && setting->ToStringGlobal() != recommendation.value) {
            pending.push_back(recommendation);
        }
    }
    return pending;
}

RecommendedSettingsDialog::RecommendedSettingsDialog(QWidget* parent,
                                                     const Recommendations::HostHardware& hardware)
    : QDialog(parent) {
    setWindowTitle(tr("Recommended Settings"));
    setMinimumWidth(520);

    const Recommendations::Result result = Recommendations::RecommendOffered(hardware);
    const auto translations = ConfigurationShared::InitializeTranslations(this);

    auto* layout = new QVBoxLayout(this);
    auto* hardware_label =
        new QLabel(tr("Detected hardware: %1")
                       .arg(QString::fromStdString(Recommendations::DescribeHardware(hardware))),
                   this);
    hardware_label->setWordWrap(true);
    layout->addWidget(hardware_label);

    for (const auto& recommendation : PendingFor(result)) {
        const Settings::BasicSetting* setting = FindSetting(recommendation.setting);
        QString label = QString::fromStdString(recommendation.setting);
        if (const auto it = translations->find(setting->Id()); it != translations->end()) {
            label = it->second.first;
            if (label.endsWith(QLatin1Char(':'))) {
                label.chop(1);
            }
        }
        auto* check_box =
            new QCheckBox(tr("%1: %2 → %3")
                              .arg(label, QString::fromStdString(setting->Canonicalize()),
                                   QString::fromStdString(recommendation.value_name)),
                          this);
        check_box->setChecked(true);
        auto* reason = new QLabel(QString::fromStdString(recommendation.reason), this);
        reason->setWordWrap(true);
        reason->setContentsMargins(24, 0, 0, 8);
        layout->addWidget(check_box);
        layout->addWidget(reason);
        rows.push_back(Row{recommendation, check_box});
    }

    if (rows.empty()) {
        auto* none = new QLabel(
            tr("Your settings already match the recommendations for this hardware."), this);
        none->setWordWrap(true);
        layout->addWidget(none);
    }

    for (const auto& notice : result.notices) {
        auto* notice_label = new QLabel(QString::fromStdString(notice), this);
        notice_label->setWordWrap(true);
        notice_label->setStyleSheet(QStringLiteral("font-style: italic;"));
        layout->addWidget(notice_label);
    }

    auto* footer = new QLabel(
        tr("Only settings that have been tested on real hardware are recommended. Per-game "
           "profiles and per-game settings still take precedence."),
        this);
    footer->setWordWrap(true);
    layout->addWidget(footer);

    auto* buttons = new QDialogButtonBox(this);
    if (rows.empty()) {
        buttons->addButton(QDialogButtonBox::Close);
    } else {
        buttons->addButton(tr("Apply"), QDialogButtonBox::AcceptRole);
        buttons->addButton(tr("Not Now"), QDialogButtonBox::RejectRole);
    }
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

RecommendedSettingsDialog::~RecommendedSettingsDialog() = default;

size_t RecommendedSettingsDialog::ApplySelected() {
    size_t changed = 0;
    for (const Row& row : rows) {
        if (!row.check_box->isChecked()) {
            continue;
        }
        Settings::BasicSetting* setting = FindSetting(row.recommendation.setting);
        if (setting == nullptr || !setting->UsingGlobal()) {
            continue;
        }
        setting->LoadString(row.recommendation.value);
        LOG_INFO(Frontend, "Applied recommended setting {} = {} (rule {})",
                 row.recommendation.setting, row.recommendation.value_name, row.recommendation.id);
        ++changed;
    }
    return changed;
}
