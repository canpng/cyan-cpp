#include "app_controller.hpp"

#include <QDir>

namespace cyan::gui {

AppController::AppController(QObject* parent)
    : QObject(parent), settings_(this), composer_(this), queue_(this), presets_(this) {
  applySettingsDefaults();
  connect(&settings_, &AppSettings::settingsChanged, this, &AppController::applySettingsDefaults);
  connect(&composer_, &ComposerController::notification, this, &AppController::notification);
  connect(&queue_, &JobQueueModel::notification, this, &AppController::notification);
  connect(&presets_, &PresetModel::notification, this, &AppController::notification);
}

ComposerController* AppController::composer() { return &composer_; }
JobQueueModel* AppController::queue() { return &queue_; }
PresetModel* AppController::presets() { return &presets_; }
AppSettings* AppController::settings() { return &settings_; }
QString AppController::cyanVersion() const { return QStringLiteral(CYAN_VERSION); }
QString AppController::guiVersion() const { return QStringLiteral(CYAN_GUI_VERSION); }

void AppController::applySettingsDefaults() {
  composer_.setDefaults(settings_.defaultOutputDirectory(), settings_.defaultCompression(),
                        settings_.defaultLdidMode(), settings_.defaultLdidPath(),
                        settings_.defaultDependencyDirectory());
}

bool AppController::commitComposer() {
  if (!composer_.canQueue()) {
    emit notification(composer_.validationMessage());
    return false;
  }
  const JobDefinition job = composer_.snapshot();
  if (composer_.editing()) {
    if (!queue_.updateJob(job)) {
      emit notification(QStringLiteral("Çalışan iş güncellenemez."));
      return false;
    }
  } else {
    queue_.addJob(job);
  }
  composer_.reset();
  emit navigateToQueue();
  return true;
}

void AppController::editJob(int row) {
  const JobDefinition job = queue_.jobAt(row);
  if (job.id.isEmpty()) {
    return;
  }
  composer_.load(job);
  emit navigateToComposer();
}

void AppController::applyPreset(int row, bool replace) {
  const PresetDefinition preset = presets_.definitionAt(row);
  if (preset.id.isEmpty()) {
    return;
  }
  composer_.applyPreset(preset, replace);
}

void AppController::createPreset(const QString& name, bool include_settings) {
  presets_.addFromJob(name, composer_.snapshot(), include_settings);
}

void AppController::editPreset(int row) {
  const PresetDefinition preset = presets_.definitionAt(row);
  if (preset.id.isEmpty()) {
    return;
  }
  composer_.applyPreset(preset, true);
  emit navigateToComposer();
}

void AppController::relinkPreset(int row, const QUrl& url) {
  presets_.relinkFirstMissing(row, url.toLocalFile());
}

QString AppController::localPath(const QUrl& url) const {
  return QDir::toNativeSeparators(url.toLocalFile());
}

}  // namespace cyan::gui
