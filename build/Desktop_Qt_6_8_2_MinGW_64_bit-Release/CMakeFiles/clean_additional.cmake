# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Release")
  file(REMOVE_RECURSE
  "CMakeFiles\\ScreenshotTool_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\ScreenshotTool_autogen.dir\\ParseCache.txt"
  "ScreenshotTool_autogen"
  )
endif()
