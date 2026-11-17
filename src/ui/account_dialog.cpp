// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "ui/account_dialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QSpinBox>
#include <QUuid>
#include <QVBoxLayout>

namespace tincan::ui {

AccountDialog::AccountDialog(QWidget *parent) : QDialog(parent) {
    setupUi();
    setWindowTitle(tr("Add SIP Account"));
    populate(AccountConfig{});
}

AccountDialog::AccountDialog(const AccountConfig &initial, QWidget *parent) : QDialog(parent) {
    setupUi();
    setWindowTitle(tr("Edit SIP Account"));
    populate(initial);
}

void AccountDialog::setupUi() {
    setObjectName(QStringLiteral("accountDialog"));
    resize(420, 480);

    auto *root = new QVBoxLayout(this);
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto *container = new QWidget(scroll);
    auto *form = new QFormLayout(container);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    buildIdentityFields(form);
    buildNetworkFields(form);
    scroll->setWidget(container);
    root->addWidget(scroll, 1);

    validationLabel_ = new QLabel(this);
    validationLabel_->setStyleSheet(QStringLiteral("color: #dc2626;"));
    validationLabel_->setVisible(false);
    root->addWidget(validationLabel_);

    auto *buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &AccountDialog::onAcceptClicked);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);
}

void AccountDialog::buildIdentityFields(QFormLayout *form) {
    displayNameEdit_ = new QLineEdit(this);
    displayNameEdit_->setObjectName(QStringLiteral("displayNameEdit"));
    userEdit_ = new QLineEdit(this);
    userEdit_->setObjectName(QStringLiteral("userEdit"));
    domainEdit_ = new QLineEdit(this);
    domainEdit_->setObjectName(QStringLiteral("domainEdit"));
    authUserEdit_ = new QLineEdit(this);
    authUserEdit_->setObjectName(QStringLiteral("authUserEdit"));
    passwordEdit_ = new QLineEdit(this);
    passwordEdit_->setObjectName(QStringLiteral("passwordEdit"));
    passwordEdit_->setEchoMode(QLineEdit::Password);
    proxyEdit_ = new QLineEdit(this);
    proxyEdit_->setObjectName(QStringLiteral("proxyEdit"));

    form->addRow(tr("Display name:"), displayNameEdit_);
    form->addRow(tr("Username:"), userEdit_);
    form->addRow(tr("Domain / Server:"), domainEdit_);
    form->addRow(tr("Auth username:"), authUserEdit_);
    form->addRow(tr("Password:"), passwordEdit_);
    form->addRow(tr("Outbound proxy:"), proxyEdit_);
}

void AccountDialog::buildNetworkFields(QFormLayout *form) {
    transportCombo_ = new QComboBox(this);
    transportCombo_->setObjectName(QStringLiteral("transportCombo"));
    transportCombo_->addItem(tr("UDP"), static_cast<int>(Transport::Udp));
    transportCombo_->addItem(tr("TCP"), static_cast<int>(Transport::Tcp));
    transportCombo_->addItem(tr("TLS"), static_cast<int>(Transport::Tls));

    srtpCombo_ = new QComboBox(this);
    srtpCombo_->setObjectName(QStringLiteral("srtpCombo"));
    srtpCombo_->addItem(tr("Disabled"), static_cast<int>(SrtpMode::Disabled));
    srtpCombo_->addItem(tr("Optional"), static_cast<int>(SrtpMode::Optional));
    srtpCombo_->addItem(tr("Mandatory"), static_cast<int>(SrtpMode::Mandatory));

    registerCheck_ = new QCheckBox(tr("Register with domain"), this);
    registerCheck_->setObjectName(QStringLiteral("registerCheck"));

    regExpirySpin_ = new QSpinBox(this);
    regExpirySpin_->setObjectName(QStringLiteral("regExpirySpin"));
    regExpirySpin_->setRange(30, 86400);
    regExpirySpin_->setSuffix(tr(" s"));

    stunServerEdit_ = new QLineEdit(this);
    stunServerEdit_->setObjectName(QStringLiteral("stunServerEdit"));
    iceCheck_ = new QCheckBox(tr("Enable ICE"), this);
    iceCheck_->setObjectName(QStringLiteral("iceCheck"));
    publicAddressEdit_ = new QLineEdit(this);
    publicAddressEdit_->setObjectName(QStringLiteral("publicAddressEdit"));
    dialPrefixEdit_ = new QLineEdit(this);
    dialPrefixEdit_->setObjectName(QStringLiteral("dialPrefixEdit"));
    voicemailEdit_ = new QLineEdit(this);
    voicemailEdit_->setObjectName(QStringLiteral("voicemailEdit"));

    form->addRow(tr("Transport:"), transportCombo_);
    form->addRow(tr("Media encryption (SRTP):"), srtpCombo_);
    form->addRow(QString{}, registerCheck_);
    form->addRow(tr("Registration expiry:"), regExpirySpin_);
    form->addRow(tr("STUN server:"), stunServerEdit_);
    form->addRow(QString{}, iceCheck_);
    form->addRow(tr("Public address:"), publicAddressEdit_);
    form->addRow(tr("Dial prefix:"), dialPrefixEdit_);
    form->addRow(tr("Voicemail number:"), voicemailEdit_);
}

void AccountDialog::populate(const AccountConfig &cfg) {
    accountId_ = cfg.id;
    displayNameEdit_->setText(cfg.displayName);
    userEdit_->setText(cfg.user);
    domainEdit_->setText(cfg.domain);
    authUserEdit_->setText(cfg.authUser);
    passwordEdit_->setText(cfg.password);
    passwordEdit_->setModified(false);
    proxyEdit_->setText(cfg.proxy);

    const int tpIdx = transportCombo_->findData(static_cast<int>(cfg.transport));
    transportCombo_->setCurrentIndex(tpIdx >= 0 ? tpIdx : 0);

    const int srtpIdx = srtpCombo_->findData(static_cast<int>(cfg.srtp));
    srtpCombo_->setCurrentIndex(srtpIdx >= 0 ? srtpIdx : 0);

    registerCheck_->setChecked(cfg.registerAccount);
    regExpirySpin_->setValue(cfg.regExpirySec > 0 ? cfg.regExpirySec : 300);
    stunServerEdit_->setText(cfg.stunServer);
    iceCheck_->setChecked(cfg.ice);
    publicAddressEdit_->setText(cfg.publicAddress);
    dialPrefixEdit_->setText(cfg.dialPrefix);
    voicemailEdit_->setText(cfg.voicemailNumber);
}

void AccountDialog::setPasswordFieldIfUnmodified(const QString &password) {
    if (!passwordEdit_->isModified() && passwordEdit_->text().isEmpty()) {
        passwordEdit_->setText(password);
        passwordEdit_->setModified(false);
    }
}

AccountConfig AccountDialog::accountConfig() const {
    AccountConfig cfg;
    cfg.id = accountId_.trimmed().isEmpty()
                 ? QUuid::createUuid().toString(QUuid::WithoutBraces)
                 : accountId_;
    cfg.displayName = displayNameEdit_->text().trimmed();
    cfg.user = userEdit_->text().trimmed();
    cfg.domain = domainEdit_->text().trimmed();
    cfg.authUser = authUserEdit_->text().trimmed();
    cfg.password = passwordEdit_->text();
    cfg.proxy = proxyEdit_->text().trimmed();
    cfg.transport = static_cast<Transport>(transportCombo_->currentData().toInt());
    cfg.srtp = static_cast<SrtpMode>(srtpCombo_->currentData().toInt());
    cfg.registerAccount = registerCheck_->isChecked();
    cfg.regExpirySec = regExpirySpin_->value();
    cfg.stunServer = stunServerEdit_->text().trimmed();
    cfg.ice = iceCheck_->isChecked();
    cfg.publicAddress = publicAddressEdit_->text().trimmed();
    cfg.dialPrefix = dialPrefixEdit_->text().trimmed();
    cfg.voicemailNumber = voicemailEdit_->text().trimmed();
    return cfg;
}

void AccountDialog::onAcceptClicked() {
    if (domainEdit_->text().trimmed().isEmpty()) {
        validationLabel_->setText(tr("Domain / Server is required."));
        validationLabel_->setVisible(true);
        domainEdit_->setFocus();
        return;
    }
    accept();
}

} // namespace tincan::ui
