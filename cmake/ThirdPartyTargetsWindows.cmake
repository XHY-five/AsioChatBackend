include_guard(GLOBAL)

set(GATESERVER_ALLOW_DEBUG_ONLY_DEPS ON CACHE BOOL "Allow Debug libraries to satisfy non-Debug configurations when Release libs are unavailable")

function(gateserver_import_library target_name debug_path)
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
  elseif(GATESERVER_ALLOW_DEBUG_ONLY_DEPS)
    set_target_properties(${target_name} PROPERTIES
      IMPORTED_LOCATION_RELEASE "${debug_path}"
      IMPORTED_LOCATION_RELWITHDEBINFO "${debug_path}"
      IMPORTED_LOCATION_MINSIZEREL "${debug_path}"
      MAP_IMPORTED_CONFIG_RELWITHDEBINFO Debug
      MAP_IMPORTED_CONFIG_MINSIZEREL Debug
      MAP_IMPORTED_CONFIG_RELEASE Debug
    )
  else()
    message(FATAL_ERROR
      "Release library for ${target_name} is missing.\n"
      "Expected: ${ARG_RELEASE_PATH}\n"
      "Either provide the Release library or enable GATESERVER_ALLOW_DEBUG_ONLY_DEPS."
    )
  endif()
endfunction()

add_library(GateServer::third_party INTERFACE IMPORTED GLOBAL)

set(_gateserver_include_dirs
  "${MYSQL_CONNECTOR_ROOT}/include"
  "${HIREDIS_ROOT}/deps/hiredis"
  "${JSON_ROOT}/include"
  "${BOOST_ROOT_LOCAL}"
  "${GRPC_ROOT}/third_party/re2"
  "${GRPC_ROOT}/third_party/address_sorting/include"
  "${GRPC_ROOT}/third_party/abseil-cpp"
  "${GRPC_ROOT}/third_party/protobuf/src"
  "${GRPC_ROOT}/include"
)

set_target_properties(GateServer::third_party PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${_gateserver_include_dirs}"
)

gateserver_import_library(GateServer::jsoncpp
  "${JSON_ROOT}/lib/json_vc71_libmtd.lib"
  RELEASE_PATH "${JSON_ROOT}/lib/json_vc71_libmt.lib"
)

gateserver_import_library(GateServer::protobuf
  "${GRPC_VISUALPRO_ROOT}/third_party/protobuf/Debug/libprotobufd.lib"
  RELEASE_PATH "${GRPC_VISUALPRO_ROOT}/third_party/protobuf/Release/libprotobuf.lib"
)

foreach(lib IN ITEMS gpr grpc grpc++ grpc++_reflection address_sorting upb)
  string(REPLACE "+" "x" _sanitized "${lib}")
  gateserver_import_library("GateServer::${_sanitized}"
    "${GRPC_VISUALPRO_ROOT}/Debug/${lib}.lib"
    RELEASE_PATH "${GRPC_VISUALPRO_ROOT}/Release/${lib}.lib"
  )
  list(APPEND _gateserver_link_targets "GateServer::${_sanitized}")
endforeach()

gateserver_import_library(GateServer::cares
  "${GRPC_VISUALPRO_ROOT}/third_party/cares/cares/lib/Debug/cares.lib"
  RELEASE_PATH "${GRPC_VISUALPRO_ROOT}/third_party/cares/cares/lib/Release/cares.lib"
)

gateserver_import_library(GateServer::zlib
  "${GRPC_VISUALPRO_ROOT}/third_party/zlib/Debug/zlibstaticd.lib"
  RELEASE_PATH "${GRPC_VISUALPRO_ROOT}/third_party/zlib/Release/zlibstatic.lib"
)

gateserver_import_library(GateServer::ssl
  "${GRPC_VISUALPRO_ROOT}/third_party/boringssl-with-bazel/Debug/ssl.lib"
  RELEASE_PATH "${GRPC_VISUALPRO_ROOT}/third_party/boringssl-with-bazel/Release/ssl.lib"
)

gateserver_import_library(GateServer::crypto
  "${GRPC_VISUALPRO_ROOT}/third_party/boringssl-with-bazel/Debug/crypto.lib"
  RELEASE_PATH "${GRPC_VISUALPRO_ROOT}/third_party/boringssl-with-bazel/Release/crypto.lib"
)

gateserver_import_library(GateServer::re2
  "${GRPC_VISUALPRO_ROOT}/third_party/re2/Debug/re2.lib"
  RELEASE_PATH "${GRPC_VISUALPRO_ROOT}/third_party/re2/Release/re2.lib"
)

gateserver_import_library(GateServer::hiredis
  "${HIREDIS_ROOT}/lib/hiredis.lib"
  RELEASE_PATH "${HIREDIS_ROOT}/lib/hiredis.lib"
)

gateserver_import_library(GateServer::win32_interop
  "${HIREDIS_ROOT}/lib/Win32_Interop.lib"
  RELEASE_PATH "${HIREDIS_ROOT}/lib/Win32_Interop.lib"
)

gateserver_import_library(GateServer::mysqlcppconn
  "${MYSQL_CONNECTOR_ROOT}/lib64/vs14/debug/mysqlcppconn.lib"
  RELEASE_PATH "${MYSQL_CONNECTOR_ROOT}/lib64/vs14/mysqlcppconn.lib"
)

gateserver_import_library(GateServer::mysqlcppconn8
  "${MYSQL_CONNECTOR_ROOT}/lib64/vs14/debug/mysqlcppconn8.lib"
  RELEASE_PATH "${MYSQL_CONNECTOR_ROOT}/lib64/vs14/mysqlcppconn8.lib"
)

gateserver_import_library(GateServer::boost_filesystem
  "${BOOST_ROOT_LOCAL}/stage/lib/libboost_filesystem-vc143-mt-gd-x64-1_89.lib"
  RELEASE_PATH "${BOOST_ROOT_LOCAL}/stage/lib/libboost_filesystem-vc143-mt-x64-1_89.lib"
)

set(_absl_components
  absl_bad_any_cast_impl
  absl_bad_optional_access
  absl_bad_variant_access
  absl_base
  absl_city
  absl_civil_time
  absl_cord
  absl_debugging_internal
  absl_demangle_internal
  absl_examine_stack
  absl_exponential_biased
  absl_failure_signal_handler
  absl_flags
  absl_flags_config
  absl_flags_internal
  absl_flags_marshalling
  absl_flags_parse
  absl_flags_program_name
  absl_flags_usage
  absl_flags_usage_internal
  absl_graphcycles_internal
  absl_hash
  absl_hashtablez_sampler
  absl_int128
  absl_leak_check
  absl_leak_check_disable
  absl_log_severity
  absl_malloc_internal
  absl_periodic_sampler
  absl_random_distributions
  absl_random_internal_distribution_test_util
  absl_random_internal_pool_urbg
  absl_random_internal_randen
  absl_random_internal_randen_hwaes
  absl_random_internal_randen_hwaes_impl
  absl_random_internal_randen_slow
  absl_random_internal_seed_material
  absl_random_seed_gen_exception
  absl_random_seed_sequences
  absl_raw_hash_set
  absl_raw_logging_internal
  absl_scoped_set_env
  absl_spinlock_wait
  absl_stacktrace
  absl_status
  absl_strings
  absl_strings_internal
  absl_str_format_internal
  absl_symbolize
  absl_synchronization
  absl_throw_delegate
  absl_time
  absl_time_zone
  absl_statusor
)

foreach(lib IN LISTS _absl_components)
  if(lib MATCHES "^absl_(bad_any_cast_impl|bad_optional_access|bad_variant_access)$")
    set(_absl_dir "${GRPC_VISUALPRO_ROOT}/third_party/abseil-cpp/absl/types")
  elseif(lib MATCHES "^absl_(flags|flags_config|flags_internal|flags_marshalling|flags_parse|flags_program_name|flags_usage|flags_usage_internal)$")
    set(_absl_dir "${GRPC_VISUALPRO_ROOT}/third_party/abseil-cpp/absl/flags")
  elseif(lib MATCHES "^absl_(debugging_internal|demangle_internal|examine_stack|failure_signal_handler|leak_check|leak_check_disable|stacktrace|symbolize)$")
    set(_absl_dir "${GRPC_VISUALPRO_ROOT}/third_party/abseil-cpp/absl/debugging")
  elseif(lib MATCHES "^absl_(hash|city)$")
    set(_absl_dir "${GRPC_VISUALPRO_ROOT}/third_party/abseil-cpp/absl/hash")
  elseif(lib MATCHES "^absl_(raw_hash_set|hashtablez_sampler)$")
    set(_absl_dir "${GRPC_VISUALPRO_ROOT}/third_party/abseil-cpp/absl/container")
  elseif(lib MATCHES "^absl_(int128)$")
    set(_absl_dir "${GRPC_VISUALPRO_ROOT}/third_party/abseil-cpp/absl/numeric")
  elseif(lib MATCHES "^absl_(civil_time|time|time_zone)$")
    set(_absl_dir "${GRPC_VISUALPRO_ROOT}/third_party/abseil-cpp/absl/time")
  elseif(lib MATCHES "^absl_(cord|strings|strings_internal|str_format_internal)$")
    set(_absl_dir "${GRPC_VISUALPRO_ROOT}/third_party/abseil-cpp/absl/strings")
  elseif(lib MATCHES "^absl_(status|statusor)$")
    set(_absl_dir "${GRPC_VISUALPRO_ROOT}/third_party/abseil-cpp/absl/status")
  elseif(lib MATCHES "^absl_(graphcycles_internal|synchronization)$")
    set(_absl_dir "${GRPC_VISUALPRO_ROOT}/third_party/abseil-cpp/absl/synchronization")
  elseif(lib MATCHES "^absl_(random_distributions|random_internal_distribution_test_util|random_internal_pool_urbg|random_internal_randen|random_internal_randen_hwaes|random_internal_randen_hwaes_impl|random_internal_randen_slow|random_internal_seed_material|random_seed_gen_exception|random_seed_sequences)$")
    set(_absl_dir "${GRPC_VISUALPRO_ROOT}/third_party/abseil-cpp/absl/random")
  else()
    set(_absl_dir "${GRPC_VISUALPRO_ROOT}/third_party/abseil-cpp/absl/base")
  endif()

  gateserver_import_library("GateServer::${lib}" "${_absl_dir}/Debug/${lib}.lib" RELEASE_PATH "${_absl_dir}/Release/${lib}.lib")
  list(APPEND _gateserver_link_targets "GateServer::${lib}")
endforeach()

list(APPEND _gateserver_link_targets
  GateServer::jsoncpp
  GateServer::protobuf
  GateServer::cares
  GateServer::zlib
  GateServer::ssl
  GateServer::crypto
  GateServer::re2
  GateServer::hiredis
  GateServer::win32_interop
  GateServer::mysqlcppconn
  GateServer::mysqlcppconn8
  GateServer::boost_filesystem
  ws2_32
)

set_target_properties(GateServer::third_party PROPERTIES
  INTERFACE_LINK_LIBRARIES "${_gateserver_link_targets}"
)
