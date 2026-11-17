// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "app/database.h"
#include "core/contact.h"

#include <QList>
#include <QString>

#include <optional>

namespace tincan {

class ContactRepository {
public:
    explicit ContactRepository(Database &database);

    qint64 insert(const Contact &contact);
    bool update(const Contact &contact);
    bool remove(qint64 id);
    std::optional<Contact> findById(qint64 id) const;
    QList<Contact> listAll() const;
    QList<Contact> search(const QString &queryText) const;

private:
    Database &database_;
};

} // namespace tincan
