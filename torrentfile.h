// SPDX-FileCopyrightText: 2015-2022 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LIBTREMOTESF_TORRENTFILE_H
#define LIBTREMOTESF_TORRENTFILE_H

#include <vector>

class QString;
class QJsonObject;

namespace libtremotesf {
    struct TorrentFile {
        enum Priority { LowPriority = -1, NormalPriority, HighPriority };

        explicit TorrentFile(int id, const QJsonObject& fileMap, const QJsonObject& fileStatsMap);
        bool update(const QJsonObject& fileStatsMap);

        int id;

        std::vector<QString> path;
        long long size;
        long long completedSize = 0;
        Priority priority = NormalPriority;
        bool wanted = false;
    };
}

#endif // LIBTREMOTESF_TORRENTFILE_H
