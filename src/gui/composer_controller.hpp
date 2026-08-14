#pragma once

#include <QObject>
#include <QTemporaryDir>
#include <QUrl>
#include <QVariantList>

#include "file_list_model.hpp"
#include "job_definition.hpp"

namespace cyan::gui {

class ComposerController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString inputPath READ inputPath NOTIFY inputChanged)
  Q_PROPERTY(QString inputName READ inputName NOTIFY inputChanged)
  Q_PROPERTY(QString inputDetails READ inputDetails NOTIFY inputChanged)
  Q_PROPERTY(QUrl inputIconUrl READ inputIconUrl NOTIFY inputChanged)
  Q_PROPERTY(QString inputError READ inputError NOTIFY validationChanged)
  Q_PROPERTY(QString selectedPresetName MEMBER selected_preset_name_ NOTIFY formChanged)

  Q_PROPERTY(FileListModel* injections READ injections CONSTANT)
  Q_PROPERTY(FileListModel* cyanPackages READ cyanPackages CONSTANT)
  Q_PROPERTY(FileListModel* payloadRootItems READ payloadRootItems CONSTANT)

  Q_PROPERTY(QString appName MEMBER app_name_ NOTIFY formChanged)
  Q_PROPERTY(QString appVersion MEMBER app_version_ NOTIFY formChanged)
  Q_PROPERTY(QString bundleId MEMBER bundle_id_ NOTIFY formChanged)
  Q_PROPERTY(QString minimumOs MEMBER minimum_os_ NOTIFY formChanged)
  Q_PROPERTY(QString iconPath MEMBER icon_path_ NOTIFY formChanged)
  Q_PROPERTY(QString plistPath MEMBER plist_path_ NOTIFY formChanged)
  Q_PROPERTY(QString entitlementsPath MEMBER entitlements_path_ NOTIFY formChanged)

  Q_PROPERTY(bool removeSupportedDevices MEMBER remove_supported_devices_ NOTIFY formChanged)
  Q_PROPERTY(bool noWatch MEMBER no_watch_ NOTIFY formChanged)
  Q_PROPERTY(bool enableDocuments MEMBER enable_documents_ NOTIFY formChanged)
  Q_PROPERTY(bool fakeSign MEMBER fake_sign_ NOTIFY formChanged)
  Q_PROPERTY(bool thin MEMBER thin_ NOTIFY formChanged)
  Q_PROPERTY(bool ignoreEncrypted MEMBER ignore_encrypted_ NOTIFY formChanged)
  Q_PROPERTY(int extensionMode MEMBER extension_mode_ NOTIFY formChanged)

  Q_PROPERTY(int ldidMode MEMBER ldid_mode_ NOTIFY formChanged)
  Q_PROPERTY(QString customLdidPath MEMBER custom_ldid_path_ NOTIFY formChanged)
  Q_PROPERTY(QString dependencyDirectory MEMBER dependency_directory_ NOTIFY formChanged)
  Q_PROPERTY(bool compatibilityMode MEMBER compatibility_mode_ NOTIFY formChanged)

  Q_PROPERTY(bool ipapatchEnabled MEMBER ipapatch_enabled_ NOTIFY formChanged)
  Q_PROPERTY(int ipapatchPayloadMode MEMBER ipapatch_payload_mode_ NOTIFY formChanged)
  Q_PROPERTY(QString customIpapatchDylib MEMBER custom_ipapatch_dylib_ NOTIFY formChanged)
  Q_PROPERTY(bool ipapatchPluginsOnly MEMBER ipapatch_plugins_only_ NOTIFY formChanged)
  Q_PROPERTY(QString ipapatchError READ ipapatchError NOTIFY validationChanged)

  Q_PROPERTY(QString outputDirectory MEMBER output_directory_ NOTIFY formChanged)
  Q_PROPERTY(QString outputFileName MEMBER output_file_name_ NOTIFY formChanged)
  Q_PROPERTY(bool overwriteExisting MEMBER overwrite_existing_ NOTIFY formChanged)
  Q_PROPERTY(int compression MEMBER compression_ NOTIFY formChanged)

  Q_PROPERTY(bool canQueue READ canQueue NOTIFY validationChanged)
  Q_PROPERTY(QString validationMessage READ validationMessage NOTIFY validationChanged)
  Q_PROPERTY(QString summary READ summary NOTIFY summaryChanged)
  Q_PROPERTY(bool editing READ editing NOTIFY editingChanged)

 public:
  explicit ComposerController(QObject* parent = nullptr);

  [[nodiscard]] QString inputPath() const;
  [[nodiscard]] QString inputName() const;
  [[nodiscard]] QString inputDetails() const;
  [[nodiscard]] QUrl inputIconUrl() const;
  [[nodiscard]] QString inputError() const;
  [[nodiscard]] FileListModel* injections();
  [[nodiscard]] FileListModel* cyanPackages();
  [[nodiscard]] FileListModel* payloadRootItems();
  [[nodiscard]] QString ipapatchError() const;
  [[nodiscard]] bool canQueue() const;
  [[nodiscard]] QString validationMessage() const;
  [[nodiscard]] QString summary() const;
  [[nodiscard]] bool editing() const;
  [[nodiscard]] QString editingId() const;

  void setDefaults(QString output_directory, int compression, int ldid_mode, QString custom_ldid,
                   QString dependency_directory);
  [[nodiscard]] JobDefinition snapshot() const;
  void load(const JobDefinition& job);
  void applyPreset(const PresetDefinition& preset, bool replace);

  Q_INVOKABLE void setInputUrl(const QUrl& url);
  Q_INVOKABLE void clearInput();
  Q_INVOKABLE void addInjectionUrls(const QVariantList& urls);
  Q_INVOKABLE void addInjectionUrl(const QUrl& url);
  Q_INVOKABLE void addPayloadUrls(const QVariantList& urls);
  Q_INVOKABLE void addPayloadUrl(const QUrl& url);
  Q_INVOKABLE void addContentUrls(const QVariantList& urls);
  Q_INVOKABLE void addContentUrl(const QUrl& url);
  Q_INVOKABLE void reset();

 signals:
  void inputChanged();
  void formChanged();
  void validationChanged();
  void summaryChanged();
  void editingChanged();
  void notification(const QString& message);

 private:
  static QString localPath(const QUrl& url);
  static bool pathExists(const QString& path, bool directory = false);
  void addInjectionPath(const QString& path);
  void addPayloadPath(const QString& path);
  void addContentPath(const QString& path);
  void readInputMetadata(bool replace_form_values);
  void refreshDerived();
  void clearForm(bool preserve_defaults);
  [[nodiscard]] QString outputPath() const;

  FileListModel injections_;
  FileListModel cyan_packages_;
  FileListModel payload_root_items_;

  QString input_path_;
  QString input_name_;
  QString input_details_;
  QUrl input_icon_url_;
  QTemporaryDir input_icon_cache_;
  QString input_error_;
  QString selected_preset_name_;

  QString app_name_;
  QString app_version_;
  QString bundle_id_;
  QString minimum_os_;
  QString icon_path_;
  QString plist_path_;
  QString entitlements_path_;
  bool remove_supported_devices_{true};
  bool no_watch_{true};
  bool enable_documents_{true};
  bool fake_sign_{true};
  bool thin_{true};
  bool ignore_encrypted_{false};
  int extension_mode_{0};
  int ldid_mode_{0};
  QString custom_ldid_path_;
  QString dependency_directory_;
  bool compatibility_mode_{false};
  bool ipapatch_enabled_{false};
  int ipapatch_payload_mode_{0};
  QString custom_ipapatch_dylib_;
  bool ipapatch_plugins_only_{false};
  QString output_directory_;
  QString output_file_name_;
  bool overwrite_existing_{false};
  int compression_{6};

  QString default_output_directory_;
  int default_compression_{6};
  int default_ldid_mode_{0};
  QString default_custom_ldid_;
  QString default_dependency_directory_;
  QString validation_message_;
  QString ipapatch_error_;
  QString editing_id_;
};

}  // namespace cyan::gui
