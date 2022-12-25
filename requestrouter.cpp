// SPDX-FileCopyrightText: 2015-2022 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "requestrouter.h"

#include <optional>
#include <utility>

#include <QAuthenticator>
#include <QFutureWatcher>
#include <QJsonDocument>
#include <QObject>
#include <QNetworkReply>
#include <QtConcurrentRun>

#include <fmt/chrono.h>

#include "log.h"

SPECIALIZE_FORMATTER_FOR_Q_ENUM(QNetworkReply::NetworkError)
SPECIALIZE_FORMATTER_FOR_Q_ENUM(QSslError::SslError)
SPECIALIZE_FORMATTER_FOR_QDEBUG(QJsonObject)
SPECIALIZE_FORMATTER_FOR_QDEBUG(QNetworkProxy)
SPECIALIZE_FORMATTER_FOR_QDEBUG(QSslError)

namespace libtremotesf::impl {
    namespace {
        const auto sessionIdHeader = QByteArrayLiteral("X-Transmission-Session-Id");

        QJsonObject getReplyArguments(const QJsonObject& parseResult) {
            return parseResult.value("arguments"_l1).toObject();
        }

        bool isResultSuccessful(const QJsonObject& parseResult) {
            return (parseResult.value("result"_l1).toString() == "success"_l1);
        }

        using ParseFutureWatcher = QFutureWatcher<std::optional<QJsonObject>>;
    }

    RequestRouter::RequestRouter(QThreadPool* threadPool, QObject* parent)
        : QObject(parent),
          mNetwork(new QNetworkAccessManager(this)),
          mThreadPool(threadPool ? threadPool : QThreadPool::globalInstance()) {
        mNetwork->setAutoDeleteReplies(true);
        QObject::connect(
            mNetwork,
            &QNetworkAccessManager::authenticationRequired,
            this,
            &RequestRouter::onAuthenticationRequired
        );
    }

    RequestRouter::RequestRouter(QObject* parent) : RequestRouter(nullptr, parent) {}

    void RequestRouter::setConfiguration(RequestsConfiguration configuration) {
        mConfiguration = std::move(configuration);

        mNetwork->setProxy(mConfiguration.proxy);
        mNetwork->clearAccessCache();

        const bool https = mConfiguration.serverUrl.scheme() == "https"_l1;

        mSslConfiguration = QSslConfiguration::defaultConfiguration();
        if (https) {
            if (!mConfiguration.clientCertificate.isNull()) {
                mSslConfiguration.setLocalCertificate(mConfiguration.clientCertificate);
            }
            if (!mConfiguration.clientPrivateKey.isNull()) {
                mSslConfiguration.setPrivateKey(mConfiguration.clientPrivateKey);
            }
            mExpectedSslErrors.clear();
            mExpectedSslErrors.reserve(mConfiguration.serverCertificateChain.size() * 3);
            for (const auto& certificate : mConfiguration.serverCertificateChain) {
                mExpectedSslErrors.push_back(QSslError(QSslError::HostNameMismatch, certificate));
                mExpectedSslErrors.push_back(QSslError(QSslError::SelfSignedCertificate, certificate));
                mExpectedSslErrors.push_back(QSslError(QSslError::SelfSignedCertificateInChain, certificate));
            }
        }

        if (!mConfiguration.serverUrl.isEmpty()) {
            logDebug("Connection configuration:");
            logDebug(" - Server url: {}", mConfiguration.serverUrl.toString());
            if (mConfiguration.proxy.type() != QNetworkProxy::NoProxy) {
                logDebug(" - Proxy: {}", mConfiguration.proxy);
            }
            logDebug(" - Timeout: {}", mConfiguration.timeout);
            logDebug(" - HTTP Basic access authentication: {}", mConfiguration.authentication);
            if (https) {
                logDebug(
                    " - Manually validating server's certificate chain: {}",
                    !mConfiguration.serverCertificateChain.isEmpty()
                );
                logDebug(
                    " - Client certificate authentication: {}",
                    !mConfiguration.clientCertificate.isNull() && !mConfiguration.clientPrivateKey.isNull()
                );
            }
        }
    }

    void RequestRouter::postRequest(
        QLatin1String method, const QJsonObject& arguments, const std::function<void(Response)>& onResponse
    ) {
        postRequest(method, makeRequestData(method, arguments), onResponse);
    }

    void RequestRouter::postRequest(
        QLatin1String method, const QByteArray& data, const std::function<void(Response)>& onResponse
    ) {
        Request request{method, QNetworkRequest(mConfiguration.serverUrl), data, onResponse};
        request.setSessionId(mSessionId);
        request.request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json"_l1);
        request.request.setSslConfiguration(mSslConfiguration);
        request.request.setTransferTimeout(static_cast<int>(mConfiguration.timeout.count()));
        postRequest(std::move(request));
    }

    void RequestRouter::cancelPendingRequestsAndClearSessionId() {
        for (QNetworkReply* reply : std::unordered_set(std::move(mPendingNetworkRequests))) {
            reply->abort();
        }
        for (QObject* futureWatcher : std::unordered_set(std::move(mPendingParseFutures))) {
            static_cast<ParseFutureWatcher*>(futureWatcher)->cancel();
            futureWatcher->deleteLater();
        }
        mSessionId.clear();
    }

    QByteArray RequestRouter::makeRequestData(const QString& method, const QJsonObject& arguments) {
        return QJsonDocument(QJsonObject{
                                 {QStringLiteral("method"), method},
                                 {QStringLiteral("arguments"), arguments},
                             })
            .toJson(QJsonDocument::Compact);
    }

    QNetworkReply* RequestRouter::postRequest(Request&& request) {
        QNetworkReply* reply = mNetwork->post(request.request, request.data);
        mPendingNetworkRequests.insert(reply);

        reply->ignoreSslErrors(mExpectedSslErrors);
        auto sslErrors = std::make_shared<QList<QSslError>>();

        QObject::connect(reply, &QNetworkReply::sslErrors, this, [=](const QList<QSslError>& errors) {
            for (const QSslError& error : errors) {
                if (!mExpectedSslErrors.contains(error)) {
                    logWarning("SSL error {} on {}", error, error.certificate().toText());
                    sslErrors->push_back(error);
                }
            }
        });

        QObject::connect(reply, &QNetworkReply::finished, this, [=, request = std::move(request)]() mutable {
            onRequestFinished(reply, *sslErrors, std::move(request));
        });

        return reply;
    }

    bool RequestRouter::retryRequest(Request&& request, QNetworkReply* previousAttempt) {
        int retryAttempts{};
        if (const auto found = mRetryingNetworkRequests.find(previousAttempt);
            found != mRetryingNetworkRequests.end()) {
            retryAttempts = found->second;
            mRetryingNetworkRequests.erase(found);
        } else {
            retryAttempts = 0;
        }
        ++retryAttempts;
        if (retryAttempts > mConfiguration.retryAttempts) {
            return false;
        }

        request.setSessionId(mSessionId);

        logWarning("Retrying '{}' request, retry attempts = {}", request.method, retryAttempts);
        QNetworkReply* reply = postRequest(std::move(request));
        mRetryingNetworkRequests.emplace(reply, retryAttempts);

        return true;
    }

    void RequestRouter::onAuthenticationRequired(QNetworkReply*, QAuthenticator* authenticator) const {
        logDebug("Authentication requested");
        if (mConfiguration.authentication) {
            logDebug("Authenticating");
            authenticator->setUser(mConfiguration.username);
            authenticator->setPassword(mConfiguration.password);
        } else {
            logWarning("Authentication requested, but authentication is disabled");
        }
    }

    void RequestRouter::onRequestFinished(QNetworkReply* reply, const QList<QSslError>& sslErrors, Request&& request) {
        if (mPendingNetworkRequests.erase(reply) == 0) {
            // Request was cancelled
            return;
        }
        if (reply->error() == QNetworkReply::NoError) {
            onRequestSuccess(reply, request);
        } else {
            onRequestError(reply, sslErrors, std::move(request));
        }
    }

    void RequestRouter::onRequestSuccess(QNetworkReply* reply, const Request& request) {
        logDebug(
            "HTTP request for method '{}' succeeded, HTTP status code: {} {}",
            request.method,
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),
            reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString()
        );

        mRetryingNetworkRequests.erase(reply);

        const auto future =
            QtConcurrent::run(mThreadPool, [replyData = reply->readAll()]() -> std::optional<QJsonObject> {
                QJsonParseError error{};
                QJsonObject json = QJsonDocument::fromJson(replyData, &error).object();
                if (error.error != QJsonParseError::NoError) {
                    logWarning(
                        "Failed to parse JSON reply from server:\n{}\nError '{}' at offset {}",
                        replyData,
                        error.errorString(),
                        error.offset
                    );
                    return {};
                }
                return json;
            });
        auto watcher = new ParseFutureWatcher(this);
        QObject::connect(watcher, &ParseFutureWatcher::finished, this, [=] {
            if (!mPendingParseFutures.erase(watcher)) {
                // Future was cancelled
                return;
            }
            auto json = watcher->result();
            if (json.has_value()) {
                const bool success = isResultSuccessful(*json);
                if (!success) {
                    logWarning("method '{}' failed, response: {}", request.method, *json);
                }
                if (request.onResponse) {
                    request.onResponse({getReplyArguments(*json), success});
                }
            } else {
                emit requestFailed(RpcError::ParseError, {}, {});
            }
            watcher->deleteLater();
        });
        mPendingParseFutures.insert(watcher);
        watcher->setFuture(future);
    }

    void RequestRouter::onRequestError(QNetworkReply* reply, const QList<QSslError>& sslErrors, Request&& request) {
        if (reply->error() == QNetworkReply::ContentConflictError && reply->hasRawHeader(sessionIdHeader)) {
            QByteArray newSessionId = reply->rawHeader(sessionIdHeader);
            // Check against session id of request instead of current session id,
            // to handle case when current session id have already been overwritten by another failed request
            if (newSessionId != request.request.rawHeader(sessionIdHeader)) {
                if (!mSessionId.isEmpty()) {
                    logInfo("Session id changed");
                }
                logDebug("Session id is {}, retrying '{}' request", newSessionId, request.method);
                mSessionId = std::move(newSessionId);
                // Retry without incrementing retryAttempts
                request.setSessionId(mSessionId);
                postRequest(std::move(request));
                return;
            }
        }

        const QString detailedErrorMessage = makeDetailedErrorMessage(reply, sslErrors);
        logWarning("HTTP request for method '{}' failed:\n{}", request.method, detailedErrorMessage);
        switch (reply->error()) {
        case QNetworkReply::AuthenticationRequiredError:
            logWarning("Authentication error");
            emit requestFailed(RpcError::AuthenticationError, reply->errorString(), detailedErrorMessage);
            break;
        case QNetworkReply::OperationCanceledError:
        case QNetworkReply::TimeoutError:
            logWarning("Timed out");
            if (!retryRequest(std::move(request), reply)) {
                emit requestFailed(RpcError::TimedOut, reply->errorString(), detailedErrorMessage);
            }
            break;
        default: {
            if (!retryRequest(std::move(request), reply)) {
                emit requestFailed(RpcError::ConnectionError, reply->errorString(), detailedErrorMessage);
            }
        }
        }
    }

    QString RequestRouter::makeDetailedErrorMessage(QNetworkReply* reply, const QList<QSslError>& sslErrors) {
        auto detailedErrorMessage = QString::fromStdString(fmt::format("{}: {}", reply->error(), reply->errorString()));
        if (reply->url() == reply->request().url()) {
            detailedErrorMessage += QString::fromStdString(fmt::format("\nURL: {}", reply->url().toString()));
        } else {
            detailedErrorMessage += QString::fromStdString(fmt::format(
                "\nOriginal URL: {}\nRedirected URL: {}",
                reply->request().url().toString(),
                reply->url().toString()
            ));
        }
        if (auto httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
            httpStatusCode.isValid()) {
            detailedErrorMessage += QString::fromStdString(fmt::format(
                "\nHTTP status code: {} {}\nConnection was encrypted: {}",
                httpStatusCode.toInt(),
                reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString(),
                reply->attribute(QNetworkRequest::ConnectionEncryptedAttribute).toBool()
            ));
            if (!reply->rawHeaderPairs().isEmpty()) {
                detailedErrorMessage += "\nReply headers:"_l1;
                for (const QNetworkReply::RawHeaderPair& pair : reply->rawHeaderPairs()) {
                    detailedErrorMessage += QString::fromStdString(fmt::format("\n  {}: {}", pair.first, pair.second));
                }
            }
        } else {
            detailedErrorMessage += "\nDid not establish HTTP connection"_l1;
        }
        if (!sslErrors.isEmpty()) {
            detailedErrorMessage += QString::fromStdString(fmt::format("\n\n{} TLS errors:", sslErrors.size()));
            int i = 1;
            for (const QSslError& sslError : sslErrors) {
                detailedErrorMessage += QString::fromStdString(fmt::format(
                    "\n\n {}. {}: {} on certificate:\n - {}",
                    i,
                    sslError.error(),
                    sslError.errorString(),
                    sslError.certificate().toText()
                ));
                ++i;
            }
        }
        return detailedErrorMessage;
    }

    void RequestRouter::Request::setSessionId(const QByteArray& sessionId) {
        if (!sessionId.isEmpty()) {
            request.setRawHeader(sessionIdHeader, sessionId);
        }
    }
}
