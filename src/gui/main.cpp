#include <QFont>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include "app_controller.hpp"

int main(int argc, char* argv[]) {
  QGuiApplication::setOrganizationName(QStringLiteral("cyan"));
  QGuiApplication::setOrganizationDomain(QStringLiteral("cyan.local"));
  QGuiApplication::setApplicationName(QStringLiteral("cyan"));
  QGuiApplication::setApplicationVersion(QStringLiteral(CYAN_GUI_VERSION));
  QQuickStyle::setStyle(QStringLiteral("Basic"));

  QGuiApplication application(argc, argv);
  QFont font(QStringLiteral("Segoe UI Variable"));
  font.setStyleHint(QFont::SansSerif);
  application.setFont(font);

  cyan::gui::AppController controller;
  QQmlApplicationEngine engine;
  engine.rootContext()->setContextProperty(QStringLiteral("app"), &controller);
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreationFailed, &application,
      [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
  engine.loadFromModule(QStringLiteral("Cyan.Gui"), QStringLiteral("Main"));
  return application.exec();
}
