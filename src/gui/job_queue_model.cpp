#include "job_queue_model.hpp"

#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QUrl>
#include <QUuid>
#include <utility>

#include "cyan/core/cyan_pipeline.hpp"
#include "cyan/core/input_validator.hpp"

namespace cyan::gui {
namespace {

QString friendly_stage(const QString& line, double* progress) {
  const QString lower = line.toLower();
  if (lower.contains(QStringLiteral("validating"))) {
    *progress = 0.03;
    return QStringLiteral("Doğrulanıyor…");
  }
  if (lower.contains(QStringLiteral("extracting ipa")) ||
      lower.contains(QStringLiteral("copying app"))) {
    *progress = 0.08;
    return QStringLiteral("IPA açılıyor…");
  }
  if (lower.contains(QStringLiteral("parsing"))) {
    *progress = 0.18;
    return QStringLiteral("Cyan paketleri uygulanıyor…");
  }
  if (lower.contains(QStringLiteral("copied")) && lower.contains(QStringLiteral("payload root"))) {
    *progress = 0.24;
    return QStringLiteral("Payload dosyaları kopyalanıyor…");
  }
  if (lower.contains(QStringLiteral("injected"))) {
    *progress = qMax(*progress, 0.48);
    return QStringLiteral("Frameworks hazırlanıyor…");
  }
  if (lower.contains(QStringLiteral("changed")) || lower.contains(QStringLiteral("merged")) ||
      lower.contains(QStringLiteral("documents"))) {
    *progress = qMax(*progress, 0.60);
    return QStringLiteral("Uygulama bilgileri güncelleniyor…");
  }
  if (lower.contains(QStringLiteral("ipapatch validating"))) {
    *progress = 0.64;
    return QStringLiteral("iPAPatch — Doğrulanıyor…");
  }
  if (lower.contains(QStringLiteral("ipapatch discovering"))) {
    *progress = 0.68;
    return QStringLiteral("iPAPatch — Bileşenler aranıyor…");
  }
  if (lower.contains(QStringLiteral("capturing signatures"))) {
    *progress = 0.72;
    return QStringLiteral("iPAPatch — İmza bilgileri hazırlanıyor…");
  }
  if (lower.contains(QStringLiteral("ipapatch injecting"))) {
    *progress = 0.77;
    return QStringLiteral("iPAPatch — Payload enjekte ediliyor…");
  }
  if (lower.contains(QStringLiteral("installing payload"))) {
    *progress = 0.81;
    return QStringLiteral("iPAPatch — Payload kuruluyor…");
  }
  if (lower.contains(QStringLiteral("ipapatch signing")) ||
      lower.contains(QStringLiteral("signed"))) {
    *progress = qMax(*progress, 0.87);
    return QStringLiteral("İmzalanıyor…");
  }
  if (lower.contains(QStringLiteral("generating ipa")) ||
      lower.contains(QStringLiteral("ipapatch packaging"))) {
    *progress = 0.94;
    return QStringLiteral("IPA hazırlanıyor…");
  }
  if (lower.contains(QStringLiteral("generated")) ||
      lower.contains(QStringLiteral("ipapatch completed"))) {
    *progress = 1.0;
    return QStringLiteral("Tamamlandı");
  }
  return {};
}

QString format_error(const cyan::Error& error) {
  QString result = QString::fromStdString(error.message);
  if (!error.path.empty()) {
    result += QStringLiteral(": ") + QString::fromStdWString(error.path.native());
  }
  return result;
}

}  // namespace

PipelineWorker::PipelineWorker(JobDefinition job) : job_(std::move(job)) {}

void PipelineWorker::run() {
  auto options = job_.to_cyan_options();
  auto valid = cyan::validate_and_normalize(options);
  if (!valid) {
    emit finished(false, format_error(valid.error()));
    return;
  }
  std::error_code error;
  if (std::filesystem::exists(options.output, error) && !job_.overwrite) {
    emit finished(
        false, QStringLiteral("Çıktı zaten var ve üzerine yazma kapalı: %1").arg(job_.output_path));
    return;
  }
  if (error) {
    emit finished(false, QStringLiteral("Çıktı yolu denetlenemedi: %1").arg(job_.output_path));
    return;
  }

  double progress = 0.02;
  emit progressChanged(progress, QStringLiteral("Doğrulanıyor…"));
  cyan::CyanPipeline pipeline;
  auto result = pipeline.run(std::move(options), [&](std::wstring_view message) {
    const QString line = QString::fromStdWString(std::wstring(message));
    emit logLine(line);
    const QString stage = friendly_stage(line, &progress);
    if (!stage.isEmpty()) {
      emit progressChanged(progress, stage);
    }
  });
  if (!result) {
    emit finished(false, format_error(result.error()));
    return;
  }
  emit progressChanged(1.0, QStringLiteral("Tamamlandı"));
  emit finished(true, {});
}

JobQueueModel::JobQueueModel(QObject* parent) : QAbstractListModel(parent) {}

JobQueueModel::~JobQueueModel() {
  if (thread_ != nullptr && thread_->isRunning()) {
    thread_->quit();
    thread_->wait();
  }
}

int JobQueueModel::rowCount(const QModelIndex& parent) const {
  return parent.isValid() ? 0 : jobs_.size();
}

QVariant JobQueueModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= jobs_.size()) {
    return {};
  }
  const auto& entry = jobs_.at(index.row());
  const QFileInfo input(entry.definition.input_path);
  const QFileInfo output(entry.definition.output_path);
  switch (role) {
    case IdRole:
      return entry.definition.id;
    case InputNameRole:
      return input.fileName();
    case OutputNameRole:
      return output.fileName();
    case PresetNameRole:
      return entry.definition.preset_name;
    case InjectionCountRole:
      return entry.definition.injections.size() + entry.definition.cyan_packages.size();
    case IpaPatchRole:
      return entry.definition.ipapatch_enabled;
    case StatusRole:
      return entry.status;
    case StatusTextRole:
      switch (entry.status) {
        case Pending:
          return QStringLiteral("Bekliyor");
        case Running:
          return QStringLiteral("Çalışıyor");
        case Completed:
          return QStringLiteral("Tamamlandı");
        case Failed:
          return QStringLiteral("Başarısız");
      }
      return {};
    case ProgressRole:
      return entry.progress;
    case StageRole:
      return entry.stage;
    case LogsRole:
      return entry.logs.join(QLatin1Char('\n'));
    case ErrorRole:
      return entry.error;
    case OutputPathRole:
      return entry.definition.output_path;
    case CanEditRole:
      return entry.status == Pending;
    case DetailsExpandedRole:
      return entry.details_expanded;
    default:
      return {};
  }
}

QHash<int, QByteArray> JobQueueModel::roleNames() const {
  return {{IdRole, "jobId"},
          {InputNameRole, "inputName"},
          {OutputNameRole, "outputName"},
          {PresetNameRole, "presetName"},
          {InjectionCountRole, "injectionCount"},
          {IpaPatchRole, "ipapatch"},
          {StatusRole, "status"},
          {StatusTextRole, "statusText"},
          {ProgressRole, "progress"},
          {StageRole, "stage"},
          {LogsRole, "logs"},
          {ErrorRole, "errorText"},
          {OutputPathRole, "outputPath"},
          {CanEditRole, "canEdit"},
          {DetailsExpandedRole, "detailsExpanded"}};
}

int JobQueueModel::count() const { return jobs_.size(); }
bool JobQueueModel::running() const { return !active_id_.isEmpty(); }

JobDefinition JobQueueModel::jobAt(int row) const {
  return row >= 0 && row < jobs_.size() ? jobs_.at(row).definition : JobDefinition{};
}

void JobQueueModel::addJob(JobDefinition job) {
  const int row = jobs_.size();
  beginInsertRows({}, row, row);
  jobs_.append({std::move(job)});
  endInsertRows();
  emit countChanged();
}

bool JobQueueModel::updateJob(const JobDefinition& job) {
  const int row = findById(job.id);
  if (row < 0 || jobs_[row].status != Pending) {
    return false;
  }
  jobs_[row].definition = job;
  notifyRow(row);
  return true;
}

int JobQueueModel::findById(const QString& id) const {
  for (int row = 0; row < jobs_.size(); ++row) {
    if (jobs_.at(row).definition.id == id) {
      return row;
    }
  }
  return -1;
}

void JobQueueModel::startQueue() {
  if (!running()) {
    startNext();
  }
}

void JobQueueModel::startNext() {
  int row = -1;
  for (int index = 0; index < jobs_.size(); ++index) {
    if (jobs_.at(index).status == Pending) {
      row = index;
      break;
    }
  }
  if (row < 0) {
    active_id_.clear();
    emit runningChanged();
    return;
  }

  auto& entry = jobs_[row];
  entry.status = Running;
  entry.progress = 0.01;
  entry.stage = QStringLiteral("Başlatılıyor…");
  active_id_ = entry.definition.id;
  notifyRow(row);
  emit runningChanged();

  thread_ = new QThread(this);
  worker_ = new PipelineWorker(entry.definition);
  worker_->moveToThread(thread_);
  connect(thread_, &QThread::started, worker_, &PipelineWorker::run);
  connect(worker_, &PipelineWorker::logLine, this, [this, id = active_id_](const QString& line) {
    const int active = findById(id);
    if (active < 0) {
      return;
    }
    jobs_[active].logs.append(line);
    notifyRow(active, {LogsRole});
  });
  connect(worker_, &PipelineWorker::progressChanged, this,
          [this, id = active_id_](double progress, const QString& stage) {
            const int active = findById(id);
            if (active < 0) {
              return;
            }
            jobs_[active].progress = progress;
            jobs_[active].stage = stage;
            notifyRow(active, {ProgressRole, StageRole});
          });
  connect(worker_, &PipelineWorker::finished, this,
          [this, id = active_id_](bool success, const QString& error) {
            finishActive(id, success, error);
          });
  connect(worker_, &PipelineWorker::finished, worker_, &QObject::deleteLater);
  connect(worker_, &PipelineWorker::finished, thread_, &QThread::quit);
  connect(thread_, &QThread::finished, thread_, &QObject::deleteLater);
  connect(thread_, &QThread::finished, this, [this]() {
    thread_ = nullptr;
    worker_ = nullptr;
    startNext();
  });
  thread_->start();
}

void JobQueueModel::finishActive(const QString& id, bool success, const QString& error) {
  const int row = findById(id);
  if (row < 0) {
    return;
  }
  auto& entry = jobs_[row];
  entry.status = success ? Completed : Failed;
  entry.progress = success ? 1.0 : entry.progress;
  entry.stage = success ? QStringLiteral("Tamamlandı") : QStringLiteral("İşlem başarısız");
  entry.error = error;
  if (!success && !error.isEmpty()) {
    entry.logs.append(QStringLiteral("[!] ") + error);
  }
  notifyRow(row);
}

void JobQueueModel::removeJob(int row) {
  if (row < 0 || row >= jobs_.size() || jobs_[row].status != Pending) {
    return;
  }
  beginRemoveRows({}, row, row);
  jobs_.removeAt(row);
  endRemoveRows();
  emit countChanged();
}

void JobQueueModel::duplicateJob(int row) {
  if (row < 0 || row >= jobs_.size() || jobs_[row].status == Running) {
    return;
  }
  JobDefinition copy = jobs_.at(row).definition;
  copy.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  const QFileInfo output(copy.output_path);
  copy.output_path =
      QDir(output.absolutePath())
          .filePath(output.completeBaseName() + QStringLiteral("_copy.") + output.suffix());
  addJob(std::move(copy));
}

void JobQueueModel::moveJob(int from, int to) {
  if (from < 0 || from >= jobs_.size() || to < 0 || to >= jobs_.size() || from == to ||
      jobs_[from].status != Pending || jobs_[to].status != Pending) {
    return;
  }
  const int destination = to > from ? to + 1 : to;
  if (!beginMoveRows({}, from, from, {}, destination)) {
    return;
  }
  jobs_.move(from, to);
  endMoveRows();
}

void JobQueueModel::toggleDetails(int row) {
  if (row < 0 || row >= jobs_.size()) {
    return;
  }
  jobs_[row].details_expanded = !jobs_[row].details_expanded;
  notifyRow(row, {DetailsExpandedRole});
}

void JobQueueModel::copyLog(int row) const {
  if (row < 0 || row >= jobs_.size()) {
    return;
  }
  QGuiApplication::clipboard()->setText(jobs_.at(row).logs.join(QLatin1Char('\n')));
}

void JobQueueModel::showInFolder(int row) const {
  if (row < 0 || row >= jobs_.size()) {
    return;
  }
  QDesktopServices::openUrl(
      QUrl::fromLocalFile(QFileInfo(jobs_.at(row).definition.output_path).absolutePath()));
}

void JobQueueModel::notifyRow(int row, const QList<int>& roles) {
  if (row >= 0 && row < jobs_.size()) {
    emit dataChanged(index(row), index(row), roles);
  }
}

}  // namespace cyan::gui
