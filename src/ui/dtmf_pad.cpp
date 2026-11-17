// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "ui/dtmf_pad.h"

#include <QGridLayout>
#include <QPushButton>

#include <array>
#include <cstddef>

namespace tincan::ui {

DtmfPad::DtmfPad(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("dtmfPad"));
    auto *grid = new QGridLayout(this);
    grid->setContentsMargins(0, 2, 0, 2);
    grid->setSpacing(4);

    static constexpr std::array<const char *, 12> digits = {
        "1", "2", "3", "4", "5", "6", "7", "8", "9", "*", "0", "#",
    };

    for (std::size_t i = 0; i < digits.size(); ++i) {
        const QString d = QString::fromLatin1(digits[i]);
        auto *btn = new QPushButton(d, this);
        btn->setObjectName(QStringLiteral("dtmfKey_") + d);
        btn->setMinimumHeight(26);
        btn->setFocusPolicy(Qt::NoFocus);
        btn->setStyleSheet(QStringLiteral(
            "QPushButton { background-color: #ffffff; color: #0f172a; border: 1px solid #cbd5e1; "
            "border-radius: 4px; font-weight: bold; padding: 2px 6px; }"
            "QPushButton:hover { background-color: #e2e8f0; }"));
        connect(btn, &QPushButton::clicked, this, [this, d]() { emit digitPressed(d); });
        grid->addWidget(btn, static_cast<int>(i / 3), static_cast<int>(i % 3));
    }
}

} // namespace tincan::ui
