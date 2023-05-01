# SPDX-FileCopyrightText: 2015-2023 Alexey Rochev
#
# SPDX-License-Identifier: CC0-1.0

if (WIN32)
    cmake_minimum_required(VERSION 3.21)
else()
    cmake_minimum_required(VERSION 3.16)
endif()
cmake_policy(VERSION ${CMAKE_MINIMUM_REQUIRED_VERSION}..3.26)

option(TREMOTESF_QT6 "Build with Qt 6" ON)
if ((DEFINED VCPKG_TARGET_TRIPLET) AND (NOT DEFINED VCPKG_MANIFEST_FEATURES))
    if (TREMOTESF_QT6)
        set(VCPKG_MANIFEST_FEATURES qt6)
    else()
        set(VCPKG_MANIFEST_FEATURES qt5)
    endif()
endif()

project(libtremotesf CXX)

include(CTest)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

include(cmake/CommonOptions.cmake)

find_package(Qt${TREMOTESF_QT_VERSION_MAJOR} ${TREMOTESF_MINIMUM_QT_VERSION} REQUIRED COMPONENTS Core Concurrent Network)
find_package(fmt 7.0 REQUIRED)

if (WIN32 OR ANDROID)
    set(registrable_domain_qt ON)
else()
    set(registrable_domain_qt OFF)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(libpsl REQUIRED IMPORTED_TARGET libpsl)
endif()

add_library(
    libtremotesf
    OBJECT
    addressutils.cpp
    addressutils.h
    demangle.cpp
    demangle.h
    fileutils.cpp
    fileutils.h
    formatters.cpp
    formatters.h
    itemlistupdater.h
    jsonutils.h
    literals.h
    log.cpp
    log.h
    pathutils.cpp
    pathutils.h
    peer.cpp
    peer.h
    rpc.cpp
    rpc.h
    requestrouter.cpp
    requestrouter.h
    serversettings.cpp
    serversettings.h
    serverstats.cpp
    serverstats.h
    stdutils.h
    target_os.h
    torrent.cpp
    torrent.h
    torrentfile.cpp
    torrentfile.h
    tracker.cpp
    tracker.h
)

target_link_libraries(libtremotesf PUBLIC Qt::Core Qt::Network fmt::fmt)
if (registrable_domain_qt)
    target_compile_definitions(libtremotesf PRIVATE LIBTREMOTESF_REGISTRABLE_DOMAIN_QT)
else()
    target_link_libraries(libtremotesf PRIVATE PkgConfig::libpsl)
endif()
target_include_directories(libtremotesf PRIVATE ${Qt${TREMOTESF_QT_VERSION_MAJOR}Concurrent_INCLUDE_DIRS})

qt_import_plugins(libtremotesf EXCLUDE_BY_TYPE bearer)

if (BUILD_TESTING AND NOT ANDROID)
    find_package(Qt${TREMOTESF_QT_VERSION_MAJOR} ${TREMOTESF_MINIMUM_QT_VERSION} REQUIRED COMPONENTS Test)

    add_executable(itemlistupdater_test itemlistupdater_test.cpp)
    add_test(NAME itemlistupdater_test COMMAND itemlistupdater_test)
    target_link_libraries(itemlistupdater_test libtremotesf Qt::Test)

    add_executable(log_test log_test.cpp)
    add_test(NAME log_test COMMAND log_test)
    target_link_libraries(log_test libtremotesf Qt::Test)

    add_executable(demangle_test demangle_test.cpp)
    add_test(NAME demangle_test COMMAND demangle_test)
    target_link_libraries(demangle_test libtremotesf Qt::Test)

    # httplib requires same minor version for compatibility so setting minimum version as 0.11 won't work
    # We can work both with 0.11 and 0.12 so perform separate find_package calls for them
    find_package(httplib 0.12 QUIET)
    if (NOT httplib_FOUND)
        message(STATUS "Did not found cpp-httplib 0.12, trying 0.11")
        find_package(httplib 0.11 QUIET)
    endif()
    if (NOT httplib_FOUND)
        message(STATUS "Did not found cpp-httplib as a CMake package, trying pkg-config")
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(httplib IMPORTED_TARGET cpp-httplib>=0.11 cpp-httplib<=0.12)
    endif()
    if (NOT httplib_FOUND)
        message(STATUS "Using bundled cpp-httplib")
        set(HTTPLIB_REQUIRE_OPENSSL ON)
        add_subdirectory(3rdparty/cpp-httplib EXCLUDE_FROM_ALL)
    else()
        if (httplib_VERSION)
            message(STATUS "Using system cpp-httplib ${httplib_VERSION}")
        else()
            message(STATUS "Using system cpp-httplib")
        endif()
    endif()

    add_executable(requestrouter_test requestrouter_test.cpp)
    target_compile_definitions(requestrouter_test PRIVATE TEST_DATA_PATH="${CMAKE_CURRENT_SOURCE_DIR}/test-data" CPPHTTPLIB_OPENSSL_SUPPORT)
    add_test(NAME requestrouter_test COMMAND requestrouter_test)
    target_link_libraries(requestrouter_test libtremotesf Qt::Test)
    if (TARGET PkgConfig::httplib)
        target_link_libraries(requestrouter_test libtremotesf PkgConfig::httplib)
    else()
        target_link_libraries(requestrouter_test libtremotesf httplib::httplib)
    endif()

    add_executable(pathutils_test pathutils_test.cpp)
    add_test(NAME pathutils_test COMMAND pathutils_test)
    target_link_libraries(pathutils_test libtremotesf Qt::Test)

    add_executable(tracker_test tracker_test.cpp)
    add_test(NAME tracker_test COMMAND tracker_test)
    target_link_libraries(tracker_test libtremotesf Qt::Test)
endif()

set_common_options_on_targets()
