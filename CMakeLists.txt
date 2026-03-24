cmake_minimum_required(VERSION 3.16)

project(AsioChatBackend VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ASIOCHAT_BUILD_CLIENT "Build the chat client" ON)
set(Boost_USE_STATIC_LIBS OFF)

if(DEFINED ENV{CONDA_PREFIX})
    list(PREPEND CMAKE_PREFIX_PATH
        "$ENV{CONDA_PREFIX}/Library"
        "$ENV{CONDA_PREFIX}/Library/lib/cmake/Boost-1.82.0"
    )
    set(ASIOCHAT_BOOST_RUNTIME_DIR "$ENV{CONDA_PREFIX}/Library/bin")
endif()

find_package(Boost 1.70 CONFIG QUIET COMPONENTS headers system json)

if(NOT Boost_FOUND)
    if(POLICY CMP0167)
        cmake_policy(SET CMP0167 OLD)
    endif()
    find_package(Boost 1.70 REQUIRED COMPONENTS system json)
endif()

add_library(asiochat_common
    src/common/protocol.cpp
    src/server/app_config.cpp
    src/server/business_executor.cpp
    src/server/offline_message_store.cpp
    src/server/online_status_store.cpp
    src/server/store_factory.cpp
    src/server/thread_pool.cpp
)

target_include_directories(asiochat_common
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${Boost_INCLUDE_DIRS}
)

if(TARGET Boost::headers)
    target_link_libraries(asiochat_common PUBLIC Boost::headers Boost::system Boost::json)
elseif(TARGET Boost::boost)
    target_link_libraries(asiochat_common PUBLIC Boost::boost Boost::system Boost::json)
else()
    target_link_libraries(asiochat_common PUBLIC Boost::system Boost::json)
endif()

if(WIN32)
    target_compile_definitions(asiochat_common PUBLIC _WIN32_WINNT=0x0A00)
endif()

target_link_libraries(asiochat_common
    PUBLIC
        ws2_32
)

function(asiochat_copy_windows_runtime target_name)
    if(WIN32)
        if(DEFINED ASIOCHAT_BOOST_RUNTIME_DIR AND EXISTS "${ASIOCHAT_BOOST_RUNTIME_DIR}/boost_json.dll")
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${ASIOCHAT_BOOST_RUNTIME_DIR}/boost_json.dll"
                    "$<TARGET_FILE_DIR:${target_name}>"
            )
        endif()
        if(DEFINED ASIOCHAT_BOOST_RUNTIME_DIR AND EXISTS "${ASIOCHAT_BOOST_RUNTIME_DIR}/boost_system.dll")
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${ASIOCHAT_BOOST_RUNTIME_DIR}/boost_system.dll"
                    "$<TARGET_FILE_DIR:${target_name}>"
            )
        endif()
    endif()
endfunction()

add_executable(asiochat_server
    src/server/main.cpp
    src/server/chat_server.cpp
    src/server/chat_room.cpp
    src/server/chat_session.cpp
)

target_link_libraries(asiochat_server
    PRIVATE
        asiochat_common
)

asiochat_copy_windows_runtime(asiochat_server)

if(ASIOCHAT_BUILD_CLIENT)
    add_executable(asiochat_client
        src/client/main.cpp
        src/client/chat_client.cpp
    )

    target_link_libraries(asiochat_client
        PRIVATE
            asiochat_common
    )

    asiochat_copy_windows_runtime(asiochat_client)
endif()
