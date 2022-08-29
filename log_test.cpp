#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <QTest>
#include <QVariant>

#include "log.h"
#include "torrent.h"

SPECIALIZE_FORMATTER_FOR_QDEBUG(QStringList)
SPECIALIZE_FORMATTER_FOR_QDEBUG(QVariant)

using namespace libtremotesf;

class PrintlnTest : public QObject {
    Q_OBJECT
private slots:
    void stdoutStringLiteral() {
        printlnStdout("foo");
        printlnStdout("{}", "foo");
        printlnStdout(FMT_STRING("{}"), "foo");
#if FMT_VERSION >= 80000
        printlnStdout(FMT_COMPILE("{}"), "foo");
        printlnStdout(fmt::runtime("{}"), "foo");
#endif
    }

    void stdoutStdString() {
        const std::string str = "foo";
        printlnStdout(str);
        printlnStdout("{}", str);
        printlnStdout(FMT_STRING("{}"), str);
#if FMT_VERSION >= 80000
        printlnStdout(FMT_COMPILE("{}"), str);
        printlnStdout(fmt::runtime("{}"), str);
#endif
    }

    void stdoutStdStringView() {
        const std::string_view str = "foo";
        printlnStdout(str);
        printlnStdout("{}", str);
        printlnStdout(FMT_STRING("{}"), str);
#if FMT_VERSION >= 80000
        printlnStdout(FMT_COMPILE("{}"), str);
        printlnStdout(fmt::runtime("{}"), str);
#endif
    }

    void stdoutQString() {
        const QString str = "foo";
        printlnStdout(str);
        printlnStdout("{}", str);
        printlnStdout(FMT_STRING("{}"), str);
#if FMT_VERSION >= 80000
        printlnStdout(FMT_COMPILE("{}"), str);
        printlnStdout(fmt::runtime("{}"), str);
#endif
    }

    void stdoutQStringView() {
        const QString _str = "foo";
        const QStringView str = _str;
        printlnStdout(str);
        printlnStdout("{}", str);
        printlnStdout(FMT_STRING("{}"), str);
#if FMT_VERSION >= 80000
        printlnStdout(FMT_COMPILE("{}"), str);
        printlnStdout(fmt::runtime("{}"), str);
#endif
    }

    void stdoutQLatin1String() {
        const QLatin1String str("foo");
        printlnStdout(str);
        printlnStdout("{}", str);
        printlnStdout(FMT_STRING("{}"), str);
#if FMT_VERSION >= 80000
        printlnStdout(FMT_COMPILE("{}"), str);
        printlnStdout(fmt::runtime("{}"), str);
#endif
    }

#if QT_VERSION_MAJOR >= 6
    void stdoutQUtf8StringView() {
        const QUtf8StringView str = "foo";
        printlnStdout(str);
        printlnStdout("{}", str);
        printlnStdout(FMT_STRING("{}"), str);
#if FMT_VERSION >= 80000
        printlnStdout(FMT_COMPILE("{}"), str);
        printlnStdout(fmt::runtime("{}"), str);
#endif
    }

    void stdoutQAnyStringView() {
        const QAnyStringView str = "foo";
        printlnStdout(str);
        printlnStdout("{}", str);
        printlnStdout(FMT_STRING("{}"), str);
#if FMT_VERSION >= 80000
        printlnStdout(FMT_COMPILE("{}"), str);
        printlnStdout(fmt::runtime("{}"), str);
#endif
    }
#endif

    void stdoutQVariant() {
        const QVariant value = "foo";
        printlnStdout(value);
        printlnStdout("{}", value);
        printlnStdout(FMT_STRING("{}"), value);
#if FMT_VERSION >= 80000
        printlnStdout(FMT_COMPILE("{}"), value);
        printlnStdout(fmt::runtime("{}"), value);
#endif
    }

    void stdoutQStringList() {
        const QStringList list{"foo"};
        printlnStdout(list);
        printlnStdout("{}", list);
        printlnStdout(FMT_STRING("{}"), list);
#if FMT_VERSION >= 80000
        printlnStdout(FMT_COMPILE("{}"), list);
        printlnStdout(fmt::runtime("{}"), list);
#endif
    }

    void stdoutTorrent() {
        const Torrent value(0, {}, nullptr);
        printlnStdout(value);
        printlnStdout("{}", value);
        printlnStdout(FMT_STRING("{}"), value);
#if FMT_VERSION >= 80000
        printlnStdout(FMT_COMPILE("{}"), value);
        printlnStdout(fmt::runtime("{}"), value);
#endif
    }

    void stdoutThis() {
        printlnStdout(*this);
        printlnStdout("{}", *this);
        printlnStdout(FMT_STRING("{}"), *this);
#if FMT_VERSION >= 80000
        printlnStdout(FMT_COMPILE("{}"), *this);
        printlnStdout(fmt::runtime("{}"), *this);
#endif
    }

    void stdoutQObject() {
        QObject value{};
        printlnStdout(value);
        printlnStdout("{}", value);
        printlnStdout(FMT_STRING("{}"), value);
#if FMT_VERSION >= 80000
        printlnStdout(FMT_COMPILE("{}"), value);
        printlnStdout(fmt::runtime("{}"), value);
#endif
    }


    void infoStringLiteral() {
        logInfo("foo");
        logInfo("{}", "foo");
        logInfo(FMT_STRING("{}"), "foo");
        logInfo(FMT_COMPILE("{}"), "foo");
#if FMT_VERSION >= 80000
        logInfo(fmt::runtime("{}"), "foo");
#endif
    }

    void infoStdString() {
        const std::string str = "foo";
        logInfo(str);
        logInfo("{}", str);
        logInfo(FMT_STRING("{}"), str);
        logInfo(FMT_COMPILE("{}"), str);
#if FMT_VERSION >= 80000
        logInfo(fmt::runtime("{}"), str);
#endif
    }

    void infoStdStringView() {
        const std::string_view str = "foo";
        logInfo(str);
        logInfo("{}", str);
        logInfo(FMT_STRING("{}"), str);
        logInfo(FMT_COMPILE("{}"), str);
#if FMT_VERSION >= 80000
        logInfo(fmt::runtime("{}"), str);
#endif
    }

    void infoQString() {
        const QString str = "foo";
        logInfo(str);
        logInfo("{}", str);
        logInfo(FMT_STRING("{}"), str);
        logInfo(FMT_COMPILE("{}"), str);
#if FMT_VERSION >= 80000
        logInfo(fmt::runtime("{}"), str);
#endif
    }

    void infoQStringView() {
        const QString _str = "foo";
        const QStringView str = _str;
        logInfo(str);
        logInfo("{}", str);
        logInfo(FMT_STRING("{}"), str);
        logInfo(FMT_COMPILE("{}"), str);
#if FMT_VERSION >= 80000
        logInfo(fmt::runtime("{}"), str);
#endif
    }

    void infoQLatin1String() {
        const QLatin1String str("foo");
        logInfo(str);
        logInfo("{}", str);
        logInfo(FMT_STRING("{}"), str);
        logInfo(FMT_COMPILE("{}"), str);
#if FMT_VERSION >= 80000
        logInfo(fmt::runtime("{}"), str);
#endif
    }

#if QT_VERSION_MAJOR >= 6
    void infoQUtf8StringView() {
        const QUtf8StringView str = "foo";
        logInfo(str);
        logInfo("{}", str);
        logInfo(FMT_STRING("{}"), str);
        logInfo(FMT_COMPILE("{}"), str);
#if FMT_VERSION >= 80000
        logInfo(fmt::runtime("{}"), str);
#endif
    }

    void infoQAnyStringView() {
        const QAnyStringView str = "foo";
        logInfo(str);
        logInfo("{}", str);
        logInfo(FMT_STRING("{}"), str);
        logInfo(FMT_COMPILE("{}"), str);
#if FMT_VERSION >= 80000
        logInfo(fmt::runtime("{}"), str);
#endif
    }
#endif

    void infoQVariant() {
        const QVariant value = "foo";
        logInfo(value);
        logInfo("{}", value);
        logInfo(FMT_STRING("{}"), value);
        logInfo(FMT_COMPILE("{}"), value);
#if FMT_VERSION >= 80000
        logInfo(fmt::runtime("{}"), value);
#endif
    }

    void infoQStringList() {
        const QStringList list{"foo"};
        logInfo(list);
        logInfo("{}", list);
        logInfo(FMT_STRING("{}"), list);
        logInfo(FMT_COMPILE("{}"), list);
#if FMT_VERSION >= 80000
        logInfo(fmt::runtime("{}"), list);
#endif
    }

    void infoTorrent() {
        const Torrent value(0, {}, nullptr);
        logInfo(value);
        logInfo("{}", value);
        logInfo(FMT_STRING("{}"), value);
        logInfo(FMT_COMPILE("{}"), value);
#if FMT_VERSION >= 80000
        logInfo(fmt::runtime("{}"), value);
#endif
    }

    void infoThis() {
        logInfo(*this);
        logInfo("{}", *this);
        logInfo(FMT_STRING("{}"), *this);
        logInfo(FMT_COMPILE("{}"), *this);
#if FMT_VERSION >= 80000
        logInfo(fmt::runtime("{}"), *this);
#endif
    }

    void infoQObject() {
        QObject value{};
        logInfo(value);
        logInfo("{}", value);
        logInfo(FMT_STRING("{}"), value);
        logInfo(FMT_COMPILE("{}"), value);
#if FMT_VERSION >= 80000
        logInfo(fmt::runtime("{}"), value);
#endif
    }
};

QTEST_MAIN(PrintlnTest)

#include "log_test.moc"
