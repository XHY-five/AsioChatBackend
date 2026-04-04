include_guard(GLOBAL)

if(WIN32)
  include("${CMAKE_CURRENT_LIST_DIR}/ThirdPartyTargetsWindows.cmake")
else()
  include("${CMAKE_CURRENT_LIST_DIR}/ThirdPartyTargetsLinux.cmake")
endif()
