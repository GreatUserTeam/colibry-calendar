#include "CalendarWindow.h"
#include "colibry/SettingsStore.h"
#include "colibry/ThemeSync.h"

#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("colibry-calendar"));
    QApplication::setApplicationDisplayName(QStringLiteral("Calendar"));
    QApplication::setApplicationVersion(QStringLiteral(Colibry_APPS_VERSION));
    QApplication::setOrganizationName(QStringLiteral("GreatUserTeam"));
    QApplication::setDesktopFileName(QStringLiteral("colibry-calendar"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Calendar for the Great User System"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);

    colibry::SettingsStore settings;
    colibry::ThemeSync themeSync(app, settings);

    colibry::calendar::EventStore store(colibry::calendar::EventStore::defaultFileName());
    store.load();
    colibry::calendar::CalendarWindow window(store, settings);
    window.show();
    return QApplication::exec();
}
