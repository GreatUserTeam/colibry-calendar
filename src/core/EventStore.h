#pragma once

#include <QDate>
#include <QList>
#include <QString>
#include <QTime>

namespace colibry::calendar {

enum class Repeat { None, Daily, Weekly, Monthly, Yearly };

QString repeatToString(Repeat r);
Repeat repeatFromString(const QString &s, bool *ok = nullptr);

struct Event
{
    QString id;
    QString title;
    QString notes;
    QDate date;           ///< first day (for repeating events: the first occurrence)
    bool allDay = true;
    QTime start;          ///< valid only when !allDay
    QTime end;            ///< optional, valid only when !allDay
    Repeat repeat = Repeat::None;
    int interval = 1;     ///< every N days / weeks / months / years
    QDate until;          ///< optional last day of a repeating event

    /// Empty when the event is acceptable; otherwise a message for the person.
    QString validationError() const;
    bool occursOn(const QDate &day) const;
};

struct Occurrence
{
    Event event;
    QDate day;
};

/// All events, kept in one JSON file. Every change is written at once (atomically).
///
/// A damaged file never takes the program down and never loses data silently: it is renamed to
/// "<name>.corrupt" and the calendar starts empty, and loadWarning() tells the person about it.
/// Single broken entries inside an otherwise good file are skipped and counted.
class EventStore
{
public:
    struct Result
    {
        bool ok = false;
        QString error;
    };

    explicit EventStore(QString fileName);

    static QString defaultFileName();

    /// Loads the file. Missing file = empty calendar (not a problem).
    void load();
    /// Text for the person if loading lost or skipped something; empty otherwise.
    QString loadWarning() const { return m_warning; }
    QString fileName() const { return m_file; }

    /// Returns the new id, or an empty string when the event is invalid (see lastError()).
    QString add(Event event);
    bool update(const Event &event);
    bool remove(const QString &id);
    bool contains(const QString &id) const;
    Event get(const QString &id) const;
    QList<Event> all() const { return m_events; }
    QString lastError() const { return m_error; }

    QList<Occurrence> occurrencesOn(const QDate &day) const;
    /// Days of the given month that have at least one occurrence.
    QList<QDate> daysWithEvents(int year, int month) const;
    /// The next occurrence on or after the given day (searching a limited time ahead).
    Occurrence nextOccurrence(const QDate &from, int searchDays = 366 * 2) const;

private:
    Result save();
    static QString newId();

    QString m_file;
    QList<Event> m_events;
    QString m_warning;
    QString m_error;
};

} // namespace colibry::calendar
