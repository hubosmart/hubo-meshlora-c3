cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS "../components" "../upstream/radiolib")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(hubo_c3_tests)
idf_component_get_property(radiolib_library radiolib COMPONENT_LIB)
target_compile_definitions(${radiolib_library} PRIVATE RADIOLIB_DEBUG_BASIC=1)

# The test volume image is flashed separately; a normal flash does not erase the persisted test data.
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/empty_test_store")
spiffs_create_partition_image(test_store "${CMAKE_BINARY_DIR}/empty_test_store")
