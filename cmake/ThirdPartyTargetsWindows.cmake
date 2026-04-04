include_guard(GLOBAL)

set(ASIOCHAT_ALLOW_DEBUG_ONLY_DEPS ON CACHE BOOL "Allow Debug libraries to satisfy non-Debug configurations when Release libs are unavailable")

function(asiochat_import_library target_name debug_path)
  set(options)
  set(oneValueArgs RELEASE_PATH)
  cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "" ${ARGN})

  if(NOT EXISTS "${debug_path}")
    message(FATAL_ERROR "Missing required library for ${target_name}: ${debug_path}")
  endif()

  add_library(${target_name} UNKNOWN IMPORTED GLOBAL)
  set_target_properties(${target_name} PROPERTIES
    IMPORTED_CONFIGURATIONS "DEBUG;RELEASE;RELWITHDEBINFO;MINSIZEREL"
    IMPORTED_LOCATION_DEBUG "${debug_path}"
  )

  if(ARG_RELEASE_PATH AND EXISTS "${ARG_RELEASE_PATH}")
    set_target_properties(${target_name} PROPERTIES
      IMPORTED_LOCATION_RELEASE "${ARG_RELEASE_PATH}"
      IMPORTED_LOCATION_RELWITHDEBINFO "${ARG_RELEASE_PATH}"
      IMPORTED_LOCATION_MINSIZEREL "${ARG_RELEASE_PATH}"
    )
  elseif(ASIOCHAT_ALLOW_DEBUG_ONLY_DEPS)
    set_target_properties(${target_name} PROPERTIES
      IMPORTED_LOCATION_RELEASE "${debug_path}"
      IMPORTED_LOCATION_RELWITHDEBINFO "${debug_path}"
      IMPORTED_LOCATION_MINSIZEREL "${debug_path}"
      MAP_IMPORTED_CONFIG_RELEASE Debug
      MAP_IMPORTED_CONFIG_RELWITHDEBINFO Debug
      MAP_IMPORTED_CONFIG_MINSIZEREL Debug
    )
  else()
    message(FATAL_ERROR
      "Release library for ${target_name} is missing.\n"
      "Expected: ${ARG_RELEASE_PATH}\n"
      "Either provide the Release library or enable ASIOCHAT_ALLOW_DEBUG_ONLY_DEPS."
    )
  endif()
endfunction()

asiochat_require_path("${BOOST_ROOT_LOCAL}" "Place Boost under the dependency root.")
asiochat_require_path("${MYSQL_CONNECTOR_ROOT}/include" "Place mysql-connector under the dependency root.")
asiochat_require_path("${MYSQL_CONNECTOR_ROOT}/lib64/vs14/debug/mysqlcppconn.lib" "Ensure mysqlcppconn debug library exists.")
asiochat_require_path("${HIREDIS_ROOT}" "Place hiredis under the dependency root.")

add_library(AsioChat::boost_headers INTERFACE IMPORTED GLOBAL)
set_target_properties(AsioChat::boost_headers PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${BOOST_ROOT_LOCAL}"
  INTERFACE_COMPILE_DEFINITIONS "BOOST_ERROR_CODE_HEADER_ONLY;BOOST_SYSTEM_NO_LIB;BOOST_ALL_NO_LIB;_WIN32_WINNT=0x0A00"
)

set(_asiochat_hiredis_include "")
if(EXISTS "${HIREDIS_ROOT}/deps/hiredis/hiredis.h")
  set(_asiochat_hiredis_include "${HIREDIS_ROOT}/deps/hiredis")
elseif(EXISTS "${HIREDIS_ROOT}/include/hiredis/hiredis.h")
  set(_asiochat_hiredis_include "${HIREDIS_ROOT}/include")
elseif(EXISTS "${HIREDIS_ROOT}/include/hiredis.h")
  set(_asiochat_hiredis_include "${HIREDIS_ROOT}/include")
endif()

if(NOT _asiochat_hiredis_include)
  message(FATAL_ERROR "Unable to locate hiredis headers under ${HIREDIS_ROOT}")
endif()

set(_asiochat_hiredis_lib "")
if(EXISTS "${HIREDIS_ROOT}/lib/hiredis.lib")
  set(_asiochat_hiredis_lib "${HIREDIS_ROOT}/lib/hiredis.lib")
elseif(EXISTS "${HIREDIS_ROOT}/build/Debug/hiredis.lib")
  set(_asiochat_hiredis_lib "${HIREDIS_ROOT}/build/Debug/hiredis.lib")
endif()

if(NOT _asiochat_hiredis_lib)
  message(FATAL_ERROR "Unable to locate hiredis.lib under ${HIREDIS_ROOT}")
endif()

asiochat_import_library(AsioChat::mysqlcppconn
  "${MYSQL_CONNECTOR_ROOT}/lib64/vs14/debug/mysqlcppconn.lib"
  RELEASE_PATH "${MYSQL_CONNECTOR_ROOT}/lib64/vs14/mysqlcppconn.lib"
)

asiochat_import_library(AsioChat::mysqlcppconn8
  "${MYSQL_CONNECTOR_ROOT}/lib64/vs14/debug/mysqlcppconn8.lib"
  RELEASE_PATH "${MYSQL_CONNECTOR_ROOT}/lib64/vs14/mysqlcppconn8.lib"
)

asiochat_import_library(AsioChat::hiredis
  "${_asiochat_hiredis_lib}"
  RELEASE_PATH "${_asiochat_hiredis_lib}"
)

set(_asiochat_server_link_targets
  AsioChat::mysqlcppconn
  AsioChat::mysqlcppconn8
  AsioChat::hiredis
  ws2_32
)

if(EXISTS "${HIREDIS_ROOT}/lib/Win32_Interop.lib")
  asiochat_import_library(AsioChat::win32_interop
    "${HIREDIS_ROOT}/lib/Win32_Interop.lib"
    RELEASE_PATH "${HIREDIS_ROOT}/lib/Win32_Interop.lib"
  )
  list(APPEND _asiochat_server_link_targets AsioChat::win32_interop)
endif()

add_library(AsioChat::server_deps INTERFACE IMPORTED GLOBAL)
set_target_properties(AsioChat::server_deps PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${MYSQL_CONNECTOR_ROOT}/include;${_asiochat_hiredis_include}"
  INTERFACE_LINK_LIBRARIES "${_asiochat_server_link_targets}"
)

set(ASIOCHAT_RUNTIME_DLLS "")
foreach(dll_name IN ITEMS mysqlcppconn-9-vs14.dll mysqlcppconn8-2-vs14.dll)
  if(EXISTS "${CMAKE_SOURCE_DIR}/bin/${dll_name}")
    list(APPEND ASIOCHAT_RUNTIME_DLLS "${CMAKE_SOURCE_DIR}/bin/${dll_name}")
  endif()
endforeach()
