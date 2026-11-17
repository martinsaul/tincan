// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include <QWidget>

namespace tincan::ui {

class DtmfPad : public QWidget {
    Q_OBJECT

public:
    explicit DtmfPad(QWidget *parent = nullptr);

signals:
    void digitPressed(const QString &digit);
};

} // namespace tincan::ui
