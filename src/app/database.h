// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include <QSqlDatabase>
#include <QString>

namespace tincan {

class Database {
public:
    explicit Database(QString dbPath = {}, QString connectionName = {});
    ~Database();

    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    bool open(QString *error = nullptr);
    void close();
    bool isOpen() const;

    const QString &databasePath() const;
    const QString &connectionName() const;
    QSqlDatabase connection() const;

private:
    QString dbPath_;
    QString connectionName_;
};

} // namespace tincan
