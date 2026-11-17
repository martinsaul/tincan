// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "ui/dialpad_widget.h"

#include "ui/icons.h"

#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSizePolicy>
#include <QToolButton>
#include <QVBoxLayout>

#include <array>
#include <utility>

namespace tincan::ui {

DialpadWidget::DialpadWidget(QWidget *parent) : QWidget(parent) {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(10);

    auto *inputRow = new QHBoxLayout();
    inputRow->setSpacing(6);

    numberEdit_ = new QLineEdit(this);
    numberEdit_->setObjectName(QStringLiteral("numberEdit"));
    numberEdit_->setPlaceholderText(tr("Number or SIP URI"));
    QFont inputFont = numberEdit_->font();
    inputFont.setPointSize(inputFont.pointSize() + 2);
    numberEdit_->setFont(inputFont);
    numberEdit_->setMinimumHeight(36);
    connect(numberEdit_, &QLineEdit::returnPressed, this, &DialpadWidget::triggerDial);

    backspaceButton_ = new QToolButton(this);
    backspaceButton_->setObjectName(QStringLiteral("backspaceButton"));
    backspaceButton_->setIcon(Icons::backspace());
    backspaceButton_->setToolTip(tr("Backspace"));
    backspaceButton_->setFixedSize(36, 36);
    connect(backspaceButton_, &QToolButton::clicked, this, &DialpadWidget::backspace);

    inputRow->addWidget(numberEdit_, 1);
    inputRow->addWidget(backspaceButton_);
    root->addLayout(inputRow);

    auto *grid = new QGridLayout();
    grid->setSpacing(8);
    buildKeyGrid(grid);
    root->addLayout(grid, 1);

    callButton_ = new QPushButton(Icons::callStart(), tr("Call"), this);
    callButton_->setObjectName(QStringLiteral("callButton"));
    callButton_->setMinimumHeight(44);
    callButton_->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #16a34a; color: #ffffff; font-weight: bold; "
        "font-size: 14px; border-radius: 6px; padding: 6px 12px; }"
        "QPushButton:hover { background-color: #15803d; }"
        "QPushButton:pressed { background-color: #166534; }"));
    connect(callButton_, &QPushButton::clicked, this, &DialpadWidget::triggerDial);
    root->addWidget(callButton_);
}

void DialpadWidget::buildKeyGrid(QGridLayout *grid) {
    struct KeySpec {
        const char *digit;
        const char *letters;
    };
    static constexpr std::array<KeySpec, 12> keys = {{
        {"1", ""},
        {"2", "ABC"},
        {"3", "DEF"},
        {"4", "GHI"},
        {"5", "JKL"},
        {"6", "MNO"},
        {"7", "PQRS"},
        {"8", "TUV"},
        {"9", "WXYZ"},
        {"*", ""},
        {"0", "+"},
        {"#", ""},
    }};

    for (std::size_t i = 0; i < keys.size(); ++i) {
        const QString d = QString::fromLatin1(keys[i].digit);
        const QString l = QString::fromLatin1(keys[i].letters);
        QPushButton *btn = createKeyButton(d, l);
        grid->addWidget(btn, static_cast<int>(i / 3), static_cast<int>(i % 3));
    }
}

QPushButton *DialpadWidget::createKeyButton(const QString &digit, const QString &letters) {
    auto *btn = new QPushButton(this);
    btn->setObjectName(QStringLiteral("dialKey_") + digit);
    btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    btn->setMinimumHeight(46);
    btn->setFocusPolicy(Qt::TabFocus);

    auto *vbox = new QVBoxLayout(btn);
    vbox->setContentsMargins(4, 4, 4, 4);
    vbox->setSpacing(0);

    auto *digitLabel = new QLabel(digit, btn);
    digitLabel->setAlignment(Qt::AlignCenter);
    digitLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    QFont digitFont = digitLabel->font();
    digitFont.setBold(true);
    digitFont.setPointSize(digitFont.pointSize() + 4);
    digitLabel->setFont(digitFont);

    auto *subLabel = new QLabel(letters, btn);
    subLabel->setAlignment(Qt::AlignCenter);
    subLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    QFont subFont = subLabel->font();
    subFont.setPointSize(std::max(7, subFont.pointSize() - 2));
    subLabel->setFont(subFont);
    subLabel->setStyleSheet(QStringLiteral("color: #64748b;"));

    vbox->addWidget(digitLabel);
    vbox->addWidget(subLabel);

    connect(btn, &QPushButton::clicked, this, [this, digit]() { appendDigit(digit); });
    return btn;
}

QString DialpadWidget::text() const {
    return numberEdit_->text();
}

void DialpadWidget::setText(const QString &value) {
    numberEdit_->setText(value);
}

void DialpadWidget::clearText() {
    numberEdit_->clear();
}

QLineEdit *DialpadWidget::numberEdit() const {
    return numberEdit_;
}

QPushButton *DialpadWidget::callButton() const {
    return callButton_;
}

void DialpadWidget::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        triggerDial();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Backspace) {
        backspace();
        event->accept();
        return;
    }
    const QString typed = event->text();
    if (typed.size() == 1) {
        const QChar ch = typed.front();
        if ((ch >= u'0' && ch <= u'9') || ch == u'*' || ch == u'#' || ch == u'+') {
            appendDigit(typed);
            event->accept();
            return;
        }
    }
    QWidget::keyPressEvent(event);
}

void DialpadWidget::appendDigit(const QString &digit) {
    numberEdit_->insert(digit);
}

void DialpadWidget::backspace() {
    if (numberEdit_->hasSelectedText()) {
        numberEdit_->del();
        return;
    }
    numberEdit_->backspace();
}

void DialpadWidget::triggerDial() {
    const QString target = numberEdit_->text().trimmed();
    if (!target.isEmpty()) {
        emit dialRequested(target);
    }
}

} // namespace tincan::ui
