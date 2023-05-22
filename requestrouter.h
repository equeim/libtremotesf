// SPDX-FileCopyrightText: 2015-2023 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LIBTREMOTESF_IMPL_REQUESTROUTER_H
#define LIBTREMOTESF_IMPL_REQUESTROUTER_H

#include <chrono>
#include <functional>
#include <unordered_map>
#include <unordered_set>

#include <QJsonObject>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkRequest>
#include <QSslCertificate>
#include <QSslKey>
#include <QString>
#include <QtContainerFwd>

#include "rpc.h"

class QAuthenticator;
class QNetworkReply;
class QSslError;
class QThreadPool;

namespace libtremotesf::impl {
    struct RpcRequestMetadata;
    struct NetworkRequestMetadata;

    class RequestRouter final : public QObject {
        Q_OBJECT

    public:
        explicit RequestRouter(QThreadPool* threadPool, QObject* parent = nullptr);
        explicit RequestRouter(QObject* parent = nullptr);

        struct RequestsConfiguration {
            QUrl serverUrl{};
            QNetworkProxy proxy{QNetworkProxy::applicationProxy()};
            QList<QSslCertificate> serverCertificateChain{};
            QSslCertificate clientCertificate{};
            QSslKey clientPrivateKey{};
            std::chrono::milliseconds timeout{};
            int retryAttempts{2};
            bool authentication{};
            QString username{};
            QString password{};
        };

        enum class RequestType { DataUpdate, Independent };

        const RequestsConfiguration& configuration() const { return mConfiguration; }
        void setConfiguration(RequestsConfiguration configuration);

        struct Response {
            QJsonObject arguments{};
            bool success{};
        };

        void postRequest(
            QLatin1String method,
            const QJsonObject& arguments,
            RequestType type,
            std::function<void(Response)>&& onResponse = {}
        );

        void postRequest(
            QLatin1String method,
            const QByteArray& data,
            RequestType type,
            std::function<void(Response)>&& onResponse = {}
        );

        const QByteArray& sessionId() const { return mSessionId; };

        bool hasPendingDataUpdateRequests() const;
        void cancelPendingRequestsAndClearSessionId();

        static QByteArray makeRequestData(const QString& method, const QJsonObject& arguments);

    private:
        void postRequest(QNetworkRequest request, NetworkRequestMetadata&& metadata);

        bool retryRequest(const QNetworkRequest& request, NetworkRequestMetadata&& metadata);

        void onRequestFinished(QNetworkReply* reply, QList<QSslError>&& sslErrors);
        void onRequestSuccess(QNetworkReply* reply, RpcRequestMetadata&& metadata);
        void onRequestError(QNetworkReply* reply, QList<QSslError>&& sslErrors, NetworkRequestMetadata&& metadata);
        static QString makeDetailedErrorMessage(QNetworkReply* reply, QList<QSslError>&& sslErrors);

        QNetworkAccessManager* mNetwork{};
        QThreadPool* mThreadPool{};
        std::unordered_set<QNetworkReply*> mPendingNetworkRequests{};
        std::unordered_set<QObject*> mPendingParseFutures{};
        QByteArray mSessionId{};
        QByteArray mBasicAuthHeaderValue{};

        RequestsConfiguration mConfiguration{};
        QSslConfiguration mSslConfiguration{};
        QList<QSslError> mExpectedSslErrors{};

    signals:
        /**
         * Emitted if request has failed with network or HTTP error
         * @brief requestFailed
         * @param error Error type
         * @param errorMessage Short error message
         * @param detailedErrorMessage Detailed error message
         */
        void requestFailed(RpcError error, const QString& errorMessage, const QString& detailedErrorMessage);
    };
}

#endif // LIBTREMOTESF_IMPL_REQUESTROUTER_H
