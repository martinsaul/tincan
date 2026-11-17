// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "core/contact.h"

#include <QList>
#include <QString>

namespace tincan::ContactCsv {

struct ImportResult {
    int imported = 0;
    int skippedDuplicates = 0;
    int skippedInvalid = 0;
    QList<Contact> contacts;
};

QString exportContacts(const QList<Contact> &contacts);
ImportResult parseContacts(const QString &csvText, const QList<Contact> &existingContacts = {});

} // namespace tincan::ContactCsv
