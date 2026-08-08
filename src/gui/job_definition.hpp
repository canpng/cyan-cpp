#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

#include "cyan/core/options.hpp"

namespace cyan::gui {

struct FileItem {
  QString path;
  QString name;
  QString type;
  QString target;
  qint64 size{-1};
};

struct JobDefinition {
  QString id;
  QString preset_name;
  QString input_path;
  QString output_path;
  QList<FileItem> injections;
  QList<FileItem> cyan_packages;
  QList<FileItem> payload_root_items;

  QString app_name;
  QString app_version;
  QString bundle_id;
  QString minimum_os;
  QString icon_path;
  QString plist_path;
  QString entitlements_path;

  bool remove_supported_devices{false};
  bool no_watch{false};
  bool enable_documents{false};
  bool fake_sign{false};
  bool thin{false};
  bool ignore_encrypted{false};
  int extension_mode{0};

  QString custom_ldid_path;
  QString dependency_directory;
  bool compatibility_mode{false};

  bool ipapatch_enabled{false};
  QString custom_ipapatch_dylib;
  bool ipapatch_plugins_only{false};

  bool overwrite{false};
  int compression_level{6};

  [[nodiscard]] CyanOptions to_cyan_options() const;
};

struct PresetDefinition {
  QString id;
  QString name;
  QList<FileItem> injections;
  QList<FileItem> cyan_packages;
  QList<FileItem> payload_root_items;
  bool include_settings{false};
  JobDefinition settings;

  [[nodiscard]] QJsonObject to_json() const;
  static PresetDefinition from_json(const QJsonObject& object);
};

}  // namespace cyan::gui

Q_DECLARE_METATYPE(cyan::gui::JobDefinition)
