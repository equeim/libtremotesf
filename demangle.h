#ifndef LIBTREMOTESF_DEMANGLE_H
#define LIBTREMOTESF_DEMANGLE_H

#include <string>
#include <typeinfo>

namespace libtremotesf {
    std::string demangleTypeName(const char* typeName);

    template<typename T>
    std::string typeName(T&& t) {
        return demangleTypeName(typeid(t).name());
    }
}

#endif // LIBTREMOTESF_DEMANGLE_H
