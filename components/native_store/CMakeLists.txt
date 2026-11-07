set(CORE "${CMAKE_CURRENT_LIST_DIR}/../../upstream/meshcore/src")
idf_component_register(SRCS "native_fs.cpp" "hubo_store.cpp" "IdentityStore.cpp" "DataStore.cpp"
    "${CORE}/helpers/ConfigSerializer.cpp"
    "${CORE}/helpers/TransportKeyStore.cpp"
    "${CORE}/helpers/RegionMap.cpp"
    "${CORE}/helpers/ClientACL.cpp"
    INCLUDE_DIRS "include" "."
    REQUIRES native_platform meshcore_core spiffs nvs_flash)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=format)
