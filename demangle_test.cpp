// SPDX-FileCopyrightText: 2015-2022 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QTest>

#include "demangle.h"

using namespace libtremotesf;

struct Foo {};
class Bar {};

namespace foobar {
    struct Foo {};
    class Bar {};
}

template<typename T>
struct What {};

class DemangleTest : public QObject {
    Q_OBJECT
private slots:
    void checkInt() {
        int foo{};
        QCOMPARE(typeName(foo), "int");
    }

    void checkStruct() {
        Foo foo{};
        QCOMPARE(typeName(foo), "Foo");
    }

    void checkClass() {
        Bar bar{};
        QCOMPARE(typeName(bar), "Bar");
    }

    void checkNamespacedStruct() {
        foobar::Foo foo{};
        QCOMPARE(typeName(foo), "foobar::Foo");
    }

    void checkNamespacedClass() {
        foobar::Bar bar{};
        QCOMPARE(typeName(bar), "foobar::Bar");
    }

    void checkTemplatedStruct() {
        What<int> what{};
        QCOMPARE(typeName(what), "What<int>");
    }

    void checkTemplatedStruct2() {
        What<Foo> what{};
        QCOMPARE(typeName(what), "What<Foo>");
    }

    void checkTemplatedStruct3() {
        What<foobar::Foo> what{};
        QCOMPARE(typeName(what), "What<foobar::Foo>");
    }
};

QTEST_MAIN(DemangleTest)

#include "demangle_test.moc"
