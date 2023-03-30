// SPDX-FileCopyrightText: 2015-2022 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "demangle.h"

#include <cstdlib>
#include <type_traits>
#include <QScopeGuard>

#if defined(__has_include)
#    if __has_include(<cxxabi.h>)
#        define HAVE_CXXABI_H
#    endif
#elif defined(__GLIBCXX__) || defined(__GLIBCPP__)
#    define HAVE_CXXABI_H
#endif

#ifdef HAVE_CXXABI_H
#    include <cxxabi.h>
#endif

namespace libtremotesf::impl {
#ifdef HAVE_CXXABI_H
    std::string demangleTypeName(const char* typeName) {
        int status{};
        char* demangled = abi::__cxa_demangle(typeName, nullptr, nullptr, &status);
        const auto guard = QScopeGuard([&] {
            if (demangled) free(demangled);
        });
        return demangled ? demangled : typeName;
    }
#else
    namespace {
        void removeSubstring(std::string& str, std::string_view substring) {
            while (true) {
                size_t pos = 0;
                if (pos = str.find(substring, pos); pos != std::string::npos) {
                    str.erase(pos, substring.size());
                } else {
                    break;
                }
            }
        }
    }

    std::string demangleTypeName(const char* typeName) {
        std::string demangled = typeName;
        removeSubstring(demangled, "struct ");
        removeSubstring(demangled, "class ");
        return demangled;
    }
#endif
}
