// SPDX-FileCopyrightText: 2015-2022 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LIBTREMOTESF_DEMANGLE_H
#define LIBTREMOTESF_DEMANGLE_H

#include <string>
#include <typeinfo>

namespace libtremotesf {
    namespace impl {
        std::string demangleTypeName(const char* typeName);
    }

    inline std::string typeName(const std::type_info& typeInfo) { return impl::demangleTypeName(typeInfo.name()); }

    template<typename T>
    std::string typeName(T&& t) {
        return typeName(typeid(t));
    }
}

namespace tremotesf {
    using libtremotesf::typeName;
}

#endif // LIBTREMOTESF_DEMANGLE_H
