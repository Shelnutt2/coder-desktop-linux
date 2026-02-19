#include "filesync/FileSyncSession.h"

QString FileSyncSession::statusString() const {
    switch (status) {
        case FileSyncStatus::Unknown:
            return QStringLiteral("Unknown");
        case FileSyncStatus::ConnectingAlpha:
            return QStringLiteral("Connecting (local)…");
        case FileSyncStatus::ConnectingBeta:
            return QStringLiteral("Connecting (remote)…");
        case FileSyncStatus::Scanning:
            return QStringLiteral("Scanning…");
        case FileSyncStatus::Reconciling:
            return QStringLiteral("Reconciling…");
        case FileSyncStatus::StagingAlpha:
            return QStringLiteral("Staging (local)…");
        case FileSyncStatus::StagingBeta:
            return QStringLiteral("Staging (remote)…");
        case FileSyncStatus::Transitioning:
            return QStringLiteral("Applying changes…");
        case FileSyncStatus::Saving:
            return QStringLiteral("Saving…");
        case FileSyncStatus::Watching:
            return QStringLiteral("Synced");
        case FileSyncStatus::Disconnected:
            return QStringLiteral("Disconnected");
        case FileSyncStatus::HaltedOnRootEmptied:
            return QStringLiteral("Halted: root emptied");
        case FileSyncStatus::HaltedOnRootDeletion:
            return QStringLiteral("Halted: root deleted");
        case FileSyncStatus::HaltedOnRootTypeChange:
            return QStringLiteral("Halted: root type changed");
        case FileSyncStatus::WaitingForRescan:
            return QStringLiteral("Waiting for rescan…");
        case FileSyncStatus::Paused:
            return QStringLiteral("Paused");
    }
    return QStringLiteral("Unknown");
}

QString FileSyncSession::statusCategory() const {
    switch (status) {
        case FileSyncStatus::Watching:
            return QStringLiteral("ok");

        case FileSyncStatus::ConnectingAlpha:
        case FileSyncStatus::ConnectingBeta:
        case FileSyncStatus::Scanning:
        case FileSyncStatus::Reconciling:
        case FileSyncStatus::StagingAlpha:
        case FileSyncStatus::StagingBeta:
        case FileSyncStatus::Transitioning:
        case FileSyncStatus::Saving:
            return QStringLiteral("working");

        case FileSyncStatus::Disconnected:
        case FileSyncStatus::HaltedOnRootEmptied:
        case FileSyncStatus::HaltedOnRootDeletion:
        case FileSyncStatus::HaltedOnRootTypeChange:
        case FileSyncStatus::WaitingForRescan:
            return QStringLiteral("error");

        case FileSyncStatus::Paused:
            return QStringLiteral("paused");

        case FileSyncStatus::Unknown:
            return QStringLiteral("error");
    }
    return QStringLiteral("error");
}
