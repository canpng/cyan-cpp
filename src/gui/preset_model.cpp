#include "preset_model.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>
#include <utility>

namespace cyan::gui {

PresetModel::PresetModel(QObject* parent) : QAbstractListModel(parent) { load(); }

int PresetModel::rowCount(const QModelIndex& parent) const {
  return parent.isValid() ? 0 : presets_.size();
}

QVariant PresetModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= presets_.size()) {
    return {};
  }
  const auto& preset = presets_.at(index.row());
  switch (role) {
    case IdRole:
      return preset.id;
    case NameRole:
      return preset.name;
    case SubtitleRole: {
      QStringList parts;
      parts << QStringLiteral("%1 enjeksiyon").arg(preset.injections.size());
      if (!preset.cyan_packages.isEmpty()) {
        parts << QStringLiteral("%1 cyan paketi").arg(preset.cyan_packages.size());
      }
      if (!preset.payload_root_items.isEmpty()) {
        parts << QStringLiteral("%1 Payload öğesi").arg(preset.payload_root_items.size());
      }
      return parts.join(QStringLiteral("  ·  "));
    }
    case MissingCountRole:
      return missingCount(preset);
    case MissingSummaryRole: {
      const QString missing = firstMissing(preset);
      return missing.isEmpty() ? QString{} : QStringLiteral("Dosya bulunamadı: %1").arg(missing);
    }
    case IncludesSettingsRole:
      return preset.include_settings;
    default:
      return {};
  }
}

QHash<int, QByteArray> PresetModel::roleNames() const {
  return {{IdRole, "presetId"},
          {NameRole, "name"},
          {SubtitleRole, "subtitle"},
          {MissingCountRole, "missingCount"},
          {MissingSummaryRole, "missingSummary"},
          {IncludesSettingsRole, "includesSettings"}};
}

int PresetModel::count() const { return presets_.size(); }

PresetDefinition PresetModel::definitionAt(int row) const {
  return row >= 0 && row < presets_.size() ? presets_.at(row) : PresetDefinition{};
}

void PresetModel::addFromJob(const QString& name, const JobDefinition& job, bool include_settings) {
  const QString trimmed = name.trimmed();
  if (trimmed.isEmpty()) {
    emit notification(QStringLiteral("Önayar adı boş olamaz."));
    return;
  }
  PresetDefinition preset;
  preset.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  preset.name = trimmed;
  preset.injections = job.injections;
  preset.cyan_packages = job.cyan_packages;
  preset.payload_root_items = job.payload_root_items;
  preset.include_settings = include_settings;
  if (include_settings) {
    preset.settings = job;
  }
  const int row = presets_.size();
  beginInsertRows({}, row, row);
  presets_.append(std::move(preset));
  endInsertRows();
  save();
  emit countChanged();
}

void PresetModel::removePreset(int row) {
  if (row < 0 || row >= presets_.size()) {
    return;
  }
  beginRemoveRows({}, row, row);
  presets_.removeAt(row);
  endRemoveRows();
  save();
  emit countChanged();
}

void PresetModel::duplicatePreset(int row) {
  if (row < 0 || row >= presets_.size()) {
    return;
  }
  PresetDefinition copy = presets_.at(row);
  copy.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  copy.name += QStringLiteral(" Kopya");
  const int destination = presets_.size();
  beginInsertRows({}, destination, destination);
  presets_.append(std::move(copy));
  endInsertRows();
  save();
  emit countChanged();
}

void PresetModel::renamePreset(int row, const QString& name) {
  if (row < 0 || row >= presets_.size() || name.trimmed().isEmpty()) {
    return;
  }
  presets_[row].name = name.trimmed();
  emit dataChanged(index(row), index(row), {NameRole});
  save();
}

void PresetModel::relinkFirstMissing(int row, const QString& new_path) {
  if (row < 0 || row >= presets_.size() || !QFileInfo::exists(new_path)) {
    return;
  }
  auto replace_first = [&](QList<FileItem>& items) {
    for (auto& item : items) {
      if (!QFileInfo::exists(item.path)) {
        const QFileInfo info(new_path);
        item.path = QDir::toNativeSeparators(info.absoluteFilePath());
        item.name = info.fileName();
        item.size = info.isFile() ? info.size() : -1;
        return true;
      }
    }
    return false;
  };
  auto& preset = presets_[row];
  if (!replace_first(preset.injections) && !replace_first(preset.cyan_packages)) {
    replace_first(preset.payload_root_items);
  }
  emit dataChanged(index(row), index(row));
  save();
}

QString PresetModel::storagePath() const {
  const QString root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  return QDir(root).filePath(QStringLiteral("presets.json"));
}

void PresetModel::load() {
  QFile file(storagePath());
  if (!file.open(QIODevice::ReadOnly)) {
    return;
  }
  const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
  const QJsonArray array = document.object()[QStringLiteral("presets")].toArray();
  beginResetModel();
  presets_.clear();
  for (const auto& value : array) {
    auto preset = PresetDefinition::from_json(value.toObject());
    if (preset.id.isEmpty()) {
      preset.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    if (!preset.name.isEmpty()) {
      presets_.append(std::move(preset));
    }
  }
  endResetModel();
  emit countChanged();
}

void PresetModel::save() const {
  const QFileInfo info(storagePath());
  QDir().mkpath(info.absolutePath());
  QJsonArray array;
  for (const auto& preset : presets_) {
    array.append(preset.to_json());
  }
  QJsonObject root;
  root[QStringLiteral("version")] = 1;
  root[QStringLiteral("presets")] = array;
  QSaveFile file(storagePath());
  if (!file.open(QIODevice::WriteOnly)) {
    return;
  }
  file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
  file.commit();
}

int PresetModel::missingCount(const PresetDefinition& preset) {
  int count = 0;
  auto inspect = [&](const QList<FileItem>& items) {
    for (const auto& item : items) {
      if (!QFileInfo::exists(item.path)) {
        ++count;
      }
    }
  };
  inspect(preset.injections);
  inspect(preset.cyan_packages);
  inspect(preset.payload_root_items);
  return count;
}

QString PresetModel::firstMissing(const PresetDefinition& preset) {
  for (const auto* items :
       {&preset.injections, &preset.cyan_packages, &preset.payload_root_items}) {
    for (const auto& item : *items) {
      if (!QFileInfo::exists(item.path)) {
        return item.path;
      }
    }
  }
  return {};
}

}  // namespace cyan::gui
