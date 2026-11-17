// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include <QDialog>
#include <QString>

class QLabel;

namespace tincan::ui {

// Shows the version, licence (with the source offer) and credits.
class AboutDialog : public QDialog {
    Q_OBJECT

public:
    explicit AboutDialog(const QString &pjsipVersion, QWidget *parent = nullptr);

    // Plain HTML shown in the dialog body; exposed for tests.
    QString bodyHtml() const;

private:
    QString versionsHtml(const QString &pjsipVersion) const;
    QString licenseHtml() const;
    QString creditsHtml() const;

    QLabel *body_ = nullptr;
};

} // namespace tincan::ui
