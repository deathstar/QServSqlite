cmake_minimum_required(VERSION 4.0)
project(qserv LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    if(NOT CMAKE_OSX_ARCHITECTURES)
        execute_process(
            COMMAND uname -m
            OUTPUT_VARIABLE HOST_ARCH
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(HOST_ARCH STREQUAL "x86_64")
            set(CMAKE_OSX_ARCHITECTURES x86_64 CACHE STRING "" FORCE)
        elseif(HOST_ARCH STREQUAL "arm64")
            set(CMAKE_OSX_ARCHITECTURES arm64 CACHE STRING "" FORCE)
        else()
            message(WARNING "Unknown macOS architecture '${HOST_ARCH}', defaulting to x86_64")
            set(CMAKE_OSX_ARCHITECTURES x86_64 CACHE STRING "" FORCE)
        endif()
    endif()
endif()

add_subdirectory(enet)
add_subdirectory(GeoIP)

# --- CORRECTED WINDOWS STATIC ZLIB OVERRIDE ---
if(WIN32)
    # Bypass find_package and hardcode paths to MinGW's internal static libz.a
    set(ZLIB_FOUND TRUE)
    set(ZLIB_LIBRARIES "C:/msys64/mingw64/lib/libz.a")
    set(ZLIB_INCLUDE_DIRS "C:/msys64/mingw64/include")
else()
    find_package(ZLIB REQUIRED)
endif()

find_package(Threads REQUIRED)

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

add_executable(qserv ${qserv_srcs})

target_include_directories(qserv PRIVATE
    ${CMAKE_SOURCE_DIR}/src/engine
    ${CMAKE_SOURCE_DIR}/fpsgame
    ${CMAKE_SOURCE_DIR}/GeoIP/libGeoIP
    ${CMAKE_SOURCE_DIR}/shared
    ${CMAKE_SOURCE_DIR}/tools
    ${CMAKE_SOURCE_DIR}/QServ
    ${CMAKE_SOURCE_DIR}/enet/include
    ${ZLIB_INCLUDE_DIRS}
)

target_compile_definitions(qserv PRIVATE STANDALONE)

include(CheckFunctionExists)
include(CheckSymbolExists)

check_function_exists(gethostbyaddr_r HAS_GETHOSTBYADDR_R)
check_function_exists(gethostbyname_r HAS_GETHOSTBYNAME_R)
check_function_exists(poll HAS_POLL)
check_function_exists(fcntl HAS_FCNTL)
check_function_exists(inet_pton HAS_INET_PTON)
check_function_exists(inet_ntop HAS_INET_NTOP)
check_symbol_exists("struct msghdr.msg_flags" "sys/socket.h" HAS_MSGHDR_FLAGS)
check_symbol_exists("socklen_t" "sys/types.h;sys/socket.h" HAS_SOCKLEN_T)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(NOT WIN32)
        target_link_options(qserv PRIVATE -rdynamic)
    endif()
endif()

# Platform-specific static linking structures
set(PLATFORM_LIBS)
if(WIN32)
    # Force complete static linking across GCC dependencies
    target_link_options(qserv PRIVATE -static -static-libgcc -static-libstdc++)
    
    # FORCE ZLIB TO STOP GENERATING __imp_ DLL DECORATIONS
    target_compile_definitions(qserv PRIVATE SQLITE_STATIC ZLIB_INTERNAL) # <-- CHANGED TO ZLIB_INTERNAL
    
    set(SQLITE3_LIBRARY "-l:libsqlite3.a")
    set(ZLIB_LINK_TARGET "${ZLIB_LIBRARIES}")
    set(PLATFORM_LIBS ws2_32 winmm)
else()
    set(SQLITE3_LIBRARY "sqlite3")
    set(ZLIB_LINK_TARGET ZLIB::ZLIB)
endif()

# Core combined linking sequence with correct library processing order
target_link_libraries(qserv PRIVATE
    enet
    GeoIP
    ${SQLITE3_LIBRARY}
    ${ZLIB_LINK_TARGET}
    Threads::Threads
    ${PLATFORM_LIBS}
)

install(TARGETS qserv DESTINATION bin)
