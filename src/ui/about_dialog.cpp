// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "ui/about_dialog.h"

#include "core/version.h"
#include "ui/icons.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QtGlobal>

namespace tincan::ui {

namespace {

QString link(const QString &url, const QString &text = {}) {
    return QStringLiteral("<a href=\"%1\">%2</a>").arg(url, text.isEmpty() ? url : text);
}

} // namespace

AboutDialog::AboutDialog(const QString &pjsipVersion, QWidget *parent) : QDialog(parent) {
    setObjectName(QStringLiteral("aboutDialog"));
    setWindowTitle(tr("About Tincan"));

    auto *icon = new QLabel(this);
    icon->setPixmap(Icons::appIcon().pixmap(64, 64));
    icon->setAlignment(Qt::AlignTop);

    body_ = new QLabel(this);
    body_->setObjectName(QStringLiteral("aboutBody"));
    body_->setTextFormat(Qt::RichText);
    body_->setWordWrap(true);
    body_->setOpenExternalLinks(true);
    body_->setTextInteractionFlags(Qt::TextBrowserInteraction);
    body_->setMinimumWidth(340);
    body_->setText(versionsHtml(pjsipVersion) + licenseHtml() + creditsHtml());

    auto *row = new QHBoxLayout();
    row->setSpacing(12);
    row->addWidget(icon);
    row->addWidget(body_, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *root = new QVBoxLayout(this);
    root->addLayout(row);
    root->addWidget(buttons);
}

QString AboutDialog::bodyHtml() const { return body_->text(); }

QString AboutDialog::versionsHtml(const QString &pjsipVersion) const {
    const QString pj = pjsipVersion.isEmpty() ? tr("unknown") : pjsipVersion;
    return QStringLiteral("<h3>%1</h3><p>%2</p><p>%3</p>")
        .arg(tr("Tincan %1").arg(Version::string()),
             tr("A lightweight SIP softphone for Linux."),
             tr("PJSIP %1 &middot; Qt %2").arg(pj, QString::fromLatin1(qVersion())));
}

QString AboutDialog::licenseHtml() const {
    return QStringLiteral("<p>%1</p><p>%2</p>")
        .arg(tr("Tincan is free software: you can redistribute it and/or modify it under the "
                "terms of the GNU Affero General Public License, version 3 or (at your option) "
                "any later version (%1). It comes with ABSOLUTELY NO WARRANTY.")
                 .arg(link(QStringLiteral("https://www.gnu.org/licenses/agpl-3.0.html"),
                           QStringLiteral("AGPL-3.0-or-later"))),
             tr("Source code: %1").arg(link(Version::sourceUrl())));
}

QString AboutDialog::creditsHtml() const {
    const QString items =
        QStringLiteral("<li>%1</li><li>%2</li><li>%3</li><li>%4</li>")
            .arg(tr("PJSIP (%1), GPL-2.0-or-later")
                     .arg(link(QStringLiteral("https://www.pjsip.org"), QStringLiteral("pjsip.org"))),
                 tr("Qt 6 (%1), LGPL-3.0")
                     .arg(link(QStringLiteral("https://www.qt.io"), QStringLiteral("qt.io"))),
                 tr("QtKeychain, BSD-2-Clause"), tr("Opus audio codec, BSD-3-Clause"));
    return QStringLiteral("<p><b>%1</b></p><ul>%2</ul><p>%3</p>")
        .arg(tr("Built with"), items,
             tr("Inspired by MicroSIP (%1); not affiliated.")
                 .arg(link(QStringLiteral("https://www.microsip.org"))));
}

} // namespace tincan::ui
