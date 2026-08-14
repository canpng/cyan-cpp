#include "composer_controller.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QRegularExpression>
#include <QUuid>
#include <filesystem>
#include <utility>

#include "cyan/metadata/app_metadata_reader.hpp"
#include "input_icon_cache.hpp"

namespace cyan::gui {
namespace {

QString human_size(qint64 bytes) {
  if (bytes < 0) {
    return QStringLiteral("Klasör");
  }
  if (bytes < 1024) {
    return QString::number(bytes) + QStringLiteral(" B");
  }
  const double kib = static_cast<double>(bytes) / 1024.0;
  if (kib < 1024.0) {
    return QLocale().toString(kib, 'f', 1) + QStringLiteral(" KB");
  }
  return QLocale().toString(kib / 1024.0, 'f', 1) + QStringLiteral(" MB");
}

QString suffix_label(const QFileInfo& info) {
  const QString suffix = info.suffix().toLower();
  return suffix.isEmpty() ? QStringLiteral("Klasör") : QStringLiteral(".") + suffix;
}

}  // namespace

ComposerController::ComposerController(QObject* parent)
    : QObject(parent), injections_(this), cyan_packages_(this), payload_root_items_(this) {
  auto connect_model = [this](FileListModel& model) {
    connect(&model, &FileListModel::countChanged, this, &ComposerController::refreshDerived);
    connect(&model, &FileListModel::duplicateRejected, this, [this](const QString& name) {
      emit notification(QStringLiteral("%1 zaten listede.").arg(name));
    });
  };
  connect_model(injections_);
  connect_model(cyan_packages_);
  connect_model(payload_root_items_);
  connect(this, &ComposerController::formChanged, this, &ComposerController::refreshDerived);
}

QString ComposerController::inputPath() const { return input_path_; }
QString ComposerController::inputName() const { return input_name_; }
QString ComposerController::inputDetails() const { return input_details_; }
QUrl ComposerController::inputIconUrl() const { return input_icon_url_; }
QString ComposerController::inputError() const { return input_error_; }
FileListModel* ComposerController::injections() { return &injections_; }
FileListModel* ComposerController::cyanPackages() { return &cyan_packages_; }
FileListModel* ComposerController::payloadRootItems() { return &payload_root_items_; }
QString ComposerController::ipapatchError() const { return ipapatch_error_; }
bool ComposerController::canQueue() const { return validation_message_.isEmpty(); }
QString ComposerController::validationMessage() const { return validation_message_; }
bool ComposerController::editing() const { return !editing_id_.isEmpty(); }
QString ComposerController::editingId() const { return editing_id_; }

QString ComposerController::summary() const {
  const int total = injections_.count() + cyan_packages_.count();
  QStringList parts;
  parts << QStringLiteral("%1 enjeksiyon").arg(total);
  if (payload_root_items_.count() > 0) {
    parts << QStringLiteral("%1 Payload öğesi").arg(payload_root_items_.count());
  }
  parts << (ipapatch_enabled_ ? QStringLiteral("iPAPatch açık")
                              : QStringLiteral("iPAPatch kapalı"));
  parts << QStringLiteral("Sıkıştırma %1").arg(compression_);
  return parts.join(QStringLiteral("  •  "));
}

void ComposerController::setDefaults(QString output_directory, int compression, int ldid_mode,
                                     QString custom_ldid, QString dependency_directory) {
  default_output_directory_ = std::move(output_directory);
  default_compression_ = qBound(0, compression, 9);
  default_ldid_mode_ = ldid_mode;
  default_custom_ldid_ = std::move(custom_ldid);
  default_dependency_directory_ = std::move(dependency_directory);
  if (input_path_.isEmpty() && !editing() && injections_.count() == 0 &&
      cyan_packages_.count() == 0 && payload_root_items_.count() == 0) {
    clearForm(true);
  }
}

QString ComposerController::localPath(const QUrl& url) {
  if (url.isLocalFile()) {
    return QDir::toNativeSeparators(url.toLocalFile());
  }
  return QDir::toNativeSeparators(url.toString());
}

bool ComposerController::pathExists(const QString& path, bool directory) {
  if (path.trimmed().isEmpty()) {
    return false;
  }
  const QFileInfo info(path);
  return directory ? info.isDir() : info.exists();
}

void ComposerController::readInputMetadata(bool replace_form_values) {
  input_icon_url_.clear();
  input_error_.clear();
  if (input_path_.isEmpty()) {
    return;
  }

  const AppMetadataReader reader;
  auto result = reader.read(std::filesystem::path(input_path_.toStdWString()));
  if (!result) {
    input_error_ = QStringLiteral("Ana uygulamanın Info.plist verisi okunamadı: %1")
                       .arg(QString::fromUtf8(result.error().message));
    return;
  }

  const AppMetadata& metadata = result.value();
  const QString detected_name = QString::fromUtf8(metadata.app_name);
  if (!detected_name.isEmpty()) {
    input_name_ = detected_name;
  }
  if (replace_form_values) {
    app_name_ = detected_name;
    app_version_ = QString::fromUtf8(metadata.version);
    bundle_id_ = QString::fromUtf8(metadata.bundle_identifier);
    minimum_os_ = QString::fromUtf8(metadata.minimum_os);
  }
  input_icon_url_ = cache_application_icon(metadata.icon_data, input_icon_cache_);
}

void ComposerController::setInputUrl(const QUrl& url) {
  const QString path = localPath(url);
  const QFileInfo info(path);
  input_error_.clear();
  const QString suffix = info.suffix().toLower();
  const bool valid_archive =
      info.isFile() && (suffix == QStringLiteral("ipa") || suffix == QStringLiteral("tipa"));
  const bool valid_app = info.isDir() && suffix == QStringLiteral("app") &&
                         QFileInfo::exists(info.filePath() + QStringLiteral("/Info.plist"));
  if (!valid_archive && !valid_app) {
    input_error_ = QStringLiteral(".ipa, .tipa veya Info.plist içeren geçerli bir .app seçin.");
    emit validationChanged();
    return;
  }

  input_path_ = QDir::toNativeSeparators(info.absoluteFilePath());
  input_name_ = info.fileName();
  input_details_ =
      suffix_label(info) + QStringLiteral("  •  ") + human_size(info.isDir() ? -1 : info.size());
  readInputMetadata(true);
  const QString base = info.completeBaseName();
  output_file_name_ =
      base + QStringLiteral("_patched.") +
      (suffix == QStringLiteral("app") ? QStringLiteral("app") : QStringLiteral("ipa"));
  output_directory_ = default_output_directory_.isEmpty()
                          ? QDir::toNativeSeparators(info.absolutePath())
                          : default_output_directory_;
  emit inputChanged();
  emit formChanged();
}

void ComposerController::clearInput() {
  input_path_.clear();
  input_name_.clear();
  input_details_.clear();
  input_icon_url_.clear();
  input_error_.clear();
  app_name_.clear();
  app_version_.clear();
  bundle_id_.clear();
  minimum_os_.clear();
  output_file_name_.clear();
  emit inputChanged();
  emit formChanged();
}

void ComposerController::addInjectionPath(const QString& path) {
  const QFileInfo info(path);
  if (!info.exists()) {
    emit notification(QStringLiteral("Dosya bulunamadı: %1").arg(path));
    return;
  }
  if (info.suffix().compare(QStringLiteral("cyan"), Qt::CaseInsensitive) == 0) {
    cyan_packages_.addPath(path, QStringLiteral("Sıralı paket"));
  } else {
    injections_.addPath(path);
  }
}

void ComposerController::addPayloadPath(const QString& path) {
  if (!QFileInfo::exists(path)) {
    emit notification(QStringLiteral("Dosya bulunamadı: %1").arg(path));
    return;
  }
  payload_root_items_.addPath(path, QStringLiteral("Payload Root"));
}

void ComposerController::addContentPath(const QString& path) {
  const QFileInfo info(path);
  if (!info.exists()) {
    emit notification(QStringLiteral("Dosya bulunamadı: %1").arg(path));
    return;
  }

  const QString suffix = info.suffix().toLower();
  const bool cyan_content = suffix == QStringLiteral("cyan");
  const bool injectable_file =
      info.isFile() && (suffix == QStringLiteral("deb") || suffix == QStringLiteral("dylib"));
  const bool injectable_bundle =
      info.isDir() && (suffix == QStringLiteral("framework") ||
                       suffix == QStringLiteral("bundle") || suffix == QStringLiteral("appex"));

  if (cyan_content || injectable_file || injectable_bundle) {
    addInjectionPath(path);
    return;
  }

  addPayloadPath(path);
  emit notification(
      QStringLiteral("%1 desteklenen bir enjeksiyon türü olmadığı için Payload Root'a eklendi.")
          .arg(info.fileName()));
}

void ComposerController::addInjectionUrls(const QVariantList& urls) {
  for (const auto& value : urls) {
    addInjectionPath(localPath(value.toUrl()));
  }
}

void ComposerController::addInjectionUrl(const QUrl& url) { addInjectionPath(localPath(url)); }

void ComposerController::addPayloadUrls(const QVariantList& urls) {
  for (const auto& value : urls) {
    addPayloadPath(localPath(value.toUrl()));
  }
}

void ComposerController::addPayloadUrl(const QUrl& url) { addPayloadPath(localPath(url)); }

void ComposerController::addContentUrls(const QVariantList& urls) {
  for (const auto& value : urls) {
    addContentPath(localPath(value.toUrl()));
  }
}

void ComposerController::addContentUrl(const QUrl& url) { addContentPath(localPath(url)); }

QString ComposerController::outputPath() const {
  if (output_directory_.trimmed().isEmpty() || output_file_name_.trimmed().isEmpty()) {
    return {};
  }
  return QDir::toNativeSeparators(QDir(output_directory_).filePath(output_file_name_));
}

void ComposerController::refreshDerived() {
  QString message;
  QString patch_error;
  if (input_path_.isEmpty()) {
    message = QStringLiteral("Önce bir uygulama seçin.");
  } else if (!QFileInfo::exists(input_path_)) {
    message = QStringLiteral("Seçili uygulama artık bulunamıyor.");
  } else if (!input_error_.isEmpty()) {
    message = input_error_;
  } else if (output_file_name_.trimmed().isEmpty()) {
    message = QStringLiteral("Çıktı dosya adını yazın.");
  } else if (!QFileInfo(output_directory_).isDir()) {
    message = QStringLiteral("Geçerli bir kayıt dizini seçin.");
  } else if (compression_ < 0 || compression_ > 9) {
    message = QStringLiteral("Sıkıştırma seviyesi 0–9 arasında olmalıdır.");
  } else if (!minimum_os_.isEmpty() && !QRegularExpression(QStringLiteral(R"(^\d+(?:\.\d+)*$)"))
                                            .match(minimum_os_)
                                            .hasMatch()) {
    message = QStringLiteral("Minimum iOS sürümü yalnızca sayı ve nokta içerebilir.");
  }

  const QList<QPair<QString, QString>> optional_files = {
      {icon_path_, QStringLiteral("Uygulama ikonu bulunamadı.")},
      {plist_path_, QStringLiteral("Info.plist merge dosyası bulunamadı.")},
      {entitlements_path_, QStringLiteral("Entitlements dosyası bulunamadı.")}};
  if (message.isEmpty()) {
    for (const auto& [path, error] : optional_files) {
      if (!path.isEmpty() && !QFileInfo::exists(path)) {
        message = error;
        break;
      }
    }
  }
  if (message.isEmpty() && !dependency_directory_.isEmpty() &&
      !QFileInfo(dependency_directory_).isDir()) {
    message = QStringLiteral("Dependency Directory geçerli bir klasör olmalıdır.");
  }
  if (message.isEmpty()) {
    for (const auto* model : {&injections_, &cyan_packages_, &payload_root_items_}) {
      for (const auto& item : model->items()) {
        if (!QFileInfo::exists(item.path)) {
          message = QStringLiteral("Seçili dosya artık bulunamıyor: %1").arg(item.name);
          break;
        }
      }
      if (!message.isEmpty()) {
        break;
      }
    }
  }
  const QString bundled_ldid =
      QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("ldid.exe"));
  const bool signer_available =
      ldid_mode_ == 0 ? QFileInfo::exists(bundled_ldid) : QFileInfo::exists(custom_ldid_path_);
  const bool signer_needed = ipapatch_enabled_ || fake_sign_ || !entitlements_path_.isEmpty();
  if (signer_needed && !signer_available) {
    patch_error =
        ipapatch_enabled_
            ? QStringLiteral("iPAPatch için geçerli bir ldid.exe gereklidir.")
            : QStringLiteral("Seçili imzalama işlemi için geçerli bir ldid.exe gereklidir.");
    if (message.isEmpty()) {
      message = patch_error;
    }
  }
  if (ipapatch_enabled_ && ipapatch_payload_mode_ == 1 &&
      !QFileInfo::exists(custom_ipapatch_dylib_)) {
    patch_error = QStringLiteral("Özel iPAPatch payload dosyası bulunamadı.");
    if (message.isEmpty()) {
      message = patch_error;
    }
  }
  const QString bundled_ipapatch = QDir(QCoreApplication::applicationDirPath())
                                       .filePath(QStringLiteral("zxPluginsInject.dylib"));
  if (ipapatch_enabled_ && ipapatch_payload_mode_ == 0 && !QFileInfo::exists(bundled_ipapatch)) {
    patch_error = QStringLiteral("Bundled zxPluginsInject.dylib bulunamadı.");
    if (message.isEmpty()) {
      message = patch_error;
    }
  }
  if (ldid_mode_ == 1 && !custom_ldid_path_.isEmpty() && !QFileInfo::exists(custom_ldid_path_) &&
      message.isEmpty()) {
    message = QStringLiteral("Özel ldid.exe bulunamadı.");
  }
  if (!outputPath().isEmpty() && QFileInfo::exists(outputPath()) && !overwrite_existing_ &&
      message.isEmpty()) {
    message = QStringLiteral("Çıktı zaten var; farklı bir ad seçin veya üzerine yazmayı açın.");
  }

  validation_message_ = message;
  ipapatch_error_ = patch_error;
  emit validationChanged();
  emit summaryChanged();
}

JobDefinition ComposerController::snapshot() const {
  JobDefinition job;
  job.id = editing_id_.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : editing_id_;
  job.preset_name = selected_preset_name_;
  job.input_path = input_path_;
  job.output_path = outputPath();
  job.injections = injections_.items();
  job.cyan_packages = cyan_packages_.items();
  job.payload_root_items = payload_root_items_.items();
  job.app_name = app_name_;
  job.app_version = app_version_;
  job.bundle_id = bundle_id_;
  job.minimum_os = minimum_os_;
  job.icon_path = icon_path_;
  job.plist_path = plist_path_;
  job.entitlements_path = entitlements_path_;
  job.remove_supported_devices = remove_supported_devices_;
  job.no_watch = no_watch_;
  job.enable_documents = enable_documents_;
  job.fake_sign = fake_sign_;
  job.thin = thin_;
  job.ignore_encrypted = ignore_encrypted_;
  job.extension_mode = extension_mode_;
  job.custom_ldid_path = ldid_mode_ == 1 ? custom_ldid_path_ : QString{};
  job.dependency_directory = dependency_directory_;
  job.compatibility_mode = compatibility_mode_;
  job.ipapatch_enabled = ipapatch_enabled_;
  job.custom_ipapatch_dylib = ipapatch_payload_mode_ == 1 ? custom_ipapatch_dylib_ : QString{};
  job.ipapatch_plugins_only = ipapatch_plugins_only_;
  job.overwrite = overwrite_existing_;
  job.compression_level = compression_;
  return job;
}

void ComposerController::load(const JobDefinition& job) {
  editing_id_ = job.id;
  selected_preset_name_ = job.preset_name;
  input_path_ = job.input_path;
  const QFileInfo input(job.input_path);
  input_name_ = input.fileName();
  input_details_ =
      suffix_label(input) + QStringLiteral("  •  ") + human_size(input.isDir() ? -1 : input.size());
  readInputMetadata(false);
  injections_.setItems(job.injections);
  cyan_packages_.setItems(job.cyan_packages);
  payload_root_items_.setItems(job.payload_root_items);
  app_name_ = job.app_name;
  app_version_ = job.app_version;
  bundle_id_ = job.bundle_id;
  minimum_os_ = job.minimum_os;
  icon_path_ = job.icon_path;
  plist_path_ = job.plist_path;
  entitlements_path_ = job.entitlements_path;
  remove_supported_devices_ = job.remove_supported_devices;
  no_watch_ = job.no_watch;
  enable_documents_ = job.enable_documents;
  fake_sign_ = job.fake_sign;
  thin_ = job.thin;
  ignore_encrypted_ = job.ignore_encrypted;
  extension_mode_ = job.extension_mode;
  ldid_mode_ = job.custom_ldid_path.isEmpty() ? 0 : 1;
  custom_ldid_path_ = job.custom_ldid_path;
  dependency_directory_ = job.dependency_directory;
  compatibility_mode_ = job.compatibility_mode;
  ipapatch_enabled_ = job.ipapatch_enabled;
  ipapatch_payload_mode_ = job.custom_ipapatch_dylib.isEmpty() ? 0 : 1;
  custom_ipapatch_dylib_ = job.custom_ipapatch_dylib;
  ipapatch_plugins_only_ = job.ipapatch_plugins_only;
  const QFileInfo output(job.output_path);
  output_directory_ = QDir::toNativeSeparators(output.absolutePath());
  output_file_name_ = output.fileName();
  overwrite_existing_ = job.overwrite;
  compression_ = job.compression_level;
  emit editingChanged();
  emit inputChanged();
  emit formChanged();
}

void ComposerController::applyPreset(const PresetDefinition& preset, bool replace) {
  if (replace) {
    injections_.clear();
    cyan_packages_.clear();
    payload_root_items_.clear();
  }
  for (const auto& item : preset.injections) {
    injections_.addPath(item.path);
  }
  for (const auto& item : preset.cyan_packages) {
    cyan_packages_.addPath(item.path, QStringLiteral("Sıralı paket"));
  }
  for (const auto& item : preset.payload_root_items) {
    payload_root_items_.addPath(item.path, QStringLiteral("Payload Root"));
  }
  selected_preset_name_ = preset.name;
  if (preset.include_settings) {
    const JobDefinition& job = preset.settings;
    app_name_ = job.app_name;
    app_version_ = job.app_version;
    bundle_id_ = job.bundle_id;
    minimum_os_ = job.minimum_os;
    icon_path_ = job.icon_path;
    plist_path_ = job.plist_path;
    entitlements_path_ = job.entitlements_path;
    remove_supported_devices_ = job.remove_supported_devices;
    no_watch_ = job.no_watch;
    enable_documents_ = job.enable_documents;
    fake_sign_ = job.fake_sign;
    thin_ = job.thin;
    ignore_encrypted_ = job.ignore_encrypted;
    extension_mode_ = job.extension_mode;
    custom_ldid_path_ = job.custom_ldid_path;
    ldid_mode_ = custom_ldid_path_.isEmpty() ? 0 : 1;
    dependency_directory_ = job.dependency_directory;
    compatibility_mode_ = job.compatibility_mode;
    ipapatch_enabled_ = job.ipapatch_enabled;
    custom_ipapatch_dylib_ = job.custom_ipapatch_dylib;
    ipapatch_payload_mode_ = custom_ipapatch_dylib_.isEmpty() ? 0 : 1;
    ipapatch_plugins_only_ = job.ipapatch_plugins_only;
    compression_ = job.compression_level;
  }
  emit formChanged();
  emit notification(QStringLiteral("%1 önayarı uygulandı.").arg(preset.name));
}

void ComposerController::clearForm(bool preserve_defaults) {
  const QString output_default = preserve_defaults ? default_output_directory_ : QString{};
  const int compression_default = preserve_defaults ? default_compression_ : 6;
  const int ldid_default = preserve_defaults ? default_ldid_mode_ : 0;
  const QString custom_ldid_default = preserve_defaults ? default_custom_ldid_ : QString{};
  const QString dependency_default = preserve_defaults ? default_dependency_directory_ : QString{};
  input_path_.clear();
  input_name_.clear();
  input_details_.clear();
  input_icon_url_.clear();
  input_error_.clear();
  selected_preset_name_.clear();
  injections_.clear();
  cyan_packages_.clear();
  payload_root_items_.clear();
  app_name_.clear();
  app_version_.clear();
  bundle_id_.clear();
  minimum_os_.clear();
  icon_path_.clear();
  plist_path_.clear();
  entitlements_path_.clear();
  remove_supported_devices_ = true;
  no_watch_ = true;
  enable_documents_ = true;
  fake_sign_ = true;
  thin_ = true;
  ignore_encrypted_ = false;
  extension_mode_ = 0;
  ldid_mode_ = ldid_default;
  custom_ldid_path_ = custom_ldid_default;
  dependency_directory_ = dependency_default;
  compatibility_mode_ = false;
  ipapatch_enabled_ = false;
  ipapatch_payload_mode_ = 0;
  custom_ipapatch_dylib_.clear();
  ipapatch_plugins_only_ = false;
  output_directory_ = output_default;
  output_file_name_.clear();
  overwrite_existing_ = false;
  compression_ = compression_default;
  validation_message_.clear();
  ipapatch_error_.clear();
  editing_id_.clear();
  emit editingChanged();
  emit inputChanged();
  emit formChanged();
}

void ComposerController::reset() { clearForm(true); }

}  // namespace cyan::gui
