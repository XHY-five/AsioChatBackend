cmake_minimum_required(VERSION 3.21)

project(AsioChat LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ASIOCHAT_BUILD_SERVER "Build the chat server executable" ON)
option(ASIOCHAT_BUILD_CLIENT "Build the chat client executable" ON)

if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
  add_compile_options(/utf-8)
endif()

find_package(Threads REQUIRED)

include(cmake/Dependencies.cmake)
include(cmake/ThirdPartyTargets.cmake)

set(ASIOCHAT_COMMON_SOURCES
  src/common/protocol.cpp
  src/common/boost_json_impl.cpp
)

set(ASIOCHAT_SERVER_SOURCES
  src/server/app_config.cpp
  src/server/business_executor.cpp
  src/server/chat_room.cpp
  src/server/chat_server.cpp
  src/server/chat_session.cpp
  src/server/mysql_connection_pool.cpp
  src/server/mysql_offline_message_dao.cpp
  src/server/mysql_user_dao.cpp
  src/server/mysql_user_store.cpp
  src/server/offline_message_store.cpp
  src/server/online_status_store.cpp
  src/server/redis_connection_pool.cpp
  src/server/room_ai_agent_service.cpp
  src/server/store_factory.cpp
  src/server/thread_pool.cpp
)

set(ASIOCHAT_CLIENT_SOURCES
  src/client/chat_client.cpp
)

add_library(asiochat_common STATIC ${ASIOCHAT_COMMON_SOURCES})
add_library(AsioChat::common ALIAS asiochat_common)

target_include_directories(asiochat_common
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(asiochat_common
  PUBLIC
    AsioChat::boost_headers
)

if(ASIOCHAT_BUILD_SERVER)
  add_library(asiochat_server_core STATIC ${ASIOCHAT_SERVER_SOURCES})
  add_library(AsioChat::server_core ALIAS asiochat_server_core)

  target_include_directories(asiochat_server_core
    PUBLIC
      "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )

  target_link_libraries(asiochat_server_core
    PUBLIC
      AsioChat::common
      Threads::Threads
    PRIVATE
      AsioChat::server_deps
  )

  target_compile_definitions(asiochat_server_core
    PUBLIC
      UNICODE
      _UNICODE
  )

  add_executable(asiochat_server src/server/main.cpp)
  target_link_libraries(asiochat_server PRIVATE AsioChat::server_core)

  set_target_properties(asiochat_server PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:asiochat_server>"
  )

  add_custom_command(TARGET asiochat_server POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/app.cfg"
            "$<TARGET_FILE_DIR:asiochat_server>/app.cfg"
    VERBATIM
  )

  if(WIN32)
    foreach(dll_file IN LISTS ASIOCHAT_RUNTIME_DLLS)
      get_filename_component(dll_name "${dll_file}" NAME)
      add_custom_command(TARGET asiochat_server POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${dll_file}"
                "$<TARGET_FILE_DIR:asiochat_server>/${dll_name}"
        VERBATIM
      )
    endforeach()
  endif()
endif()

if(ASIOCHAT_BUILD_CLIENT)
  add_library(asiochat_client_core STATIC ${ASIOCHAT_CLIENT_SOURCES})
  add_library(AsioChat::client_core ALIAS asiochat_client_core)

  target_include_directories(asiochat_client_core
    PUBLIC
      "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )

  target_link_libraries(asiochat_client_core
    PUBLIC
      AsioChat::common
      Threads::Threads
  )

  if(WIN32)
    target_link_libraries(asiochat_client_core PUBLIC ws2_32)
  endif()

  add_executable(asiochat_client src/client/main.cpp)
  target_link_libraries(asiochat_client PRIVATE AsioChat::client_core)
endif()
