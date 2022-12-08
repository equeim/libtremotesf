// SPDX-FileCopyrightText: 2015-2022 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LIBTREMOTESF_IMPL_REQUESTROUTER_H
#define LIBTREMOTESF_IMPL_REQUESTROUTER_H

#include <chrono>
#include <functional>
#include <unordered_map>
#include <unordered_set>

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkRequest>
#include <QString>
#include <QtContainerFwd>

#include <QList>
#include <QSslCertificate>
#include <QSslKey>

#include "rpc.h"

class QAuthenticator;
class QNetworkReply;
class QSslError;
class QThreadPool;

namespace libtremotesf::impl {
    class RequestRouter : public QObject {
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

        const RequestsConfiguration& configuration() const { return mConfiguration; }
        void setConfiguration(RequestsConfiguration configuration);

        struct Response {
            QJsonObject arguments{};
            bool success{};
        };

        void postRequest(
            QLatin1String method, const QVariantMap& arguments, const std::function<void(Response)>& onResponse = {}
        );

        void
        postRequest(QLatin1String method, const QByteArray& data, const std::function<void(Response)>& onResponse = {});

        const QByteArray& sessionId() const { return mSessionId; };

        void cancelPendingRequestsAndClearSessionId();

        static QByteArray makeRequestData(const QString& method, const QVariantMap& arguments);

    private:
        struct Request {
            QLatin1String method{};
            QNetworkRequest request{};
            QByteArray data{};
            std::function<void(Response)> onResponse;

            void setSessionId(const QByteArray& sessionId);
        };

        QNetworkReply* postRequest(Request&& request);

        bool retryRequest(Request&& request, QNetworkReply* previousAttempt);

        void onAuthenticationRequired(QNetworkReply*, QAuthenticator* authenticator) const;
        void onRequestFinished(QNetworkReply* reply, const QList<QSslError>& sslErrors, Request&& request);
        void onRequestSuccess(QNetworkReply* reply, const Request& request);
        void onRequestError(QNetworkReply* reply, const QList<QSslError>& sslErrors, Request&& request);
        static QString makeDetailedErrorMessage(QNetworkReply* reply, const QList<QSslError>& sslErrors);

        QNetworkAccessManager* mNetwork{};
        QThreadPool* mThreadPool{};
        std::unordered_set<QNetworkReply*> mPendingNetworkRequests{};
        std::unordered_set<QObject*> mPendingParseFutures{};
        std::unordered_map<QNetworkReply*, int> mRetryingNetworkRequests{};
        QByteArray mSessionId{};

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
