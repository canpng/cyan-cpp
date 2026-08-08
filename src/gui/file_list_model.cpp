#include "file_list_model.hpp"

#include <QDir>
#include <QFileInfo>
#include <utility>

namespace cyan::gui {
namespace {

QString normalized_path(const QString& path) {
  const QFileInfo info(path);
  const QString canonical = info.canonicalFilePath();
  return QDir::toNativeSeparators(canonical.isEmpty() ? info.absoluteFilePath() : canonical);
}

QString target_for(const QFileInfo& info) {
  const QString suffix = info.suffix().toLower();
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

FileItem make_item(const QString& path, const QString& forced_target) {
  const QFileInfo info(path);
  FileItem item;
  item.path = normalized_path(path);
  item.name = info.fileName();
  const QString suffix = info.suffix().toLower();
  item.type = suffix.isEmpty() ? (info.isDir() ? QStringLiteral("Klasör") : QStringLiteral("Dosya"))
                               : QStringLiteral(".") + suffix;
  item.target = forced_target.isEmpty() ? target_for(info) : forced_target;
  item.size = info.isFile() ? info.size() : -1;
  return item;
}

}  // namespace

FileListModel::FileListModel(QObject* parent) : QAbstractListModel(parent) {}

int FileListModel::rowCount(const QModelIndex& parent) const {
  return parent.isValid() ? 0 : items_.size();
}

QVariant FileListModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= items_.size()) {
    return {};
  }
  const auto& item = items_.at(index.row());
  switch (role) {
    case PathRole:
      return item.path;
    case NameRole:
      return item.name;
    case TypeRole:
      return item.type;
    case TargetRole:
      return item.target;
    case SizeRole:
      return item.size;
    case MissingRole:
      return !QFileInfo::exists(item.path);
    default:
      return {};
  }
}

QHash<int, QByteArray> FileListModel::roleNames() const {
  return {{PathRole, "path"},     {NameRole, "name"}, {TypeRole, "type"},
          {TargetRole, "target"}, {SizeRole, "size"}, {MissingRole, "missing"}};
}

int FileListModel::count() const { return items_.size(); }

const QList<FileItem>& FileListModel::items() const { return items_; }

bool FileListModel::addPath(const QString& path, const QString& forced_target) {
  const QString normalized = normalized_path(path);
  for (const auto& item : items_) {
    if (QString::compare(item.path, normalized, Qt::CaseInsensitive) == 0) {
      emit duplicateRejected(item.name);
      return false;
    }
  }
  const int row = items_.size();
  beginInsertRows({}, row, row);
  items_.append(make_item(path, forced_target));
  endInsertRows();
  emit countChanged();
  return true;
}

void FileListModel::setItems(QList<FileItem> items) {
  beginResetModel();
  items_ = std::move(items);
  endResetModel();
  emit countChanged();
}

void FileListModel::remove(int row) {
  if (row < 0 || row >= items_.size()) {
    return;
  }
  beginRemoveRows({}, row, row);
  items_.removeAt(row);
  endRemoveRows();
  emit countChanged();
}

void FileListModel::move(int from, int to) {
  if (from < 0 || from >= items_.size() || to < 0 || to >= items_.size() || from == to) {
    return;
  }
  const int destination = to > from ? to + 1 : to;
  if (!beginMoveRows({}, from, from, {}, destination)) {
    return;
  }
  items_.move(from, to);
  endMoveRows();
}

void FileListModel::clear() {
  if (items_.isEmpty()) {
    return;
  }
  beginResetModel();
  items_.clear();
  endResetModel();
  emit countChanged();
}

}  // namespace cyan::gui
