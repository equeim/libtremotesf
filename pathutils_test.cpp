// SPDX-FileCopyrightText: 2015-2022 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <array>
#include <QTest>

#include "pathutils.h"

using namespace libtremotesf;

class PathUtilsTest : public QObject {
    Q_OBJECT
private:
    struct NormalizeTestCase {
        QString inputPath{};
        QString expectedNormalizedPath{};
    };

    struct NativeSeparatorsTestCase {
        QString inputPath{};
        QString expectedNativeSeparatorsPath{};
    };
private slots:
    void checkNormalize() {
        const auto testCases = std::array{
            NormalizeTestCase{"", ""},
            NormalizeTestCase{"/", "/"},
            NormalizeTestCase{"//", "/"},
            NormalizeTestCase{"///", "/"},
            NormalizeTestCase{" / ", "/"},
            NormalizeTestCase{"///home//foo", "/home/foo"},
            NormalizeTestCase{"C:/home//foo", "C:/home/foo"},
            NormalizeTestCase{"C:/home//foo/", "C:/home/foo"},
            NormalizeTestCase{R"(C:\home\foo)", "C:/home/foo"},
            NormalizeTestCase{R"(C:\home\foo\\)", "C:/home/foo"},
            NormalizeTestCase{R"(z:\home\foo)", "Z:/home/foo"},
            NormalizeTestCase{R"(D:\)", "D:/"},
            NormalizeTestCase{R"( D:\ )", "D:/"},
            NormalizeTestCase{R"(D:\\)", "D:/"},
            NormalizeTestCase{"D:/", "D:/"},
            NormalizeTestCase{"D://", "D:/"},

            // Backslashes in Unix paths are untouched
            NormalizeTestCase{R"(///home//fo\o)", R"(/home/fo\o)"},

            // Internal whitespace is untouched
            NormalizeTestCase{"///home//fo  o", "/home/fo  o"},
            NormalizeTestCase{R"(C:\home\fo o)", "C:/home/fo o"},

            // These are not absolute Windows file paths and are left untouched
            NormalizeTestCase{"d:", "d:"},
            NormalizeTestCase{"d:foo", "d:foo"},
            NormalizeTestCase{R"(C::\wtf)", R"(C::\wtf)"},
            NormalizeTestCase{R"(\\LOCALHOST\c$\home\foo)", R"(\\LOCALHOST\c$\home\foo)"}};

        for (const auto& [inputPath, expectedNormalizedPath] : testCases) {
            QCOMPARE(normalizePath(inputPath), expectedNormalizedPath);
        }
    }

    void checkToNativeSeparators() {
        const auto testCases = std::array{
            NativeSeparatorsTestCase{"/", "/"},
            NativeSeparatorsTestCase{"/home/foo", "/home/foo"},
            NativeSeparatorsTestCase{"C:/", R"(C:\)"},
            NativeSeparatorsTestCase{"C:/home/foo", R"(C:\home\foo)"},

            // These are not absolute Windows file paths and are left untouched
            NativeSeparatorsTestCase{"d:", "d:"},
            NativeSeparatorsTestCase{"d:foo", "d:foo"},
            NativeSeparatorsTestCase{R"(C::/wtf)", R"(C::/wtf)"},
            NativeSeparatorsTestCase{R"(//LOCALHOST/c$/home/foo)", R"(//LOCALHOST/c$/home/foo)"}};
        for (const auto& [inputPath, expectedNativeSeparatorsPath] : testCases) {
            QCOMPARE(toNativeSeparators(inputPath), expectedNativeSeparatorsPath);
        }
    }
};

QTEST_MAIN(PathUtilsTest)

#include "pathutils_test.moc"
