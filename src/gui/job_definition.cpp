#include "job_definition.hpp"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <filesystem>
#include <optional>

namespace cyan::gui {
namespace {

std::filesystem::path fs_path(const QString& value) {
  return std::filesystem::path(value.toStdWString());
}

std::optional<std::filesystem::path> optional_path(const QString& value) {
  if (value.trimmed().isEmpty()) {
    return std::nullopt;
  }
  return fs_path(value);
}

std::optional<std::wstring> optional_text(const QString& value) {
  if (value.trimmed().isEmpty()) {
    return std::nullopt;
  }
  return value.toStdWString();
}

QJsonArray files_to_json(const QList<FileItem>& files) {
  QJsonArray result;
  for (const auto& item : files) {
    result.append(item.path);
  }
  return result;
}

QString target_for_suffix(const QString& suffix) {
  if (suffix == QStringLiteral("dylib") || suffix == QStringLiteral("framework")) {
    return QStringLiteral("Frameworks");
  }
  if (suffix == QStringLiteral("appex")) {
    return QStringLiteral("PlugIns");
  }
  if (suffix == QStringLiteral("deb")) {
    return QStringLiteral("Auto");
  }
  if (suffix == QStringLiteral("cyan")) {
    return QStringLiteral("Sıralı paket");
  }
  return QStringLiteral("App Root");
}

FileItem file_from_path(const QString& path, const QString& forced_target = {}) {
  const QFileInfo info(path);
  const QString suffix = info.suffix().toLower();
  FileItem result;
  result.path = QDir::toNativeSeparators(info.absoluteFilePath());
  result.name = info.fileName();
  result.type = suffix.isEmpty()
                    ? (info.isDir() ? QStringLiteral("Klasör") : QStringLiteral("Dosya"))
                    : QStringLiteral(".") + suffix;
  result.target = forced_target.isEmpty() ? target_for_suffix(suffix) : forced_target;
  result.size = info.isFile() ? info.size() : -1;
  return result;
}

QList<FileItem> files_from_json(const QJsonValue& value, const QString& target = {}) {
  QList<FileItem> result;
  for (const auto& path : value.toArray()) {
    result.append(file_from_path(path.toString(), target));
  }
  return result;
}

QJsonObject settings_to_json(const JobDefinition& value) {
  QJsonObject object;
  object[QStringLiteral("appName")] = value.app_name;
  object[QStringLiteral("appVersion")] = value.app_version;
  object[QStringLiteral("bundleId")] = value.bundle_id;
  object[QStringLiteral("minimumOs")] = value.minimum_os;
  object[QStringLiteral("iconPath")] = value.icon_path;
  object[QStringLiteral("plistPath")] = value.plist_path;
  object[QStringLiteral("entitlementsPath")] = value.entitlements_path;
  object[QStringLiteral("removeSupportedDevices")] = value.remove_supported_devices;
  object[QStringLiteral("noWatch")] = value.no_watch;
  object[QStringLiteral("enableDocuments")] = value.enable_documents;
  object[QStringLiteral("fakeSign")] = value.fake_sign;
  object[QStringLiteral("thin")] = value.thin;
  object[QStringLiteral("ignoreEncrypted")] = value.ignore_encrypted;
  object[QStringLiteral("extensionMode")] = value.extension_mode;
  object[QStringLiteral("customLdidPath")] = value.custom_ldid_path;
  object[QStringLiteral("dependencyDirectory")] = value.dependency_directory;
  object[QStringLiteral("compatibilityMode")] = value.compatibility_mode;
  object[QStringLiteral("ipapatchEnabled")] = value.ipapatch_enabled;
  object[QStringLiteral("customIpapatchDylib")] = value.custom_ipapatch_dylib;
  object[QStringLiteral("ipapatchPluginsOnly")] = value.ipapatch_plugins_only;
  object[QStringLiteral("compressionLevel")] = value.compression_level;
  return object;
}

JobDefinition settings_from_json(const QJsonObject& object) {
  JobDefinition value;
  value.app_name = object[QStringLiteral("appName")].toString();
  value.app_version = object[QStringLiteral("appVersion")].toString();
  value.bundle_id = object[QStringLiteral("bundleId")].toString();
  value.minimum_os = object[QStringLiteral("minimumOs")].toString();
  value.icon_path = object[QStringLiteral("iconPath")].toString();
  value.plist_path = object[QStringLiteral("plistPath")].toString();
  value.entitlements_path = object[QStringLiteral("entitlementsPath")].toString();
  value.remove_supported_devices = object[QStringLiteral("removeSupportedDevices")].toBool();
  value.no_watch = object[QStringLiteral("noWatch")].toBool();
  value.enable_documents = object[QStringLiteral("enableDocuments")].toBool();
  value.fake_sign = object[QStringLiteral("fakeSign")].toBool();
  value.thin = object[QStringLiteral("thin")].toBool();
  value.ignore_encrypted = object[QStringLiteral("ignoreEncrypted")].toBool();
  value.extension_mode = object[QStringLiteral("extensionMode")].toInt();
  value.custom_ldid_path = object[QStringLiteral("customLdidPath")].toString();
  value.dependency_directory = object[QStringLiteral("dependencyDirectory")].toString();
  value.compatibility_mode = object[QStringLiteral("compatibilityMode")].toBool();
  value.ipapatch_enabled = object[QStringLiteral("ipapatchEnabled")].toBool();
  value.custom_ipapatch_dylib = object[QStringLiteral("customIpapatchDylib")].toString();
  value.ipapatch_plugins_only = object[QStringLiteral("ipapatchPluginsOnly")].toBool();
  value.compression_level = object[QStringLiteral("compressionLevel")].toInt(6);
  return value;
}

}  // namespace

CyanOptions JobDefinition::to_cyan_options() const {
  CyanOptions options;
  options.input = fs_path(input_path);
  options.output = fs_path(output_path);
  for (const auto& item : cyan_packages) {
    options.cyan_files.push_back(fs_path(item.path));
  }
  for (const auto& item : injections) {
    options.injected_items.push_back(fs_path(item.path));
  }
  for (const auto& item : payload_root_items) {
    options.payload_root_items.push_back(fs_path(item.path));
  }
  options.name = optional_text(app_name);
  options.version = optional_text(app_version);
  options.bundle_id = optional_text(bundle_id);
  options.minimum_os = optional_text(minimum_os);
  options.icon = optional_path(icon_path);
  options.merge_plist = optional_path(plist_path);
  options.entitlements = optional_path(entitlements_path);
  options.remove_supported_devices = remove_supported_devices;
  options.no_watch = no_watch;
  options.enable_documents = enable_documents;
  options.fakesign = fake_sign;
  options.thin = thin;
  options.ignore_encrypted = ignore_encrypted;
  options.remove_extensions = extension_mode == 1;
  options.remove_encrypted = extension_mode == 2;
  options.ldid_path = optional_path(custom_ldid_path);
  options.dependency_directory = optional_path(dependency_directory);
  options.compatibility_cyan = compatibility_mode;
  options.ipapatch = ipapatch_enabled;
  options.ipapatch_dylib = optional_path(custom_ipapatch_dylib);
  options.ipapatch_plugins_only = ipapatch_plugins_only;
  options.overwrite = overwrite;
  options.compression_level = compression_level;
  return options;
}

QJsonObject PresetDefinition::to_json() const {
  QJsonObject object;
  object[QStringLiteral("id")] = id;
  object[QStringLiteral("name")] = name;
  object[QStringLiteral("injections")] = files_to_json(injections);
  object[QStringLiteral("cyanPackages")] = files_to_json(cyan_packages);
  object[QStringLiteral("payloadRootItems")] = files_to_json(payload_root_items);
  object[QStringLiteral("includeSettings")] = include_settings;
  if (include_settings) {
    object[QStringLiteral("settings")] = settings_to_json(settings);
  }
  return object;
}

PresetDefinition PresetDefinition::from_json(const QJsonObject& object) {
  PresetDefinition value;
  value.id = object[QStringLiteral("id")].toString();
  value.name = object[QStringLiteral("name")].toString();
  value.injections = files_from_json(object[QStringLiteral("injections")]);
  value.cyan_packages = files_from_json(object[QStringLiteral("cyanPackages")]);
  value.payload_root_items =
      files_from_json(object[QStringLiteral("payloadRootItems")], QStringLiteral("Payload Root"));
  value.include_settings = object[QStringLiteral("includeSettings")].toBool();
  if (value.include_settings) {
    value.settings = settings_from_json(object[QStringLiteral("settings")].toObject());
  }
  return value;
}

}  // namespace cyan::gui
