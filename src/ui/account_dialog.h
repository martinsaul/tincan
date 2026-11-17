// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "core/account_config.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QSpinBox;

namespace tincan::ui {

class AccountDialog : public QDialog {
    Q_OBJECT

public:
    explicit AccountDialog(QWidget *parent = nullptr);
    AccountDialog(const AccountConfig &initial, QWidget *parent = nullptr);

    void setPasswordFieldIfUnmodified(const QString &password);
    AccountConfig accountConfig() const;

private:
    void setupUi();
    void buildIdentityFields(QFormLayout *form);
    void buildNetworkFields(QFormLayout *form);
    void populate(const AccountConfig &cfg);
    void onAcceptClicked();

    QString accountId_;
    QLineEdit *displayNameEdit_ = nullptr;
    QLineEdit *userEdit_ = nullptr;
    QLineEdit *domainEdit_ = nullptr;
    QLineEdit *authUserEdit_ = nullptr;
    QLineEdit *passwordEdit_ = nullptr;
    QLineEdit *proxyEdit_ = nullptr;
    QComboBox *transportCombo_ = nullptr;
    QComboBox *srtpCombo_ = nullptr;
    QCheckBox *registerCheck_ = nullptr;
    QSpinBox *regExpirySpin_ = nullptr;
    QLineEdit *stunServerEdit_ = nullptr;
    QCheckBox *iceCheck_ = nullptr;
    QLineEdit *publicAddressEdit_ = nullptr;
    QLineEdit *dialPrefixEdit_ = nullptr;
    QLineEdit *voicemailEdit_ = nullptr;
    QCheckBox *publishPresenceCheck_ = nullptr;
    QLabel *validationLabel_ = nullptr;
};

} // namespace tincan::ui
