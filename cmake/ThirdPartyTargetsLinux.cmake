include_guard(GLOBAL)

if(POLICY CMP0144)
  cmake_policy(SET CMP0144 NEW)
endif()

if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

find_package(Boost CONFIG REQUIRED COMPONENTS filesystem)

find_package(Protobuf CONFIG QUIET)
if(NOT Protobuf_FOUND)
  find_package(Protobuf REQUIRED)
endif()

find_package(gRPC CONFIG QUIET)
if(NOT gRPC_FOUND)
  find_package(gRPC REQUIRED)
endif()

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(HIREDIS REQUIRED IMPORTED_TARGET hiredis)
pkg_check_modules(JSONCPP REQUIRED IMPORTED_TARGET jsoncpp)

find_path(MYSQLCPPCONN_INCLUDE_DIR
  NAMES mysql/jdbc.h mysql_driver.h
  HINTS
    "${MYSQL_CONNECTOR_ROOT}/include"
    /usr/include/mysql-cppconn-1.1
    /usr/include
    /usr/local/include
)

find_library(MYSQLCPPCONN_LIBRARY
  NAMES mysqlcppconn mysqlcppconn8
  HINTS
    "${MYSQL_CONNECTOR_ROOT}/lib64"
    "${MYSQL_CONNECTOR_ROOT}/lib"
    /usr/lib/x86_64-linux-gnu
    /usr/lib
    /usr/lib64
    /usr/local/lib/x86_64-linux-gnu
    /usr/local/lib
    /usr/local/lib64
)

if(NOT MYSQLCPPCONN_INCLUDE_DIR OR NOT MYSQLCPPCONN_LIBRARY)
  message(FATAL_ERROR
    "MySQL Connector/C++ was not found for Linux.\n"
    "Set MYSQL_CONNECTOR_ROOT or install the development package that provides mysql/jdbc.h or mysql_driver.h and libmysqlcppconn."
  )
endif()

add_library(GateServer::mysqlcppconn UNKNOWN IMPORTED GLOBAL)
set_target_properties(GateServer::mysqlcppconn PROPERTIES
  IMPORTED_LOCATION "${MYSQLCPPCONN_LIBRARY}"
  INTERFACE_INCLUDE_DIRECTORIES "${MYSQLCPPCONN_INCLUDE_DIR}"
)

add_library(GateServer::third_party INTERFACE IMPORTED GLOBAL)
set_target_properties(GateServer::third_party PROPERTIES
  INTERFACE_LINK_LIBRARIES
    "Boost::filesystem;protobuf::libprotobuf;gRPC::grpc++;gRPC::grpc++_reflection;GateServer::mysqlcppconn;PkgConfig::HIREDIS;PkgConfig::JSONCPP;Threads::Threads"
)

message(STATUS "Configured Linux third-party skeleton with system packages and MYSQL_CONNECTOR_ROOT=${MYSQL_CONNECTOR_ROOT}")
