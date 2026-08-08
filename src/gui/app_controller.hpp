#pragma once

#include <QObject>
#include <QUrl>

#include "app_settings.hpp"
#include "composer_controller.hpp"
#include "job_queue_model.hpp"
#include "preset_model.hpp"

namespace cyan::gui {

class AppController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(ComposerController* composer READ composer CONSTANT)
  Q_PROPERTY(JobQueueModel* queue READ queue CONSTANT)
  Q_PROPERTY(PresetModel* presets READ presets CONSTANT)
  Q_PROPERTY(AppSettings* settings READ settings CONSTANT)
  Q_PROPERTY(QString cyanVersion READ cyanVersion CONSTANT)
  Q_PROPERTY(QString guiVersion READ guiVersion CONSTANT)

 public:
  explicit AppController(QObject* parent = nullptr);

  [[nodiscard]] ComposerController* composer();
  [[nodiscard]] JobQueueModel* queue();
  [[nodiscard]] PresetModel* presets();
  [[nodiscard]] AppSettings* settings();
  [[nodiscard]] QString cyanVersion() const;
  [[nodiscard]] QString guiVersion() const;

  Q_INVOKABLE bool commitComposer();
  Q_INVOKABLE void editJob(int row);
  Q_INVOKABLE void applyPreset(int row, bool replace);
  Q_INVOKABLE void createPreset(const QString& name, bool include_settings);
  Q_INVOKABLE void editPreset(int row);
  Q_INVOKABLE void relinkPreset(int row, const QUrl& url);
  Q_INVOKABLE QString localPath(const QUrl& url) const;

 signals:
  void navigateToComposer();
  void navigateToQueue();
  void notification(const QString& message);

 private:
  void applySettingsDefaults();

  AppSettings settings_;
  ComposerController composer_;
  JobQueueModel queue_;
  PresetModel presets_;
};

}  // namespace cyan::gui
