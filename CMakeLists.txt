cmake_minimum_required(VERSION 3.4)  # Updated for better macOS support

set(CMAKE_CXX_STANDARD 11)

project(qserv)

# Detect architecture on macOS and set CMAKE_OSX_ARCHITECTURES accordingly
if(APPLE)
    execute_process(
        COMMAND uname -m
        OUTPUT_VARIABLE ARCH_OUTPUT
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    message(STATUS "Detected architecture: ${ARCH_OUTPUT}")

    if(ARCH_OUTPUT STREQUAL "arm64")
        set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build architecture" FORCE)
    elseif(ARCH_OUTPUT STREQUAL "x86_64")
        set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "Build architecture" FORCE)
    else()
        message(WARNING "Unknown architecture detected: ${ARCH_OUTPUT}, defaulting to x86_64")
        set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "Build architecture" FORCE)
    endif()
endif()

subdirs(enet GeoIP)
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

# Try pkg-config to find SQLite3
find_package(PkgConfig REQUIRED)
pkg_check_modules(SQLITE3 REQUIRED sqlite3)

set(qserv_srcs
    engine/server.cpp
    engine/command.cpp
    fpsgame/server.cpp
    mod/QServ.cpp
    mod/QCom.cpp
    shared/tools.cpp
    shared/stream.cpp
    shared/crypto.cpp
    ircbot/ircbot.cpp
)

set(qserv_LIBS
    enet
    GeoIP
    ${SQLITE3_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

# Platform-specific linking (optional fallback)
if(APPLE)
    set(CMAKE_MACOSX_RPATH 1)  # For modern macOS
    find_library(SQLITE3_LIBRARY NAMES sqlite3)
    if(NOT SQLITE3_LIBRARY)
        message(FATAL_ERROR "sqlite3 not found! Try installing via Homebrew: brew install sqlite3")
    endif()
    set(qserv_LIBS ${qserv_LIBS} ${SQLITE3_LIBRARY})
elseif(UNIX)
    find_library(SQLITE3_LIBRARY NAMES sqlite3)
    if(SQLITE3_LIBRARY)
        set(qserv_LIBS ${qserv_LIBS} ${SQLITE3_LIBRARY})
    endif()
endif()

# Optional fallback include path for systems without pkg-config
include_directories(
    ${SQLITE3_INCLUDE_DIRS}
    /usr/include
    /usr/local/include
    /opt/homebrew/include
)

link_directories(
    ${SQLITE3_LIBRARY_DIRS}
    /usr/lib
    /usr/local/lib
    /opt/homebrew/lib
)

if(WIN32)
    set(qserv_LIBS ${qserv_LIBS} GeoIP)
endif()

include(CheckFunctionExists)
include(CheckSymbolExists)

if(CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "-rdynamic")
endif()

check_function_exists(gethostbyaddr_r HAS_GETHOSTBYADDR_R)
check_function_exists(gethostbyname_r HAS_GETHOSTBYNAME_R)
check_function_exists(poll HAS_POLL)
check_function_exists(fcntl HAS_FCNTL)
check_function_exists(inet_pton HAS_INET_PTON)
check_function_exists(inet_ntop HAS_INET_NTOP)

check_symbol_exists("struct msghdr.msg_flags" "sys/socket.h" HAS_MSGHDR_FLAGS)
check_symbol_exists("socklen_t" "sys/types.h;sys/socket.h" HAS_SOCKLEN_T)

add_definitions(-DSTANDALONE)

include_directories(
    ${CMAKE_SOURCE_DIR}/src/engine
    ${CMAKE_SOURCE_DIR}/fpsgame
    ${CMAKE_SOURCE_DIR}/GeoIP/libGeoIP
    ${CMAKE_SOURCE_DIR}/shared
    ${CMAKE_SOURCE_DIR}/tools
    ${CMAKE_SOURCE_DIR}/QServ
    ${CMAKE_SOURCE_DIR}/enet/include
    ${ZLIB_INCLUDE_PATH}
)

add_executable(qserv ${qserv_srcs})

target_link_libraries(qserv ${qserv_LIBS})

install(TARGETS qserv DESTINATION bin)

# macOS: suppress .a archive "no symbols" warning
if (APPLE)
    set(CMAKE_C_ARCHIVE_CREATE   "<CMAKE_AR> Scr <TARGET> <LINK_FLAGS> <OBJECTS>")
    set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> Scr <TARGET> <LINK_FLAGS> <OBJECTS>")
    set(CMAKE_C_ARCHIVE_FINISH   "<CMAKE_RANLIB> -no_warning_for_no_symbols -c <TARGET>")
    set(CMAKE_CXX_ARCHIVE_FINISH "<CMAKE_RANLIB> -no_warning_for_no_symbols -c <TARGET>")
endif()