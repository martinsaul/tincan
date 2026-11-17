// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "ui/contact_dialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

namespace tincan::ui {

namespace {

QString accountLabel(const AccountConfig &acc) {
    if (!acc.displayName.trimmed().isEmpty()) {
        return acc.displayName.trimmed();
    }
    if (!acc.user.trimmed().isEmpty()) {
        return QStringLiteral("%1@%2").arg(acc.user.trimmed(), acc.domain.trimmed());
    }
    return acc.domain.trimmed();
}

} // namespace

ContactDialog::ContactDialog(const QList<AccountConfig> &accounts, QWidget *parent)
    : QDialog(parent) {
    setupUi(accounts);
    setWindowTitle(tr("Add Contact"));
    populate(Contact{});
}

ContactDialog::ContactDialog(const Contact &initial, const QList<AccountConfig> &accounts,
                             QWidget *parent)
    : QDialog(parent), initial_(initial) {
    setupUi(accounts);
    setWindowTitle(tr("Edit Contact"));
    populate(initial);
}

void ContactDialog::setupUi(const QList<AccountConfig> &accounts) {
    setObjectName(QStringLiteral("contactDialog"));
    resize(360, 260);

    auto *root = new QVBoxLayout(this);
    auto *form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    nameEdit_ = new QLineEdit(this);
    nameEdit_->setObjectName(QStringLiteral("contactNameEdit"));

    numberEdit_ = new QLineEdit(this);
    numberEdit_->setObjectName(QStringLiteral("contactNumberEdit"));
    numberEdit_->setPlaceholderText(tr("Extension, number, or SIP URI"));

    accountCombo_ = new QComboBox(this);
    accountCombo_->setObjectName(QStringLiteral("contactAccountCombo"));
    accountCombo_->addItem(tr("Default account"), QString{});
    for (const AccountConfig &acc : accounts) {
        accountCombo_->addItem(accountLabel(acc), acc.id);
    }

    presenceCheck_ = new QCheckBox(tr("Subscribe to presence (BLF)"), this);
    presenceCheck_->setObjectName(QStringLiteral("contactPresenceCheck"));

    notesEdit_ = new QLineEdit(this);
    notesEdit_->setObjectName(QStringLiteral("contactNotesEdit"));

    form->addRow(tr("Name:"), nameEdit_);
    form->addRow(tr("Number:"), numberEdit_);
    form->addRow(tr("Account:"), accountCombo_);
    form->addRow(QString{}, presenceCheck_);
    form->addRow(tr("Notes:"), notesEdit_);
    root->addLayout(form);

    validationLabel_ = new QLabel(this);
    validationLabel_->setStyleSheet(QStringLiteral("color: #dc2626;"));
    validationLabel_->setVisible(false);
    root->addWidget(validationLabel_);

    root->addStretch();

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &ContactDialog::onAcceptClicked);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);
}

void ContactDialog::populate(const Contact &initial) {
    nameEdit_->setText(initial.name);
    numberEdit_->setText(initial.number);
    const int idx = accountCombo_->findData(initial.accountId);
    accountCombo_->setCurrentIndex(idx >= 0 ? idx : 0);
    presenceCheck_->setChecked(initial.presence);
    notesEdit_->setText(initial.notes);
}

Contact ContactDialog::contact() const {
    Contact out = initial_;
    out.name = nameEdit_->text().trimmed();
    out.number = numberEdit_->text().trimmed();
    out.accountId = accountCombo_->currentData().toString();
    out.presence = presenceCheck_->isChecked();
    out.notes = notesEdit_->text().trimmed();
    return out;
}

void ContactDialog::onAcceptClicked() {
    if (nameEdit_->text().trimmed().isEmpty()) {
        validationLabel_->setText(tr("Name is required."));
        validationLabel_->setVisible(true);
        nameEdit_->setFocus();
        return;
    }
    if (numberEdit_->text().trimmed().isEmpty()) {
        validationLabel_->setText(tr("Number or SIP URI is required."));
        validationLabel_->setVisible(true);
        numberEdit_->setFocus();
        return;
    }
    accept();
}

} // namespace tincan::ui
