#pragma once

#include "core/EventStore.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDateEdit;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;
class QTimeEdit;

namespace colibry::calendar {

/// Dialog for creating or changing one event.
class EventDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EventDialog(const Event &event, QWidget *parent = nullptr);

    /// The event as currently typed (keeps the id of the one that was passed in).
    Event currentEvent() const;

    // Direct access for tests.
    QLineEdit *titleEdit() const { return m_title; }
    QCheckBox *allDayBox() const { return m_allDay; }
    QComboBox *repeatBox() const { return m_repeat; }
    QTimeEdit *startEdit() const { return m_start; }
    QTimeEdit *endEdit() const { return m_end; }
    QLabel *errorLabel() const { return m_error; }

    void accept() override;

private:
    void updateEnabled();

    Event m_original;
    QLineEdit *m_title;
    QDateEdit *m_date;
    QCheckBox *m_allDay;
    QTimeEdit *m_start;
    QCheckBox *m_hasEnd;
    QTimeEdit *m_end;
    QComboBox *m_repeat;
    QSpinBox *m_interval;
    QCheckBox *m_hasUntil;
    QDateEdit *m_until;
    QPlainTextEdit *m_notes;
    QLabel *m_error;
};

} // namespace colibry::calendar
