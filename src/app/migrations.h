// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include <QSqlDatabase>
#include <QString>

namespace tincan::Migrations {

int latestVersion();
int currentVersion(const QSqlDatabase &db);
bool apply(QSqlDatabase db, QString *error = nullptr);

} // namespace tincan::Migrations
