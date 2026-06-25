cmake_minimum_required(VERSION 3.15...4.0)
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

# Windows static zlib override
if(WIN32)
    # Use $ENV{MSYSTEM_PREFIX} to dynamically find the correct MSYS2 path
    set(MSYS_ROOT "$ENV{MSYSTEM_PREFIX}")
    set(ZLIB_LIBRARIES "${MSYS_ROOT}/lib/libz.a")
    set(ZLIB_INCLUDE_DIRS "${MSYS_ROOT}/include")
    
    # Verify the path exists; if not, fall back to find_package
    if(NOT EXISTS "${ZLIB_LIBRARIES}")
        message(STATUS "Static libz.a not found at ${ZLIB_LIBRARIES}, using system find_package")
        find_package(ZLIB REQUIRED)
    else()
        set(ZLIB_FOUND TRUE)
        message(STATUS "Using static zlib: ${ZLIB_LIBRARIES}")
    endif()
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

# suppress sqlite macro redef warnings
target_compile_options(qserv PRIVATE -Wno-builtin-macro-redefined)

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

set(PLATFORM_LIBS)
if(WIN32)
    # Force complete static linking across GCC dependencies
    target_link_options(qserv PRIVATE -static -static-libgcc -static-libstdc++)
    
    # FORCE ZLIB TO STOP GENERATING __imp_ DLL DECORATIONS
    target_compile_definitions(qserv PRIVATE SQLITE_STATIC ZLIB_INTERNAL) 
    
    set(SQLITE3_LIBRARY "-l:libsqlite3.a")
    set(ZLIB_LINK_TARGET "${ZLIB_LIBRARIES}")
    set(PLATFORM_LIBS ws2_32 winmm)
else()
    set(SQLITE3_LIBRARY "sqlite3")
    set(ZLIB_LINK_TARGET ZLIB::ZLIB)
endif()

target_link_libraries(qserv PRIVATE
    enet
    GeoIP
    ${SQLITE3_LIBRARY}
    ${ZLIB_LINK_TARGET}
    Threads::Threads
    ${PLATFORM_LIBS}
)

install(TARGETS qserv DESTINATION bin)
