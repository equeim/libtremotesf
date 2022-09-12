#include "log.h"

namespace libtremotesf::impl {
    void QMessageLoggerDelegate::logString(const QString& string) const {
        // We use internal qt_message_output() function here because there are only two methods
        // to output string to QMessageLogger and they have overheads that are unneccessary
        // when we are doing formatting on our own:
        // 1. QDebug marshalls everything through QTextStream
        // 2. QMessageLogger::<>(const char*, ...) overloads perform QString::vasprintf() formatting
        qt_message_output(type, context, string);
    }

    void QMessageLoggerDelegate::logString(std::string_view string) const {
        logString(QString::fromUtf8(string.data(), static_cast<QString::size_type>(string.size())));
    }

    template void QMessageLoggerDelegate::logExceptionRecursively<std::exception, true>(const std::exception&) const;
    template void QMessageLoggerDelegate::logExceptionRecursively<std::exception, false>(const std::exception&) const;
#ifdef Q_OS_WIN
    template void QMessageLoggerDelegate::logExceptionRecursively<winrt::hresult_error, true>(const winrt::hresult_error&) const;
    template void QMessageLoggerDelegate::logExceptionRecursively<winrt::hresult_error, false>(const winrt::hresult_error&) const;
#endif
}
