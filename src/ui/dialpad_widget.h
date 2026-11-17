// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include <QWidget>

class QGridLayout;
class QKeyEvent;
class QLineEdit;
class QPushButton;
class QToolButton;

namespace tincan::ui {

class DialpadWidget : public QWidget {
    Q_OBJECT

public:
    explicit DialpadWidget(QWidget *parent = nullptr);

    QString text() const;
    void setText(const QString &value);
    void clearText();
    QLineEdit *numberEdit() const;
    QPushButton *callButton() const;

signals:
    void dialRequested(const QString &number);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void buildKeyGrid(QGridLayout *grid);
    QPushButton *createKeyButton(const QString &digit, const QString &letters);
    void appendDigit(const QString &digit);
    void backspace();
    void triggerDial();

    QLineEdit *numberEdit_ = nullptr;
    QToolButton *backspaceButton_ = nullptr;
    QPushButton *callButton_ = nullptr;
};

} // namespace tincan::ui
