include_guard(GLOBAL)

if(WIN32)
  include("${CMAKE_CURRENT_LIST_DIR}/ThirdPartyTargetsWindows.cmake")
elseif(UNIX AND NOT APPLE)
  include("${CMAKE_CURRENT_LIST_DIR}/ThirdPartyTargetsLinux.cmake")
else()
  message(FATAL_ERROR "Unsupported platform for GateServer: ${CMAKE_SYSTEM_NAME}")
endif()
