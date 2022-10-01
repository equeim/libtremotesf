// SPDX-FileCopyrightText: 2015-2022 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "torrentfile.h"

#include <QJsonObject>
#include <QStringList>

#include "stdutils.h"

namespace libtremotesf
{
    TorrentFile::TorrentFile(int id, const QJsonObject& fileMap, const QJsonObject& fileStatsMap)
        : id(id), size(static_cast<long long>(fileMap.value(QLatin1String("length")).toDouble()))
    {
        auto p = fileMap
                .value(QLatin1String("name"))
                .toString()
                .split(QLatin1Char('/'), Qt::SkipEmptyParts);
        path.reserve(static_cast<size_t>(p.size()));
        for (QString& part : p) {
            path.push_back(std::move(part));
        }
        update(fileStatsMap);
    }

    bool TorrentFile::update(const QJsonObject& fileStatsMap)
    {
        bool changed = false;

        setChanged(completedSize, static_cast<long long>(fileStatsMap.value(QLatin1String("bytesCompleted")).toDouble()), changed);
        setChanged(priority, [&] {
            switch (int p = fileStatsMap.value(QLatin1String("priority")).toInt()) {
            case TorrentFile::LowPriority:
            case TorrentFile::NormalPriority:
            case TorrentFile::HighPriority:
                return static_cast<TorrentFile::Priority>(p);
            default:
                return TorrentFile::NormalPriority;
            }
        }(), changed);
        setChanged(wanted, fileStatsMap.value(QLatin1String("wanted")).toBool(), changed);

        return changed;
    }
}
