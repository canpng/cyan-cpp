#pragma once

#include <QAbstractListModel>

#include "job_definition.hpp"

namespace cyan::gui {

class FileListModel final : public QAbstractListModel {
  Q_OBJECT
  Q_PROPERTY(int count READ count NOTIFY countChanged)

 public:
  enum Role { PathRole = Qt::UserRole + 1, NameRole, TypeRole, TargetRole, SizeRole, MissingRole };

  explicit FileListModel(QObject* parent = nullptr);

  [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
  [[nodiscard]] int count() const;
  [[nodiscard]] const QList<FileItem>& items() const;

  bool addPath(const QString& path, const QString& forced_target = {});
  void setItems(QList<FileItem> items);
  Q_INVOKABLE void remove(int row);
  Q_INVOKABLE void move(int from, int to);
  Q_INVOKABLE void clear();

 signals:
  void countChanged();
  void duplicateRejected(const QString& name);

 private:
  QList<FileItem> items_;
};

}  // namespace cyan::gui
