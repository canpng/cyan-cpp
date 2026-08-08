#pragma once

#include <QAbstractListModel>
#include <QThread>

#include "job_definition.hpp"

namespace cyan::gui {

class PipelineWorker final : public QObject {
  Q_OBJECT

 public:
  explicit PipelineWorker(JobDefinition job);

 public slots:
  void run();

 signals:
  void logLine(const QString& line);
  void progressChanged(double progress, const QString& stage);
  void finished(bool success, const QString& error);

 private:
  JobDefinition job_;
};

class JobQueueModel final : public QAbstractListModel {
  Q_OBJECT
  Q_PROPERTY(int count READ count NOTIFY countChanged)
  Q_PROPERTY(bool running READ running NOTIFY runningChanged)

 public:
  enum Status { Pending, Running, Completed, Failed };
  Q_ENUM(Status)

  enum Role {
    IdRole = Qt::UserRole + 1,
    InputNameRole,
    OutputNameRole,
    PresetNameRole,
    InjectionCountRole,
    IpaPatchRole,
    StatusRole,
    StatusTextRole,
    ProgressRole,
    StageRole,
    LogsRole,
    ErrorRole,
    OutputPathRole,
    CanEditRole,
    DetailsExpandedRole
  };

  explicit JobQueueModel(QObject* parent = nullptr);
  ~JobQueueModel() override;

  [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
  [[nodiscard]] int count() const;
  [[nodiscard]] bool running() const;
  [[nodiscard]] JobDefinition jobAt(int row) const;

  void addJob(JobDefinition job);
  bool updateJob(const JobDefinition& job);
  Q_INVOKABLE void startQueue();
  Q_INVOKABLE void removeJob(int row);
  Q_INVOKABLE void duplicateJob(int row);
  Q_INVOKABLE void moveJob(int from, int to);
  Q_INVOKABLE void toggleDetails(int row);
  Q_INVOKABLE void copyLog(int row) const;
  Q_INVOKABLE void showInFolder(int row) const;

 signals:
  void countChanged();
  void runningChanged();
  void notification(const QString& message);

 private:
  struct Entry {
    JobDefinition definition;
    Status status{Pending};
    double progress{0.0};
    QString stage{QStringLiteral("Başlatılmayı bekliyor")};
    QStringList logs;
    QString error;
    bool details_expanded{false};
  };

  [[nodiscard]] int findById(const QString& id) const;
  void startNext();
  void finishActive(const QString& id, bool success, const QString& error);
  void notifyRow(int row, const QList<int>& roles = {});

  QList<Entry> jobs_;
  QThread* thread_{nullptr};
  PipelineWorker* worker_{nullptr};
  QString active_id_;
};

}  // namespace cyan::gui
