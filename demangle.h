// SPDX-FileCopyrightText: 2015-2023 Alexey Rochev
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

    template<typename T>
    std::string typeName(T&& t) {
        return impl::demangleTypeName(typeid(t).name());
    }
}

namespace tremotesf {
    using libtremotesf::typeName;
}

#endif // LIBTREMOTESF_DEMANGLE_H
