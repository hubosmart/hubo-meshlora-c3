set(MESHCORE "${CMAKE_CURRENT_LIST_DIR}/../../upstream/meshcore/src")
idf_component_register(
    SRCS "hubo_platform.cpp"
    INCLUDE_DIRS "include" "${MESHCORE}"
    REQUIRES esp_timer esp_system bootloader_support
)
