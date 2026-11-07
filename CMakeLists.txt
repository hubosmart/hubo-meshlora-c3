cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS "upstream/radiolib")
# The version can be passed in via the HUBO_FW_VERSION environment variable; manual builds use the default
if(DEFINED ENV{HUBO_FW_VERSION})
  set(PROJECT_VER "$ENV{HUBO_FW_VERSION}")
else()
  set(PROJECT_VER "hubo-idf-dev")
endif()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(hubo_c3_companion)

# Flash storage.bin explicitly on first install; a normal flash keeps identity and config.
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/empty_storage")
spiffs_create_partition_image(storage "${CMAKE_BINARY_DIR}/empty_storage")
