#include "EventStore.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>

#include <algorithm>

namespace colibry::calendar {

namespace {

constexpr int kMaxInterval = 999;
constexpr int kMaxTitle = 200;

QString tr(const char *text)
{
    return QCoreApplication::translate("colibry::calendar", text);
}

QString timeToString(const QTime &t)
{
    return t.isValid() ? t.toString(QStringLiteral("HH:mm")) : QString();
}

QTime timeFromString(const QString &s)
{
    return QTime::fromString(s, QStringLiteral("HH:mm"));
}

QJsonObject toJson(const Event &e)
{
    QJsonObject o;
    o.insert(QStringLiteral("id"), e.id);
    o.insert(QStringLiteral("title"), e.title);
    o.insert(QStringLiteral("notes"), e.notes);
    o.insert(QStringLiteral("date"), e.date.toString(Qt::ISODate));
    o.insert(QStringLiteral("allDay"), e.allDay);
    o.insert(QStringLiteral("start"), e.allDay ? QString() : timeToString(e.start));
    o.insert(QStringLiteral("end"), e.allDay ? QString() : timeToString(e.end));
    o.insert(QStringLiteral("repeat"), repeatToString(e.repeat));
    o.insert(QStringLiteral("interval"), e.interval);
    o.insert(QStringLiteral("until"), e.until.isValid() ? e.until.toString(Qt::ISODate) : QString());
    return o;
}

/// Reads one entry. Returns false for entries that cannot be used.
bool fromJson(const QJsonValue &value, Event *out)
{
    if (!value.isObject())
        return false;
    const QJsonObject o = value.toObject();
    Event e;
    e.id = o.value(QStringLiteral("id")).toString();
    e.title = o.value(QStringLiteral("title")).toString();
    e.notes = o.value(QStringLiteral("notes")).toString();
    e.date = QDate::fromString(o.value(QStringLiteral("date")).toString(), Qt::ISODate);
    e.allDay = o.value(QStringLiteral("allDay")).toBool(true);
    e.start = timeFromString(o.value(QStringLiteral("start")).toString());
    e.end = timeFromString(o.value(QStringLiteral("end")).toString());
    bool ok = true;
    const QJsonValue repeat = o.value(QStringLiteral("repeat"));
    e.repeat = repeat.isUndefined() ? Repeat::None : repeatFromString(repeat.toString(), &ok);
    if (!ok)
        return false;
    e.interval = o.value(QStringLiteral("interval")).toInt(1);
    e.until = QDate::fromString(o.value(QStringLiteral("until")).toString(), Qt::ISODate);
    if (e.id.isEmpty() || !e.validationError().isEmpty())
        return false;
    *out = e;
    return true;
}

} // namespace

QString repeatToString(Repeat r)
{
    switch (r) {
    case Repeat::None: return QStringLiteral("none");
    case Repeat::Daily: return QStringLiteral("daily");
    case Repeat::Weekly: return QStringLiteral("weekly");
    case Repeat::Monthly: return QStringLiteral("monthly");
    case Repeat::Yearly: return QStringLiteral("yearly");
    }
    return QStringLiteral("none");
}

Repeat repeatFromString(const QString &s, bool *ok)
{
    static const Repeat all[] = {Repeat::None, Repeat::Daily, Repeat::Weekly, Repeat::Monthly, Repeat::Yearly};
    for (Repeat r : all) {
        if (repeatToString(r) == s) {
            if (ok)
                *ok = true;
            return r;
        }
    }
    if (ok)
        *ok = false;
    return Repeat::None;
}

QString Event::validationError() const
{
    if (title.trimmed().isEmpty())
        return tr("The title cannot be empty.");
    if (title.size() > kMaxTitle)
        return tr("The title is too long.");
    if (!date.isValid())
        return tr("The date is not valid.");
    if (!allDay) {
        if (!start.isValid())
            return tr("The start time is not valid.");
        if (end.isValid() && end < start)
            return tr("The event cannot end before it starts.");
    }
    if (interval < 1 || interval > kMaxInterval)
        return tr("The repeat interval must be between 1 and 999.");
    if (until.isValid() && until < date)
        return tr("The repeat cannot end before the event starts.");
    return {};
}

bool Event::occursOn(const QDate &day) const
{
    if (!day.isValid() || !date.isValid() || day < date)
        return false;
    if (until.isValid() && day > until)
        return false;

    switch (repeat) {
    case Repeat::None:
        return day == date;
    case Repeat::Daily:
        return date.daysTo(day) % interval == 0;
    case Repeat::Weekly:
        return date.daysTo(day) % (7 * interval) == 0;
    case Repeat::Monthly: {
        const int months = (day.year() - date.year()) * 12 + (day.month() - date.month());
        // On a short month "the 31st" means the last day of that month.
        return months % interval == 0 && day.day() == qMin(date.day(), day.daysInMonth());
    }
    case Repeat::Yearly: {
        const int years = day.year() - date.year();
        return years % interval == 0 && day.month() == date.month()
               && day.day() == qMin(date.day(), day.daysInMonth());
    }
    }
    return false;
}

// ---------------------------------------------------------------------------

EventStore::EventStore(QString fileName)
    : m_file(std::move(fileName))
{
}

QString EventStore::defaultFileName()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/events.json");
}

QString EventStore::newId()
{
    return QStringLiteral("%1%2")
        .arg(QDateTime::currentMSecsSinceEpoch(), 0, 36)
        .arg(QRandomGenerator::global()->bounded(0x100000), 5, 36, QLatin1Char('0'));
}

void EventStore::load()
{
    m_events.clear();
    m_warning.clear();

    QFile file(m_file);
    if (!file.exists())
        return;
    if (!file.open(QIODevice::ReadOnly)) {
        m_warning = tr("The calendar file cannot be read: %1").arg(file.errorString());
        return;
    }
    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()
        || !doc.object().value(QStringLiteral("events")).isArray()) {
        // Keep the damaged file for the person instead of overwriting it later.
        QString backup = m_file + QStringLiteral(".corrupt");
        if (QFile::exists(backup))
            QFile::remove(backup);
        const bool kept = QFile::rename(m_file, backup);
        m_warning = kept ? tr("The calendar file was damaged. It was saved as \"%1\" and the calendar starts empty.").arg(backup)
                         : tr("The calendar file was damaged and could not be read.");
        return;
    }

    int skipped = 0;
    QSet<QString> seen;
    const QJsonArray array = doc.object().value(QStringLiteral("events")).toArray();
    for (const QJsonValue &v : array) {
        Event e;
        if (!fromJson(v, &e) || seen.contains(e.id)) {
            ++skipped;
            continue;
        }
        seen.insert(e.id);
        m_events.append(e);
    }
    if (skipped > 0)
        m_warning = QCoreApplication::translate("colibry::calendar", "%n damaged entries were skipped.", nullptr, skipped);
}

EventStore::Result EventStore::save()
{
    if (!QDir().mkpath(QFileInfo(m_file).absolutePath()))
        return {false, tr("Cannot create the calendar folder.")};

    QJsonArray array;
    for (const Event &e : m_events)
        array.append(toJson(e));
    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("events"), array);

    QSaveFile file(m_file);
    if (!file.open(QIODevice::WriteOnly))
        return {false, file.errorString()};
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit())
        return {false, file.errorString()};
    return {true, {}};
}

QString EventStore::add(Event event)
{
    m_error = event.validationError();
    if (!m_error.isEmpty())
        return {};
    do {
        event.id = newId();
    } while (contains(event.id));
    event.title = event.title.trimmed();
    m_events.append(event);
    const Result r = save();
    if (!r.ok)
        m_error = r.error; // the event stays in memory; the caller can tell the person
    return event.id;
}

bool EventStore::update(const Event &event)
{
    m_error = event.validationError();
    if (!m_error.isEmpty())
        return false;
    for (Event &e : m_events) {
        if (e.id == event.id) {
            e = event;
            e.title = e.title.trimmed();
            const Result r = save();
            m_error = r.ok ? QString() : r.error;
            return true;
        }
    }
    m_error = tr("The event no longer exists.");
    return false;
}

bool EventStore::remove(const QString &id)
{
    for (qsizetype i = 0; i < m_events.size(); ++i) {
        if (m_events.at(i).id == id) {
            m_events.removeAt(i);
            const Result r = save();
            m_error = r.ok ? QString() : r.error;
            return true;
        }
    }
    return false;
}

bool EventStore::contains(const QString &id) const
{
    return std::any_of(m_events.begin(), m_events.end(), [&](const Event &e) { return e.id == id; });
}

Event EventStore::get(const QString &id) const
{
    for (const Event &e : m_events)
        if (e.id == id)
            return e;
    return {};
}

QList<Occurrence> EventStore::occurrencesOn(const QDate &day) const
{
    QList<Occurrence> list;
    for (const Event &e : m_events)
        if (e.occursOn(day))
            list.append({e, day});
    std::sort(list.begin(), list.end(), [](const Occurrence &a, const Occurrence &b) {
        if (a.event.allDay != b.event.allDay)
            return a.event.allDay; // all-day first
        if (!a.event.allDay && a.event.start != b.event.start)
            return a.event.start < b.event.start;
        const int c = a.event.title.compare(b.event.title, Qt::CaseInsensitive);
        return c != 0 ? c < 0 : a.event.id < b.event.id;
    });
    return list;
}

QList<QDate> EventStore::daysWithEvents(int year, int month) const
{
    QList<QDate> days;
    const QDate first(year, month, 1);
    if (!first.isValid())
        return days;
    for (int d = 1; d <= first.daysInMonth(); ++d) {
        const QDate day(year, month, d);
        for (const Event &e : m_events) {
            if (e.occursOn(day)) {
                days.append(day);
                break;
            }
        }
    }
    return days;
}

Occurrence EventStore::nextOccurrence(const QDate &from, int searchDays) const
{
    for (int i = 0; i <= searchDays; ++i) {
        const QDate day = from.addDays(i);
        const QList<Occurrence> list = occurrencesOn(day);
        if (!list.isEmpty())
            return list.first();
    }
    return {};
}

} // namespace colibry::calendar
