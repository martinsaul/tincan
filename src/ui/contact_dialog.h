// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "core/account_config.h"
#include "core/contact.h"

#include <QDialog>
#include <QList>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;

namespace tincan::ui {

class ContactDialog : public QDialog {
    Q_OBJECT

public:
    explicit ContactDialog(const QList<AccountConfig> &accounts, QWidget *parent = nullptr);
    ContactDialog(const Contact &initial, const QList<AccountConfig> &accounts,
                  QWidget *parent = nullptr);

    Contact contact() const;

private:
    void setupUi(const QList<AccountConfig> &accounts);
    void populate(const Contact &initial);
    void onAcceptClicked();

    Contact initial_;
    QLineEdit *nameEdit_ = nullptr;
    QLineEdit *numberEdit_ = nullptr;
    QComboBox *accountCombo_ = nullptr;
    QCheckBox *presenceCheck_ = nullptr;
    QLineEdit *notesEdit_ = nullptr;
    QLabel *validationLabel_ = nullptr;
};

} // namespace tincan::ui
