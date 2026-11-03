#include "EventDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTimeEdit>
#include <QVBoxLayout>

namespace colibry::calendar {

EventDialog::EventDialog(const Event &event, QWidget *parent)
    : QDialog(parent)
    , m_original(event)
{
    setWindowTitle(event.id.isEmpty() ? tr("New Event") : tr("Edit Event"));
    setModal(true);

    m_title = new QLineEdit(event.title, this);
    m_title->setMaxLength(200);
    m_title->setPlaceholderText(tr("What is happening?"));

    m_date = new QDateEdit(event.date.isValid() ? event.date : QDate::currentDate(), this);
    m_date->setCalendarPopup(true);

    m_allDay = new QCheckBox(tr("All day"), this);
    m_allDay->setChecked(event.allDay);
    m_start = new QTimeEdit(event.start.isValid() ? event.start : QTime(9, 0), this);
    m_hasEnd = new QCheckBox(tr("Ends at"), this);
    m_hasEnd->setChecked(event.end.isValid());
    m_end = new QTimeEdit(event.end.isValid() ? event.end : m_start->time().addSecs(3600), this);

    m_repeat = new QComboBox(this);
    m_repeat->addItem(tr("Does not repeat"), static_cast<int>(Repeat::None));
    m_repeat->addItem(tr("Daily"), static_cast<int>(Repeat::Daily));
    m_repeat->addItem(tr("Weekly"), static_cast<int>(Repeat::Weekly));
    m_repeat->addItem(tr("Monthly"), static_cast<int>(Repeat::Monthly));
    m_repeat->addItem(tr("Yearly"), static_cast<int>(Repeat::Yearly));
    m_repeat->setCurrentIndex(m_repeat->findData(static_cast<int>(event.repeat)));
    m_interval = new QSpinBox(this);
    m_interval->setRange(1, 999);
    m_interval->setPrefix(tr("every "));
    m_interval->setValue(event.interval);
    m_hasUntil = new QCheckBox(tr("Repeat until"), this);
    m_hasUntil->setChecked(event.until.isValid());
    m_until = new QDateEdit(event.until.isValid() ? event.until : m_date->date().addYears(1), this);
    m_until->setCalendarPopup(true);

    m_notes = new QPlainTextEdit(event.notes, this);
    m_notes->setPlaceholderText(tr("Notes (optional)"));
    m_notes->setFixedHeight(80);

    m_error = new QLabel(this);
    m_error->setWordWrap(true);
    m_error->setStyleSheet(QStringLiteral("color: #c0392b;"));
    m_error->hide();

    auto *form = new QFormLayout;
    form->addRow(tr("Title"), m_title);
    form->addRow(tr("Date"), m_date);
    form->addRow(QString(), m_allDay);
    form->addRow(tr("Starts at"), m_start);
    form->addRow(m_hasEnd, m_end);
    form->addRow(tr("Repeat"), m_repeat);
    form->addRow(QString(), m_interval);
    form->addRow(m_hasUntil, m_until);
    form->addRow(tr("Notes"), m_notes);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &EventDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(m_error);
    layout->addWidget(buttons);

    connect(m_allDay, &QCheckBox::toggled, this, [this] { updateEnabled(); });
    connect(m_hasEnd, &QCheckBox::toggled, this, [this] { updateEnabled(); });
    connect(m_hasUntil, &QCheckBox::toggled, this, [this] { updateEnabled(); });
    connect(m_repeat, &QComboBox::currentIndexChanged, this, [this] { updateEnabled(); });
    updateEnabled();
    m_title->setFocus();
}

void EventDialog::updateEnabled()
{
    const bool timed = !m_allDay->isChecked();
    m_start->setEnabled(timed);
    m_hasEnd->setEnabled(timed);
    m_end->setEnabled(timed && m_hasEnd->isChecked());

    const bool repeats = static_cast<Repeat>(m_repeat->currentData().toInt()) != Repeat::None;
    m_interval->setEnabled(repeats);
    m_hasUntil->setEnabled(repeats);
    m_until->setEnabled(repeats && m_hasUntil->isChecked());
}

Event EventDialog::currentEvent() const
{
    Event e = m_original;
    e.title = m_title->text().trimmed();
    e.date = m_date->date();
    e.allDay = m_allDay->isChecked();
    e.start = e.allDay ? QTime() : m_start->time();
    e.end = (!e.allDay && m_hasEnd->isChecked()) ? m_end->time() : QTime();
    e.repeat = static_cast<Repeat>(m_repeat->currentData().toInt());
    e.interval = e.repeat == Repeat::None ? 1 : m_interval->value();
    e.until = (e.repeat != Repeat::None && m_hasUntil->isChecked()) ? m_until->date() : QDate();
    e.notes = m_notes->toPlainText();
    return e;
}

void EventDialog::accept()
{
    const QString problem = currentEvent().validationError();
    if (!problem.isEmpty()) {
        m_error->setText(problem);
        m_error->show();
        return;
    }
    QDialog::accept();
}

} // namespace colibry::calendar
