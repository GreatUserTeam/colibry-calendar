#include "CalendarWindow.h"

#include "EventDialog.h"
#include "colibry/Keys.h"
#include "colibry/SettingsStore.h"

#include <QAction>
#include <QCalendarWidget>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QTextCharFormat>
#include <QVBoxLayout>

namespace colibry::calendar {

namespace {
constexpr int kIdRole = Qt::UserRole + 1;
}

CalendarWindow::CalendarWindow(EventStore &store, SettingsStore &settings, QWidget *parent)
    : QMainWindow(parent)
    , m_store(store)
    , m_settings(settings)
{
    setWindowTitle(tr("Calendar"));
    setWindowIcon(QIcon::fromTheme(QStringLiteral("x-office-calendar")));
    resize(900, 560);
    setMinimumSize(560, 380);
    buildUi();

    const QByteArray geometry = m_settings.value(Keys::CalendarGeometry).toByteArray();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);

    refreshAll();
    if (!m_store.loadWarning().isEmpty())
        statusBar()->showMessage(m_store.loadWarning());
}

void CalendarWindow::buildUi()
{
    auto *central = new QWidget(this);
    auto *row = new QHBoxLayout(central);

    m_calendar = new QCalendarWidget(central);
    m_calendar->setGridVisible(true);
    m_calendar->setFirstDayOfWeek(QLocale().firstDayOfWeek());
    m_calendar->setVerticalHeaderFormat(QCalendarWidget::ISOWeekNumbers);
    m_calendar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    row->addWidget(m_calendar, 3);

    auto *side = new QVBoxLayout;
    m_dayTitle = new QLabel(central);
    QFont f = m_dayTitle->font();
    f.setBold(true);
    f.setPointSizeF(f.pointSizeF() * 1.25);
    m_dayTitle->setFont(f);
    side->addWidget(m_dayTitle);

    m_list = new QListWidget(central);
    m_list->setAlternatingRowColors(true);
    side->addWidget(m_list, 1);

    auto *buttons = new QHBoxLayout;
    m_add = new QPushButton(tr("Add…"), central);
    m_edit = new QPushButton(tr("Edit…"), central);
    m_delete = new QPushButton(tr("Delete…"), central);
    buttons->addWidget(m_add);
    buttons->addWidget(m_edit);
    buttons->addWidget(m_delete);
    side->addLayout(buttons);

    m_upcoming = new QLabel(central);
    m_upcoming->setWordWrap(true);
    side->addWidget(m_upcoming);
    row->addLayout(side, 2);
    setCentralWidget(central);

    auto *today = new QAction(tr("Today"), this);
    today->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    connect(today, &QAction::triggered, this, [this] { goToday(); });
    auto *add = new QAction(tr("New Event…"), this);
    add->setShortcut(QKeySequence::New);
    connect(add, &QAction::triggered, this, [this] { promptAdd(); });
    QMenu *menu = menuBar()->addMenu(tr("&Calendar"));
    menu->addAction(add);
    menu->addAction(today);
    QAction *close = menu->addAction(tr("Close"));
    close->setShortcut(QKeySequence::Close);
    connect(close, &QAction::triggered, this, &QWidget::close);
    addAction(add);
    addAction(today);

    connect(m_calendar, &QCalendarWidget::selectionChanged, this, [this] { refreshList(); });
    connect(m_calendar, &QCalendarWidget::currentPageChanged, this, [this] { refreshMarks(); });
    connect(m_calendar, &QCalendarWidget::activated, this, [this] { promptAdd(); });
    connect(m_list, &QListWidget::currentRowChanged, this, [this] { updateButtons(); });
    connect(m_list, &QListWidget::itemActivated, this, [this] { promptEdit(); });
    connect(m_add, &QPushButton::clicked, this, [this] { promptAdd(); });
    connect(m_edit, &QPushButton::clicked, this, [this] { promptEdit(); });
    connect(m_delete, &QPushButton::clicked, this, [this] { promptDelete(); });
}

QDate CalendarWindow::selectedDate() const
{
    return m_calendar->selectedDate();
}

void CalendarWindow::selectDate(const QDate &date)
{
    if (date.isValid())
        m_calendar->setSelectedDate(date); // also moves the shown month
    refreshAll();
}

void CalendarWindow::goToday()
{
    selectDate(QDate::currentDate());
}

QString CalendarWindow::selectedEventId() const
{
    const int row = m_list->currentRow();
    return row >= 0 && row < m_shown.size() ? m_shown.at(row).event.id : QString();
}

void CalendarWindow::selectRow(int row)
{
    m_list->setCurrentRow(row);
}

QList<QDate> CalendarWindow::markedDays() const
{
    return m_marked;
}

void CalendarWindow::refreshAll()
{
    refreshMarks();
    refreshList();
}

void CalendarWindow::refreshMarks()
{
    // Reset what was marked before, then mark the days that have events.
    for (const QDate &d : std::as_const(m_marked))
        m_calendar->setDateTextFormat(d, QTextCharFormat());
    m_marked = m_store.daysWithEvents(m_calendar->yearShown(), m_calendar->monthShown());

    QTextCharFormat marked;
    marked.setFontWeight(QFont::Bold);
    marked.setForeground(palette().color(QPalette::Highlight));
    marked.setFontUnderline(true);
    for (const QDate &d : std::as_const(m_marked))
        m_calendar->setDateTextFormat(d, marked);
}

void CalendarWindow::refreshList()
{
    const QString keep = selectedEventId();
    const QDate day = selectedDate();
    m_shown = m_store.occurrencesOn(day);

    const QSignalBlocker blocker(m_list);
    m_list->clear();
    const QLocale locale;
    m_dayTitle->setText(locale.toString(day, QLocale::LongFormat));
    for (const Occurrence &o : std::as_const(m_shown)) {
        const Event &e = o.event;
        QString when = tr("All day");
        if (!e.allDay) {
            when = e.start.toString(QStringLiteral("HH:mm"));
            if (e.end.isValid())
                when += QStringLiteral(" – ") + e.end.toString(QStringLiteral("HH:mm"));
        }
        QString text = QStringLiteral("%1  %2").arg(when, e.title);
        if (e.repeat != Repeat::None)
            text += QStringLiteral("  ⟳");
        auto *item = new QListWidgetItem(text, m_list);
        item->setData(kIdRole, e.id);
        if (!e.notes.trimmed().isEmpty())
            item->setToolTip(e.notes);
    }
    for (int i = 0; i < m_shown.size(); ++i)
        if (m_shown.at(i).event.id == keep)
            m_list->setCurrentRow(i);

    if (m_shown.isEmpty())
        m_list->addItem(tr("No events"));
    if (m_shown.isEmpty())
        m_list->item(0)->setFlags(Qt::NoItemFlags);

    m_upcoming->setText(upcomingText());
    updateButtons();
}

QString CalendarWindow::upcomingText() const
{
    const QDate today = QDate::currentDate();
    const Occurrence next = m_store.nextOccurrence(today);
    if (next.event.id.isEmpty())
        return tr("Nothing planned.");
    return tr("Next: %1 — %2").arg(QLocale().toString(next.day, QLocale::ShortFormat), next.event.title);
}

void CalendarWindow::updateButtons()
{
    const bool has = !selectedEventId().isEmpty();
    m_edit->setEnabled(has);
    m_delete->setEnabled(has);
}

// ---------------------------------------------------------------------------

QString CalendarWindow::addEvent(const Event &event)
{
    const QString id = m_store.add(event);
    if (id.isEmpty()) {
        statusBar()->showMessage(m_store.lastError(), 6000);
        return {};
    }
    if (!m_store.lastError().isEmpty())
        statusBar()->showMessage(tr("Could not save: %1").arg(m_store.lastError()), 8000);
    refreshAll();
    return id;
}

bool CalendarWindow::updateEvent(const Event &event)
{
    const bool ok = m_store.update(event);
    if (!ok)
        statusBar()->showMessage(m_store.lastError(), 6000);
    else if (!m_store.lastError().isEmpty())
        statusBar()->showMessage(tr("Could not save: %1").arg(m_store.lastError()), 8000);
    refreshAll();
    return ok;
}

bool CalendarWindow::deleteEvent(const QString &id)
{
    const bool ok = m_store.remove(id);
    refreshAll();
    return ok;
}

void CalendarWindow::promptAdd()
{
    Event e;
    e.date = selectedDate();
    EventDialog dialog(e, this);
    if (dialog.exec() == QDialog::Accepted)
        addEvent(dialog.currentEvent());
}

void CalendarWindow::promptEdit()
{
    const QString id = selectedEventId();
    if (id.isEmpty())
        return;
    EventDialog dialog(m_store.get(id), this);
    if (dialog.exec() == QDialog::Accepted)
        updateEvent(dialog.currentEvent());
}

void CalendarWindow::promptDelete()
{
    const QString id = selectedEventId();
    if (id.isEmpty())
        return;
    const Event e = m_store.get(id);
    const QString extra = e.repeat != Repeat::None ? tr("\nAll its repeats will be deleted too.") : QString();
    const auto answer = QMessageBox::warning(this, tr("Delete Event"),
                                             tr("Delete \"%1\"?%2").arg(e.title, extra),
                                             QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (answer == QMessageBox::Yes)
        deleteEvent(id);
}

void CalendarWindow::closeEvent(QCloseEvent *event)
{
    m_settings.setValue(Keys::CalendarGeometry, saveGeometry());
    QMainWindow::closeEvent(event);
}

} // namespace colibry::calendar
