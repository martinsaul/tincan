// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/contact_csv.h"

#include "core/sip_uri.h"

#include <QSet>
#include <QStringList>

namespace tincan::ContactCsv {

namespace {

QString quoteCsvField(const QString &field) {
    bool needsQuotes = false;
    for (const QChar ch : field) {
        if (ch == u',' || ch == u'"' || ch == u'\r' || ch == u'\n') {
            needsQuotes = true;
            break;
        }
    }
    if (!needsQuotes) {
        return field;
    }

    QString out;
    out.reserve(field.size() + 4);
    out.append(u'"');
    for (const QChar ch : field) {
        if (ch == u'"') {
            out.append(QStringLiteral("\"\""));
        } else {
            out.append(ch);
        }
    }
    out.append(u'"');
    return out;
}

void stepQuotedField(const QString &input, qsizetype &i, QString &field, bool &inQuotes) {
    if (input[i] == u'"') {
        if (i + 1 < input.size() && input[i + 1] == u'"') {
            field.append(u'"');
            i += 2;
            return;
        }
        inQuotes = false;
    } else {
        field.append(input[i]);
    }
    ++i;
}

QList<QStringList> tokenizeRfc4180(const QString &input) {
    QList<QStringList> rows;
    QStringList currentRow;
    QString currentField;
    bool inQuotes = false;

    qsizetype i = (!input.isEmpty() && input.front() == QChar(0xFEFF)) ? 1 : 0;
    while (i < input.size()) {
        if (inQuotes) {
            stepQuotedField(input, i, currentField, inQuotes);
            continue;
        }
        const QChar ch = input[i];
        if (ch == u'"' && currentField.isEmpty()) {
            inQuotes = true;
        } else if (ch == u',') {
            currentRow.append(currentField);
            currentField.clear();
        } else if (ch == u'\r' || ch == u'\n') {
            if (ch == u'\r' && i + 1 < input.size() && input[i + 1] == u'\n') {
                ++i;
            }
            currentRow.append(currentField);
            currentField.clear();
            rows.append(currentRow);
            currentRow.clear();
        } else {
            currentField.append(ch);
        }
        ++i;
    }

    if (!currentField.isEmpty() || !currentRow.isEmpty()) {
        currentRow.append(currentField);
        rows.append(currentRow);
    }
    return rows;
}

bool isBlankRow(const QStringList &row) {
    return row.size() == 1 && row.first().trimmed().isEmpty();
}

bool isHeaderRow(const QStringList &row) {
    return row.size() >= 2 &&
           row[0].trimmed().compare(QStringLiteral("name"), Qt::CaseInsensitive) == 0 &&
           row[1].trimmed().compare(QStringLiteral("number"), Qt::CaseInsensitive) == 0;
}

bool parsePresenceFlag(const QString &value) {
    const QString lower = value.trimmed().toLower();
    return lower == QStringLiteral("1") || lower == QStringLiteral("true") ||
           lower == QStringLiteral("yes");
}

} // namespace

QString exportContacts(const QList<Contact> &contacts) {
    QString out = QStringLiteral("name,number,presence\r\n");
    for (const Contact &c : contacts) {
        out.append(quoteCsvField(c.name));
        out.append(u',');
        out.append(quoteCsvField(c.number));
        out.append(u',');
        out.append(c.presence ? QLatin1Char('1') : QLatin1Char('0'));
        out.append(QStringLiteral("\r\n"));
    }
    return out;
}

ImportResult parseContacts(const QString &csvText, const QList<Contact> &existingContacts) {
    ImportResult result;
    QSet<QString> seenKeys;
    for (const Contact &existing : existingContacts) {
        const QString key = SipUri::normalizeContactKey(existing.number);
        if (!key.isEmpty()) {
            seenKeys.insert(key);
        }
    }

    const QList<QStringList> rows = tokenizeRfc4180(csvText);
    bool checkedFirstDataRow = false;

    for (const QStringList &row : rows) {
        if (isBlankRow(row)) {
            continue;
        }
        if (!checkedFirstDataRow) {
            checkedFirstDataRow = true;
            if (isHeaderRow(row)) {
                continue;
            }
        }
        if (row.size() < 2) {
            ++result.skippedInvalid;
            continue;
        }

        const QString name = row[0].trimmed();
        const QString number = row[1].trimmed();
        const QString key = SipUri::normalizeContactKey(number);
        if (name.isEmpty() || number.isEmpty() || key.isEmpty()) {
            ++result.skippedInvalid;
            continue;
        }
        if (seenKeys.contains(key)) {
            ++result.skippedDuplicates;
            continue;
        }

        seenKeys.insert(key);
        Contact c;
        c.name = name;
        c.number = number;
        c.presence = row.size() >= 3 && parsePresenceFlag(row[2]);
        result.contacts.append(c);
        ++result.imported;
    }

    return result;
}

} // namespace tincan::ContactCsv
