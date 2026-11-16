// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "ui/main_window.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

namespace tincan::ui {

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("Tincan"));
    resize(300, 480);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    auto *placeholder = new QLabel(tr("Tincan"), central);
    placeholder->setAlignment(Qt::AlignCenter);
    layout->addWidget(placeholder);
    setCentralWidget(central);
}

} // namespace tincan::ui
