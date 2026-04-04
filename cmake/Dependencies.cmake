set(_asiochat_default_deps_root "")

if(DEFINED ENV{ASIOCHAT_DEPS_ROOT} AND NOT "$ENV{ASIOCHAT_DEPS_ROOT}" STREQUAL "")
  set(_asiochat_default_deps_root "$ENV{ASIOCHAT_DEPS_ROOT}")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/third_party")
  set(_asiochat_default_deps_root "${CMAKE_SOURCE_DIR}/third_party")
elseif(WIN32)
  set(_asiochat_default_deps_root "E:/cppsoft")
endif()

set(CPPSOFT_ROOT "${_asiochat_default_deps_root}" CACHE PATH "Root directory of third-party dependencies")
set(MYSQL_CONNECTOR_ROOT "${CPPSOFT_ROOT}/mysql-connector" CACHE PATH "MySQL Connector/C++ root")

set(_asiochat_default_hiredis_root "${CPPSOFT_ROOT}/reids")
if(NOT EXISTS "${_asiochat_default_hiredis_root}" AND EXISTS "${CPPSOFT_ROOT}/hiredis")
  set(_asiochat_default_hiredis_root "${CPPSOFT_ROOT}/hiredis")
endif()

set(HIREDIS_ROOT "${HIREDIS_ROOT}" CACHE PATH "hiredis root")
if(NOT HIREDIS_ROOT OR NOT EXISTS "${HIREDIS_ROOT}")
  set(HIREDIS_ROOT "${_asiochat_default_hiredis_root}" CACHE PATH "hiredis root" FORCE)
endif()

set(BOOST_ROOT_LOCAL "${CPPSOFT_ROOT}/boost_1_89_0" CACHE PATH "Boost root")

function(asiochat_require_path path_value hint)
  if(NOT EXISTS "${path_value}")
    message(FATAL_ERROR
      "Missing required dependency path: ${path_value}\n"
      "Hint: ${hint}\n"
      "You can set ASIOCHAT_DEPS_ROOT or override the specific *_ROOT cache entry."
    )
  endif()
endfunction()

if(CPPSOFT_ROOT)
  message(STATUS "Using CPPSOFT_ROOT=${CPPSOFT_ROOT}")
endif()
