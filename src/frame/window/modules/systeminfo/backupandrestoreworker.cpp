#include "backupandrestoreworker.h"
#include "backupandrestoremodel.h"

#include <QSharedPointer>
#include <QProcess>
#include <DDBusSender>

#include <QFuture>
#include <QtConcurrent>
#include <DDialog>
#include <QScopedPointer>

DWIDGET_USE_NAMESPACE

using namespace DCC_NAMESPACE;
using namespace DCC_NAMESPACE::systeminfo;

BackupAndRestoreWorker::BackupAndRestoreWorker(BackupAndRestoreModel* model, QObject *parent)
    : QObject(parent)
    , m_model(model)
    , m_grubInter(new GrubInter("com.deepin.daemon.Grub2", "/com/deepin/daemon/Grub2", QDBusConnection::systemBus(), this))
{
    m_grubInter->setSync(false, false);
}

void BackupAndRestoreWorker::manualBackup(const QString &directory)
{
    m_model->setBackupDirectory(directory);

    QFutureWatcher<bool> *watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, [this, watcher] {
        const bool result = watcher->result();
        qDebug() << Q_FUNC_INFO << result;
        m_model->setBackupButtonEnabled(true);
        watcher->deleteLater();
    });

    QFuture<bool> future = QtConcurrent::run(this, &BackupAndRestoreWorker::doManualBackup);
    watcher->setFuture(future);

    m_model->setBackupButtonEnabled(false);
}

void BackupAndRestoreWorker::manualRestore(const QString &directory)
{
    m_model->setRestoreDirectory(directory);

    QFutureWatcher<bool> *watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, [this, watcher] {
        const bool result = watcher->result();
        qDebug() << Q_FUNC_INFO << result;
        m_model->setManualRestoreCheckFailed(!result);
        m_model->setRestoreButtonEnabled(true);
        watcher->deleteLater();
    });

    QFuture<bool> future = QtConcurrent::run(this, &BackupAndRestoreWorker::doManualRestore);
    watcher->setFuture(future);

    m_model->setRestoreButtonEnabled(false);
}

void BackupAndRestoreWorker::systemRestore(bool formatData)
{
    m_model->setFormatData(formatData);

    QFutureWatcher<bool> *watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, [this, watcher] {
        const bool result = watcher->result();
        qDebug() << Q_FUNC_INFO << result;
        m_model->setRestoreButtonEnabled(true);
        watcher->deleteLater();
    });

    QFuture<bool> future = QtConcurrent::run(this, &BackupAndRestoreWorker::doSystemRestore);
    watcher->setFuture(future);

    m_model->setRestoreButtonEnabled(false);
}

bool BackupAndRestoreWorker::doManualBackup()
{
    QSharedPointer<QProcess> process(new QProcess);
    process->start("pkexec", QStringList() << "/bin/restore-tool" << "--actionType" << "manual_backup" << "--path" << m_model->backupDirectory());
    process->waitForFinished(-1);

    if (process->exitCode() != 0) {
        return false;
    }

    QScopedPointer<QDBusPendingCallWatcher> watcher(new QDBusPendingCallWatcher(m_grubInter->SetDefaultEntry("UOS Backup & Restore")));
    watcher->waitForFinished();
    if (watcher->isError()) {
        qWarning() << Q_FUNC_INFO << watcher->error();
        return false;
    }

    QThread::sleep(5);

    DDBusSender()
    .service("com.deepin.dde.shutdownFront")
    .path("/com/deepin/dde/shutdownFront")
    .interface("com.deepin.dde.shutdownFront")
    .method("Restart")
    .call();

    return true;
}

bool BackupAndRestoreWorker::doManualRestore()
{
    const QString& selectPath = m_model->restoreDirectory();

    auto checkValid = [](const QString& filePath) -> bool {
        QScopedPointer<QProcess> process(new QProcess);
        process->setProgram("deepin-clone");
        process->setArguments({"--dim-info", filePath});
        process->start();
        process->waitForFinished();

        return process->exitCode() == 0 && process->exitStatus() == QProcess::NormalExit;
    };

    if (!checkValid(QString("%1/boot.dim").arg(selectPath)) ||
        !checkValid(QString("%1/system.dim").arg(selectPath))) {
        qWarning() << Q_FUNC_INFO << "md5 check failed!";
        return false;
    }

    QSharedPointer<QProcess> process(new QProcess);
    process->start("pkexec", QStringList() << "/bin/restore-tool" << "--actionType" << "manual_restore" << "--path" << selectPath);
    process->waitForFinished(-1);

    if (process->exitCode() != 0) {
        qWarning() << Q_FUNC_INFO << "restore tool run failed!";
        return false;
    }

    QScopedPointer<QDBusPendingCallWatcher> watcher(new QDBusPendingCallWatcher(m_grubInter->SetDefaultEntry("UOS Backup & Restore")));
    watcher->waitForFinished();
    if (watcher->isError()) {
        qWarning() << Q_FUNC_INFO << watcher->error();
        return false;
    }

    QThread::sleep(5);

    DDBusSender()
    .service("com.deepin.dde.shutdownFront")
    .path("/com/deepin/dde/shutdownFront")
    .interface("com.deepin.dde.shutdownFront")
    .method("Restart")
    .call();

    return true;
}

bool BackupAndRestoreWorker::doSystemRestore()
{
    const bool formatData = m_model->formatData();

    QSharedPointer<QProcess> process(new QProcess);
    process->start("pkexec", QStringList() << "/bin/restore-tool" << "--actionType" << "system_restore" << (formatData ? "--formatData" : ""));
    process->waitForFinished(-1);

    if (process->exitCode() != 0) {
        qWarning() << Q_FUNC_INFO << "restore tool run failed!";
        return false;
    }

    QScopedPointer<QDBusPendingCallWatcher> watcher(new QDBusPendingCallWatcher(m_grubInter->SetDefaultEntry("UOS Backup & Restore")));
    watcher->waitForFinished();
    if (watcher->isError()) {
        qWarning() << Q_FUNC_INFO << watcher->error();
        return false;
    }

    QThread::sleep(5);

    DDBusSender()
    .service("com.deepin.dde.shutdownFront")
    .path("/com/deepin/dde/shutdownFront")
    .interface("com.deepin.dde.shutdownFront")
    .method("Restart")
    .call();

    return true;
}
