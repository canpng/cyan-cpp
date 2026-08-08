#pragma once

#include <QObject>

namespace cyan::gui {

class AppSettings final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString theme MEMBER theme_ NOTIFY settingsChanged)
  Q_PROPERTY(QString defaultOutputDirectory MEMBER default_output_directory_ NOTIFY settingsChanged)
  Q_PROPERTY(int defaultCompression MEMBER default_compression_ NOTIFY settingsChanged)
  Q_PROPERTY(int defaultLdidMode MEMBER default_ldid_mode_ NOTIFY settingsChanged)
  Q_PROPERTY(QString defaultLdidPath MEMBER default_ldid_path_ NOTIFY settingsChanged)
  Q_PROPERTY(QString defaultDependencyDirectory MEMBER default_dependency_directory_ NOTIFY
                 settingsChanged)

 public:
  explicit AppSettings(QObject* parent = nullptr);

  [[nodiscard]] QString theme() const;
  [[nodiscard]] QString defaultOutputDirectory() const;
  [[nodiscard]] int defaultCompression() const;
  [[nodiscard]] int defaultLdidMode() const;
  [[nodiscard]] QString defaultLdidPath() const;
  [[nodiscard]] QString defaultDependencyDirectory() const;

 signals:
  void settingsChanged();

 private:
  void load();
  void save() const;

  QString theme_{QStringLiteral("system")};
  QString default_output_directory_;
  int default_compression_{6};
  int default_ldid_mode_{0};
  QString default_ldid_path_;
  QString default_dependency_directory_;
};

}  // namespace cyan::gui
