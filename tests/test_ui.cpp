#include "CalendarWindow.h"
#include "EventDialog.h"
#include "colibry/Keys.h"
#include "colibry/SettingsStore.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimeEdit>
#include <QtTest>

using namespace colibry;
using namespace colibry::calendar;

namespace {
Event make(const QString &title, const QDate &date)
{
    Event e;
    e.title = title;
    e.date = date;
    return e;
}
} // namespace

class CalendarUiTest : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        QVERIFY(m_tmp.isValid());
        QDir(m_tmp.path()).removeRecursively();
        QDir().mkpath(m_tmp.path());
        m_store = std::make_unique<EventStore>(m_tmp.path() + QStringLiteral("/events.json"));
        m_settings = std::make_unique<SettingsStore>(m_tmp.path() + QStringLiteral("/s.ini"));
    }

    void showsEventsOfSelectedDay()
    {
        m_store->add(make(QStringLiteral("A"), QDate(2026, 4, 10)));
        m_store->add(make(QStringLiteral("B"), QDate(2026, 4, 11)));
        CalendarWindow w(*m_store, *m_settings);
        w.selectDate(QDate(2026, 4, 10));
        QCOMPARE(w.shownOccurrences().size(), 1);
        QCOMPARE(w.shownOccurrences().first().event.title, QStringLiteral("A"));
        w.selectDate(QDate(2026, 4, 12));
        QCOMPARE(w.shownOccurrences().size(), 0);
    }

    void marksDaysWithEventsAndFollowsMonth()
    {
        m_store->add(make(QStringLiteral("A"), QDate(2026, 4, 10)));
        m_store->add(make(QStringLiteral("B"), QDate(2026, 5, 3)));
        CalendarWindow w(*m_store, *m_settings);
        w.selectDate(QDate(2026, 4, 1));
        QCOMPARE(w.markedDays(), (QList<QDate>{QDate(2026, 4, 10)}));
        w.selectDate(QDate(2026, 5, 20));
        QCOMPARE(w.markedDays(), (QList<QDate>{QDate(2026, 5, 3)}));
    }

    void addUpdatesListAndMarks()
    {
        CalendarWindow w(*m_store, *m_settings);
        w.selectDate(QDate(2026, 6, 15));
        QVERIFY(w.markedDays().isEmpty());
        const QString id = w.addEvent(make(QStringLiteral("Party"), QDate(2026, 6, 15)));
        QVERIFY(!id.isEmpty());
        QCOMPARE(w.shownOccurrences().size(), 1);
        QCOMPARE(w.markedDays(), (QList<QDate>{QDate(2026, 6, 15)}));
    }

    void invalidAddIsRefused()
    {
        CalendarWindow w(*m_store, *m_settings);
        QVERIFY(w.addEvent(make(QString(), QDate(2026, 6, 15))).isEmpty());
        QVERIFY(m_store->all().isEmpty());
    }

    void editMovesEventToAnotherDay()
    {
        CalendarWindow w(*m_store, *m_settings);
        w.selectDate(QDate(2026, 6, 15));
        const QString id = w.addEvent(make(QStringLiteral("Party"), QDate(2026, 6, 15)));
        Event e = m_store->get(id);
        e.date = QDate(2026, 6, 20);
        QVERIFY(w.updateEvent(e));
        QCOMPARE(w.shownOccurrences().size(), 0);
        QCOMPARE(w.markedDays(), (QList<QDate>{QDate(2026, 6, 20)}));
    }

    void deleteRemovesRepeatingEventEverywhere()
    {
        CalendarWindow w(*m_store, *m_settings);
        Event e = make(QStringLiteral("Standup"), QDate(2026, 6, 1));
        e.repeat = Repeat::Daily;
        const QString id = w.addEvent(e);
        w.selectDate(QDate(2026, 6, 9));
        QCOMPARE(w.shownOccurrences().size(), 1);
        QVERIFY(w.deleteEvent(id));
        QCOMPARE(w.shownOccurrences().size(), 0);
        QVERIFY(w.markedDays().isEmpty());
        QVERIFY(!w.deleteEvent(id));
    }

    void selectionAndButtonsFollowList()
    {
        CalendarWindow w(*m_store, *m_settings);
        w.selectDate(QDate(2026, 6, 15));
        QVERIFY(w.selectedEventId().isEmpty());
        const QString id = w.addEvent(make(QStringLiteral("X"), QDate(2026, 6, 15)));
        w.selectRow(0);
        QCOMPARE(w.selectedEventId(), id);
        w.show();
        QVERIFY(w.findChild<QPushButton *>() != nullptr);
    }

    void survivesDamagedStoreAndShowsWarning()
    {
        QFile f(m_tmp.path() + QStringLiteral("/events.json"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("garbage");
        f.close();
        m_store->load();
        CalendarWindow w(*m_store, *m_settings);
        QVERIFY(!m_store->loadWarning().isEmpty());
        QVERIFY(w.shownOccurrences().isEmpty());
    }

    void upcomingTextMentionsNextEvent()
    {
        CalendarWindow w(*m_store, *m_settings);
        QVERIFY(!w.upcomingText().contains(QStringLiteral("Soon")));
        m_store->add(make(QStringLiteral("Soon"), QDate::currentDate().addDays(3)));
        QVERIFY(w.upcomingText().contains(QStringLiteral("Soon")));
    }

    void geometryIsRemembered()
    {
        {
            CalendarWindow w(*m_store, *m_settings);
            w.show();
            w.close();
        }
        QVERIFY(!m_settings->value(Keys::CalendarGeometry).toByteArray().isEmpty());
    }

    // --- dialog -------------------------------------------------------------

    void dialogReturnsWhatWasTyped()
    {
        Event e = make(QString(), QDate(2026, 8, 8));
        EventDialog d(e);
        d.titleEdit()->setText(QStringLiteral("  Lunch "));
        d.allDayBox()->setChecked(false);
        d.startEdit()->setTime(QTime(12, 30));
        const Event r = d.currentEvent();
        QCOMPARE(r.title, QStringLiteral("Lunch"));
        QCOMPARE(r.date, QDate(2026, 8, 8));
        QVERIFY(!r.allDay);
        QCOMPARE(r.start, QTime(12, 30));
        QVERIFY(!r.end.isValid());
        QVERIFY(r.validationError().isEmpty());
    }

    void dialogRefusesEmptyTitleAndShowsWhy()
    {
        EventDialog d(make(QString(), QDate(2026, 8, 8)));
        d.show();
        d.accept();
        QCOMPARE(d.result(), static_cast<int>(QDialog::Rejected));
        QVERIFY(d.isVisible());
        QVERIFY(d.errorLabel()->isVisible());
        QVERIFY(!d.errorLabel()->text().isEmpty());
        d.titleEdit()->setText(QStringLiteral("ok"));
        d.accept();
        QCOMPARE(d.result(), static_cast<int>(QDialog::Accepted));
    }

    void dialogKeepsIdAndDisablesIrrelevantFields()
    {
        Event e = make(QStringLiteral("x"), QDate(2026, 8, 8));
        e.id = QStringLiteral("keep");
        EventDialog d(e);
        QCOMPARE(d.currentEvent().id, QStringLiteral("keep"));
        QVERIFY(!d.startEdit()->isEnabled()); // all-day
        d.allDayBox()->setChecked(false);
        QVERIFY(d.startEdit()->isEnabled());
        d.repeatBox()->setCurrentIndex(d.repeatBox()->findData(static_cast<int>(Repeat::Weekly)));
        QCOMPARE(d.currentEvent().repeat, Repeat::Weekly);
        d.repeatBox()->setCurrentIndex(0);
        QCOMPARE(d.currentEvent().interval, 1);
    }

    void dialogRefusesEndBeforeStart()
    {
        EventDialog d(make(QStringLiteral("x"), QDate(2026, 8, 8)));
        d.allDayBox()->setChecked(false);
        d.startEdit()->setTime(QTime(15, 0));
        d.findChild<QCheckBox *>(QString()); // no-op, keeps the test independent of widget names
        const auto boxes = d.findChildren<QCheckBox *>();
        for (QCheckBox *b : boxes)
            if (b->text().contains(QStringLiteral("Ends")))
                b->setChecked(true);
        d.endEdit()->setTime(QTime(14, 0));
        QVERIFY(!d.currentEvent().validationError().isEmpty());
    }

private:
    QTemporaryDir m_tmp;
    std::unique_ptr<EventStore> m_store;
    std::unique_ptr<SettingsStore> m_settings;
};

QTEST_MAIN(CalendarUiTest)
#include "test_ui.moc"
