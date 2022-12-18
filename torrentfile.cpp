// SPDX-FileCopyrightText: 2015-2022 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "torrentfile.h"

#include <QJsonObject>
#include <QStringList>

#include "literals.h"
#include "stdutils.h"

namespace libtremotesf {
    TorrentFile::TorrentFile(int id, const QJsonObject& fileMap, const QJsonObject& fileStatsMap)
        : id(id), size(static_cast<long long>(fileMap.value("length"_l1).toDouble())) {
        auto p = fileMap.value("name"_l1).toString().split(QLatin1Char('/'), Qt::SkipEmptyParts);
        path.reserve(static_cast<size_t>(p.size()));
        for (QString& part : p) {
            path.push_back(std::move(part));
        }
        update(fileStatsMap);
    }

    bool TorrentFile::update(const QJsonObject& fileStatsMap) {
        bool changed = false;

        setChanged(completedSize, static_cast<long long>(fileStatsMap.value("bytesCompleted"_l1).toDouble()), changed);
        setChanged(
            priority,
            [&] {
                switch (auto p = static_cast<Priority>(fileStatsMap.value("priority"_l1).toInt())) {
                case Priority::Low:
                case Priority::Normal:
                case Priority::High:
                    return p;
                default:
                    return Priority::Normal;
                }
            }(),
            changed
        );
        setChanged(wanted, fileStatsMap.value("wanted"_l1).toBool(), changed);

        return changed;
    }
}
