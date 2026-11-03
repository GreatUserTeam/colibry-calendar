#include "core/EventStore.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace colibry::calendar;

namespace {
Event make(const QString &title, const QDate &date, Repeat r = Repeat::None, int interval = 1)
{
    Event e;
    e.title = title;
    e.date = date;
    e.repeat = r;
    e.interval = interval;
    return e;
}
void writeFile(const QString &path, const QByteArray &data)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}
} // namespace

class EventStoreTest : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        QVERIFY(m_tmp.isValid());
        m_file = m_tmp.path() + QStringLiteral("/sub/events.json");
        QDir(m_tmp.path()).removeRecursively();
        QDir().mkpath(m_tmp.path());
    }

    void validation()
    {
        QVERIFY(make(QStringLiteral("ok"), QDate(2026, 1, 1)).validationError().isEmpty());
        QVERIFY(!make(QStringLiteral("  "), QDate(2026, 1, 1)).validationError().isEmpty());
        QVERIFY(!make(QStringLiteral("x"), QDate()).validationError().isEmpty());
        QVERIFY(!make(QString(300, QLatin1Char('x')), QDate(2026, 1, 1)).validationError().isEmpty());
        QVERIFY(!make(QStringLiteral("x"), QDate(2026, 1, 1), Repeat::Daily, 0).validationError().isEmpty());
        QVERIFY(!make(QStringLiteral("x"), QDate(2026, 1, 1), Repeat::Daily, 1000).validationError().isEmpty());

        Event timed = make(QStringLiteral("x"), QDate(2026, 1, 1));
        timed.allDay = false;
        QVERIFY(!timed.validationError().isEmpty()); // no start time
        timed.start = QTime(10, 0);
        QVERIFY(timed.validationError().isEmpty());
        timed.end = QTime(9, 0);
        QVERIFY(!timed.validationError().isEmpty());
        timed.end = QTime(10, 0);
        QVERIFY(timed.validationError().isEmpty());

        Event until = make(QStringLiteral("x"), QDate(2026, 5, 5), Repeat::Daily);
        until.until = QDate(2026, 5, 4);
        QVERIFY(!until.validationError().isEmpty());
    }

    void singleEvent()
    {
        const Event e = make(QStringLiteral("x"), QDate(2026, 3, 10));
        QVERIFY(e.occursOn(QDate(2026, 3, 10)));
        QVERIFY(!e.occursOn(QDate(2026, 3, 9)));
        QVERIFY(!e.occursOn(QDate(2026, 3, 11)));
        QVERIFY(!e.occursOn(QDate()));
    }

    void daily()
    {
        const Event e = make(QStringLiteral("x"), QDate(2026, 3, 10), Repeat::Daily, 3);
        QVERIFY(e.occursOn(QDate(2026, 3, 10)));
        QVERIFY(!e.occursOn(QDate(2026, 3, 11)));
        QVERIFY(e.occursOn(QDate(2026, 3, 13)));
        QVERIFY(e.occursOn(QDate(2026, 4, 9))); // 30 days later
        QVERIFY(!e.occursOn(QDate(2026, 3, 7))); // never before the start
    }

    void weekly()
    {
        const Event e = make(QStringLiteral("x"), QDate(2026, 3, 10), Repeat::Weekly, 2);
        QVERIFY(e.occursOn(QDate(2026, 3, 24)));
        QVERIFY(!e.occursOn(QDate(2026, 3, 17)));
        QVERIFY(!e.occursOn(QDate(2026, 3, 25)));
    }

    void monthlyOnTheThirtyFirstUsesLastDayOfShortMonths()
    {
        const Event e = make(QStringLiteral("x"), QDate(2026, 1, 31), Repeat::Monthly);
        QVERIFY(e.occursOn(QDate(2026, 2, 28)));
        QVERIFY(!e.occursOn(QDate(2026, 2, 27)));
        QVERIFY(e.occursOn(QDate(2026, 4, 30)));
        QVERIFY(e.occursOn(QDate(2026, 5, 31)));
        QVERIFY(!e.occursOn(QDate(2026, 5, 30)));
        QVERIFY(e.occursOn(QDate(2028, 2, 29))); // leap year
    }

    void monthlyWithInterval()
    {
        const Event e = make(QStringLiteral("x"), QDate(2026, 1, 15), Repeat::Monthly, 3);
        QVERIFY(e.occursOn(QDate(2026, 4, 15)));
        QVERIFY(e.occursOn(QDate(2027, 1, 15)));
        QVERIFY(!e.occursOn(QDate(2026, 3, 15)));
    }

    void yearlyAndLeapDay()
    {
        const Event b = make(QStringLiteral("x"), QDate(2020, 2, 29), Repeat::Yearly);
        QVERIFY(b.occursOn(QDate(2021, 2, 28)));
        QVERIFY(!b.occursOn(QDate(2021, 3, 1)));
        QVERIFY(b.occursOn(QDate(2024, 2, 29)));
        QVERIFY(!b.occursOn(QDate(2024, 2, 28)));
        const Event y = make(QStringLiteral("x"), QDate(2026, 7, 4), Repeat::Yearly, 2);
        QVERIFY(y.occursOn(QDate(2028, 7, 4)));
        QVERIFY(!y.occursOn(QDate(2027, 7, 4)));
    }

    void untilIsInclusive()
    {
        Event e = make(QStringLiteral("x"), QDate(2026, 3, 1), Repeat::Daily);
        e.until = QDate(2026, 3, 5);
        QVERIFY(e.occursOn(QDate(2026, 3, 5)));
        QVERIFY(!e.occursOn(QDate(2026, 3, 6)));
    }

    void addGetUpdateRemoveAndPersist()
    {
        QString id;
        {
            EventStore s(m_file);
            s.load();
            id = s.add(make(QStringLiteral("  Dentist  "), QDate(2026, 5, 1)));
            QVERIFY(!id.isEmpty());
            QCOMPARE(s.get(id).title, QStringLiteral("Dentist"));
            Event e = s.get(id);
            e.title = QStringLiteral("Doctor");
            e.notes = QStringLiteral("Bring card 😀");
            QVERIFY(s.update(e));
        }
        {
            EventStore s(m_file);
            s.load();
            QVERIFY(s.loadWarning().isEmpty());
            QCOMPARE(s.all().size(), 1);
            QCOMPARE(s.get(id).title, QStringLiteral("Doctor"));
            QCOMPARE(s.get(id).notes, QStringLiteral("Bring card 😀"));
            QVERIFY(s.remove(id));
            QVERIFY(!s.remove(id));
        }
        EventStore s(m_file);
        s.load();
        QVERIFY(s.all().isEmpty());
    }

    void roundTripKeepsEveryField()
    {
        Event e = make(QStringLiteral("Gym"), QDate(2026, 2, 3), Repeat::Weekly, 2);
        e.allDay = false;
        e.start = QTime(18, 30);
        e.end = QTime(19, 45);
        e.until = QDate(2026, 12, 31);
        e.notes = QStringLiteral("line1\nline2");
        QString id;
        {
            EventStore s(m_file);
            id = s.add(e);
        }
        EventStore s(m_file);
        s.load();
        const Event r = s.get(id);
        QCOMPARE(r.title, e.title);
        QCOMPARE(r.date, e.date);
        QCOMPARE(r.allDay, false);
        QCOMPARE(r.start, e.start);
        QCOMPARE(r.end, e.end);
        QCOMPARE(r.repeat, e.repeat);
        QCOMPARE(r.interval, 2);
        QCOMPARE(r.until, e.until);
        QCOMPARE(r.notes, e.notes);
    }

    void invalidEventsAreRejectedAndNotStored()
    {
        EventStore s(m_file);
        QVERIFY(s.add(make(QString(), QDate(2026, 1, 1))).isEmpty());
        QVERIFY(!s.lastError().isEmpty());
        QVERIFY(s.all().isEmpty());
        QVERIFY(!QFileInfo::exists(m_file));
        Event ghost = make(QStringLiteral("x"), QDate(2026, 1, 1));
        ghost.id = QStringLiteral("nope");
        QVERIFY(!s.update(ghost));
    }

    void missingFileIsFine()
    {
        EventStore s(m_file);
        s.load();
        QVERIFY(s.all().isEmpty());
        QVERIFY(s.loadWarning().isEmpty());
    }

    void corruptFileIsKeptAndCalendarStartsEmpty()
    {
        QDir().mkpath(m_tmp.path() + QStringLiteral("/sub"));
        writeFile(m_file, "{ this is not json");
        EventStore s(m_file);
        s.load();
        QVERIFY(s.all().isEmpty());
        QVERIFY(!s.loadWarning().isEmpty());
        QVERIFY(QFileInfo::exists(m_file + QStringLiteral(".corrupt")));
        QVERIFY(!QFileInfo::exists(m_file));
        // and it still works afterwards
        QVERIFY(!s.add(make(QStringLiteral("x"), QDate(2026, 1, 1))).isEmpty());
    }

    void wrongShapeIsTreatedAsCorrupt()
    {
        QDir().mkpath(m_tmp.path() + QStringLiteral("/sub"));
        for (const char *bad : {"[]", "42", "{\"events\": 5}", "{}", ""}) {
            QFile::remove(m_file + QStringLiteral(".corrupt"));
            writeFile(m_file, bad);
            EventStore s(m_file);
            s.load();
            QVERIFY2(!s.loadWarning().isEmpty(), bad);
            QVERIFY(s.all().isEmpty());
        }
    }

    void brokenEntriesAreSkippedGoodOnesKept()
    {
        QDir().mkpath(m_tmp.path() + QStringLiteral("/sub"));
        writeFile(m_file, R"({"version":1,"events":[
          {"id":"a","title":"Good","date":"2026-01-02","allDay":true},
          {"id":"b","title":"","date":"2026-01-02"},
          {"id":"c","title":"No date"},
          "string",
          {"id":"d","title":"Bad repeat","date":"2026-01-02","repeat":"hourly"},
          {"id":"a","title":"Duplicate id","date":"2026-01-03"},
          {"title":"No id","date":"2026-01-02"},
          {"id":"e","title":"Interval","date":"2026-01-02","repeat":"daily","interval":-4},
          {"id":"f","title":"Fine too","date":"2026-02-02","allDay":false,"start":"08:15"}
        ]})");
        EventStore s(m_file);
        s.load();
        QCOMPARE(s.all().size(), 2);
        QVERIFY(s.contains(QStringLiteral("a")));
        QVERIFY(s.contains(QStringLiteral("f")));
        QCOMPARE(s.get(QStringLiteral("f")).start, QTime(8, 15));
        QVERIFY(s.loadWarning().contains(QStringLiteral("7")));
        QVERIFY(QFileInfo::exists(m_file)); // not renamed
    }

    void occurrencesAreSortedAllDayThenTimeThenTitle()
    {
        EventStore s(m_file);
        const QDate d(2026, 6, 1);
        Event late = make(QStringLiteral("Late"), d);
        late.allDay = false;
        late.start = QTime(20, 0);
        Event early = make(QStringLiteral("Early"), d);
        early.allDay = false;
        early.start = QTime(7, 0);
        s.add(late);
        s.add(early);
        s.add(make(QStringLiteral("b all day"), d));
        s.add(make(QStringLiteral("A all day"), d));
        const auto list = s.occurrencesOn(d);
        QCOMPARE(list.size(), 4);
        QCOMPARE(list.at(0).event.title, QStringLiteral("A all day"));
        QCOMPARE(list.at(1).event.title, QStringLiteral("b all day"));
        QCOMPARE(list.at(2).event.title, QStringLiteral("Early"));
        QCOMPARE(list.at(3).event.title, QStringLiteral("Late"));
    }

    void daysWithEventsInMonth()
    {
        EventStore s(m_file);
        s.add(make(QStringLiteral("once"), QDate(2026, 2, 10)));
        s.add(make(QStringLiteral("weekly"), QDate(2026, 2, 2), Repeat::Weekly));
        const auto days = s.daysWithEvents(2026, 2);
        QCOMPARE(days, (QList<QDate>{QDate(2026, 2, 2), QDate(2026, 2, 9), QDate(2026, 2, 10),
                                      QDate(2026, 2, 16), QDate(2026, 2, 23)}));
        QVERIFY(s.daysWithEvents(2026, 13).isEmpty());
        QVERIFY(s.daysWithEvents(2025, 12).isEmpty());
    }

    void nextOccurrence()
    {
        EventStore s(m_file);
        QVERIFY(s.nextOccurrence(QDate(2026, 1, 1)).event.id.isEmpty());
        s.add(make(QStringLiteral("later"), QDate(2026, 3, 1)));
        const auto n = s.nextOccurrence(QDate(2026, 1, 1));
        QCOMPARE(n.day, QDate(2026, 3, 1));
        QCOMPARE(n.event.title, QStringLiteral("later"));
        QVERIFY(s.nextOccurrence(QDate(2026, 3, 2)).event.id.isEmpty());
    }

    void unwritableLocationKeepsEventInMemoryAndReports()
    {
        QFile blocker(m_tmp.path() + QStringLiteral("/blocker"));
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();
        EventStore s(m_tmp.path() + QStringLiteral("/blocker/events.json"));
        const QString id = s.add(make(QStringLiteral("x"), QDate(2026, 1, 1)));
        QVERIFY(!id.isEmpty());
        QVERIFY(!s.lastError().isEmpty());
        QVERIFY(s.contains(id));
    }

    void repeatNamesRoundTrip()
    {
        for (Repeat r : {Repeat::None, Repeat::Daily, Repeat::Weekly, Repeat::Monthly, Repeat::Yearly}) {
            bool ok = false;
            QCOMPARE(repeatFromString(repeatToString(r), &ok), r);
            QVERIFY(ok);
        }
        bool ok = true;
        repeatFromString(QStringLiteral("x"), &ok);
        QVERIFY(!ok);
    }

private:
    QTemporaryDir m_tmp;
    QString m_file;
};

QTEST_MAIN(EventStoreTest)
#include "test_core.moc"
