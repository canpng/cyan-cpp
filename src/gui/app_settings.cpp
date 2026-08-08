#include "app_settings.hpp"

#include <QSettings>
#include <QStandardPaths>

namespace cyan::gui {

AppSettings::AppSettings(QObject* parent) : QObject(parent) {
  load();
  connect(this, &AppSettings::settingsChanged, this, &AppSettings::save);
}

QString AppSettings::theme() const { return theme_; }
QString AppSettings::defaultOutputDirectory() const { return default_output_directory_; }
int AppSettings::defaultCompression() const { return default_compression_; }
int AppSettings::defaultLdidMode() const { return default_ldid_mode_; }
QString AppSettings::defaultLdidPath() const { return default_ldid_path_; }
QString AppSettings::defaultDependencyDirectory() const { return default_dependency_directory_; }

void AppSettings::load() {
  QSettings settings;
  theme_ = settings.value(QStringLiteral("appearance/theme"), QStringLiteral("system")).toString();
  default_output_directory_ =
      settings
          .value(QStringLiteral("defaults/outputDirectory"),
                 QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
          .toString();
  default_compression_ = settings.value(QStringLiteral("defaults/compression"), 6).toInt();
  default_ldid_mode_ = settings.value(QStringLiteral("signing/ldidMode"), 0).toInt();
  default_ldid_path_ = settings.value(QStringLiteral("signing/ldidPath")).toString();
  default_dependency_directory_ =
      settings.value(QStringLiteral("defaults/dependencyDirectory")).toString();
}

void AppSettings::save() const {
  QSettings settings;
  settings.setValue(QStringLiteral("appearance/theme"), theme_);
  settings.setValue(QStringLiteral("defaults/outputDirectory"), default_output_directory_);
  settings.setValue(QStringLiteral("defaults/compression"), qBound(0, default_compression_, 9));
  settings.setValue(QStringLiteral("signing/ldidMode"), default_ldid_mode_);
  settings.setValue(QStringLiteral("signing/ldidPath"), default_ldid_path_);
  settings.setValue(QStringLiteral("defaults/dependencyDirectory"), default_dependency_directory_);
}

}  // namespace cyan::gui
