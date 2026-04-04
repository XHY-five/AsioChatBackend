include_guard(GLOBAL)

find_path(ASIOCHAT_BOOST_INCLUDE_DIR
  NAMES boost/asio.hpp
  HINTS
    "${BOOST_ROOT_LOCAL}"
    "${BOOST_ROOT_LOCAL}/include"
)

find_path(ASIOCHAT_MYSQL_CONNECTOR_INCLUDE_DIR
  NAMES mysql/jdbc.h
  HINTS
    "${MYSQL_CONNECTOR_ROOT}"
    "${MYSQL_CONNECTOR_ROOT}/include"
  PATH_SUFFIXES include include/jdbc
)

find_library(ASIOCHAT_MYSQLCPPCONN_LIBRARY
  NAMES mysqlcppconn mysqlcppconn-static
  HINTS "${MYSQL_CONNECTOR_ROOT}"
  PATH_SUFFIXES lib lib64 lib/x86_64-linux-gnu
)

find_library(ASIOCHAT_MYSQLCPPCONN8_LIBRARY
  NAMES mysqlcppconn8 mysqlcppconn8-static
  HINTS "${MYSQL_CONNECTOR_ROOT}"
  PATH_SUFFIXES lib lib64 lib/x86_64-linux-gnu
)

find_path(ASIOCHAT_HIREDIS_INCLUDE_DIR
  NAMES hiredis.h hiredis/hiredis.h
  HINTS "${HIREDIS_ROOT}"
  PATH_SUFFIXES include/hiredis include deps/hiredis
)

find_library(ASIOCHAT_HIREDIS_LIBRARY
  NAMES hiredis
  HINTS "${HIREDIS_ROOT}"
  PATH_SUFFIXES lib lib64 lib/x86_64-linux-gnu
)

if(NOT ASIOCHAT_BOOST_INCLUDE_DIR)
  message(FATAL_ERROR "Unable to locate Boost headers. Set BOOST_ROOT_LOCAL or ASIOCHAT_DEPS_ROOT.")
endif()

if(NOT ASIOCHAT_MYSQL_CONNECTOR_INCLUDE_DIR)
  message(FATAL_ERROR "Unable to locate MySQL Connector/C++ headers. Set MYSQL_CONNECTOR_ROOT.")
endif()

if(NOT ASIOCHAT_MYSQLCPPCONN_LIBRARY)
  message(FATAL_ERROR "Unable to locate mysqlcppconn library. Set MYSQL_CONNECTOR_ROOT.")
endif()

if(NOT ASIOCHAT_MYSQLCPPCONN8_LIBRARY)
  message(FATAL_ERROR "Unable to locate mysqlcppconn8 library. Set MYSQL_CONNECTOR_ROOT.")
endif()

if(NOT ASIOCHAT_HIREDIS_INCLUDE_DIR OR NOT ASIOCHAT_HIREDIS_LIBRARY)
  message(FATAL_ERROR "Unable to locate hiredis headers or library. Set HIREDIS_ROOT.")
endif()

add_library(AsioChat::boost_headers INTERFACE IMPORTED GLOBAL)
set_target_properties(AsioChat::boost_headers PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${ASIOCHAT_BOOST_INCLUDE_DIR}"
  INTERFACE_COMPILE_DEFINITIONS "BOOST_ERROR_CODE_HEADER_ONLY;BOOST_SYSTEM_NO_LIB"
)

add_library(AsioChat::server_deps INTERFACE IMPORTED GLOBAL)
set_target_properties(AsioChat::server_deps PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${ASIOCHAT_MYSQL_CONNECTOR_INCLUDE_DIR};${ASIOCHAT_HIREDIS_INCLUDE_DIR}"
  INTERFACE_LINK_LIBRARIES "${ASIOCHAT_MYSQLCPPCONN_LIBRARY};${ASIOCHAT_MYSQLCPPCONN8_LIBRARY};${ASIOCHAT_HIREDIS_LIBRARY}"
)
