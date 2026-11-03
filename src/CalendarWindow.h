#pragma once

#include "core/EventStore.h"

#include <QMainWindow>

class QCalendarWidget;
class QLabel;
class QListWidget;
class QPushButton;
class QTimer;

namespace colibry {
class SettingsStore;
}

namespace colibry::calendar {

/// Month view on the left, the events of the chosen day on the right.
class CalendarWindow : public QMainWindow
{
    Q_OBJECT

public:
    CalendarWindow(EventStore &store, SettingsStore &settings, QWidget *parent = nullptr);

    QDate selectedDate() const;
    void selectDate(const QDate &date);
    void goToday();

    QList<Occurrence> shownOccurrences() const { return m_shown; }
    QString selectedEventId() const;
    void selectRow(int row);
    /// Days of the shown month that are highlighted because they have events.
    QList<QDate> markedDays() const;
    QString upcomingText() const;

    // Dialog-free operations (the buttons call these after asking the person).
    QString addEvent(const Event &event);
    bool updateEvent(const Event &event);
    bool deleteEvent(const QString &id);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void buildUi();
    void refreshAll();
    void refreshList();
    void refreshMarks();
    void updateButtons();
    void promptAdd();
    void promptEdit();
    void promptDelete();

    EventStore &m_store;
    SettingsStore &m_settings;
    QCalendarWidget *m_calendar = nullptr;
    QLabel *m_dayTitle = nullptr;
    QListWidget *m_list = nullptr;
    QLabel *m_upcoming = nullptr;
    QPushButton *m_add = nullptr;
    QPushButton *m_edit = nullptr;
    QPushButton *m_delete = nullptr;
    QList<Occurrence> m_shown;
    QList<QDate> m_marked;
};

} // namespace colibry::calendar
