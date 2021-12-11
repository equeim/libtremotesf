cmake_minimum_required(VERSION 3.12)

project(libtremotesf CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

if (NOT LIBTREMOTESF_QT_VERSION_MAJOR)
    message(WARNING "LIBTREMOTESF_QT_VERSION_MAJOR is not defined, defaulting to 6")
    set(LIBTREMOTESF_QT_VERSION_MAJOR 6)
endif()

find_package(Qt${LIBTREMOTESF_QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Concurrent Network)

if (ANDROID)
    set(library_type STATIC)
else()
    set(library_type OBJECT)
endif()

add_library(
    libtremotesf
    ${library_type}
    itemlistupdater.h
    peer.cpp
    peer.h
    qtsupport.h
    rpc.cpp
    rpc.h
    serversettings.cpp
    serversettings.h
    serverstats.cpp
    serverstats.h
    stdutils.h
    torrent.cpp
    torrent.h
    torrent_qdebug.h
    torrentfile.cpp
    torrentfile.h
    tracker.cpp
    tracker.h
)

target_compile_definitions(
    libtremotesf PUBLIC
    QT_DEPRECATED_WARNINGS
    QT_DISABLE_DEPRECATED_BEFORE=0x050e00
)

if (NOT SAILFISHOS)
    target_compile_definitions(libtremotesf PUBLIC QT_MESSAGELOGCONTEXT)
endif()

if (MSVC)
    target_compile_options(libtremotesf PUBLIC /W4)
else()
    target_compile_options(
        libtremotesf PUBLIC
        -Wall
        -Wextra
        -Wpedantic
        -Wnon-virtual-dtor
        -Wcast-align
        -Woverloaded-virtual
        -Wconversion
        -Wsign-conversion
        -Wdouble-promotion
        -Wformat=2
        -Werror=format
    )
    if (NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(libtremotesf PUBLIC -Wlogical-op)
    endif()
endif()

set(
    CXX_STANDARD_PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
get_directory_property(parent_directory PARENT_DIRECTORY)
if (parent_directory)
    set(CXX_STANDARD_PROPERTIES ${CXX_STANDARD_PROPERTIES} PARENT_SCOPE)
endif()

set_target_properties(libtremotesf PROPERTIES ${CXX_STANDARD_PROPERTIES})

target_link_libraries(libtremotesf Qt${LIBTREMOTESF_QT_VERSION_MAJOR}::Core Qt${LIBTREMOTESF_QT_VERSION_MAJOR}::Network)
target_include_directories(libtremotesf PUBLIC ${Qt${LIBTREMOTESF_QT_VERSION_MAJOR}Concurrent_INCLUDE_DIRS})
