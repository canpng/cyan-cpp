#pragma once

#include <QAbstractListModel>

#include "job_definition.hpp"

namespace cyan::gui {

class PresetModel final : public QAbstractListModel {
  Q_OBJECT
  Q_PROPERTY(int count READ count NOTIFY countChanged)

 public:
  enum Role {
    IdRole = Qt::UserRole + 1,
    NameRole,
    SubtitleRole,
    MissingCountRole,
    MissingSummaryRole,
    IncludesSettingsRole
  };

  explicit PresetModel(QObject* parent = nullptr);

  [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
  [[nodiscard]] int count() const;
  [[nodiscard]] PresetDefinition definitionAt(int row) const;

  void addFromJob(const QString& name, const JobDefinition& job, bool include_settings);
  Q_INVOKABLE void removePreset(int row);
  Q_INVOKABLE void duplicatePreset(int row);
  Q_INVOKABLE void renamePreset(int row, const QString& name);
  Q_INVOKABLE void relinkFirstMissing(int row, const QString& new_path);

 signals:
  void countChanged();
  void notification(const QString& message);

 private:
  void load();
  void save() const;
  [[nodiscard]] QString storagePath() const;
  [[nodiscard]] static int missingCount(const PresetDefinition& preset);
  [[nodiscard]] static QString firstMissing(const PresetDefinition& preset);

  QList<PresetDefinition> presets_;
};

}  // namespace cyan::gui
