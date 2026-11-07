idf_component_register(SRCS "app_main.cpp" "hw_probe.cpp" INCLUDE_DIRS "."
    PRIV_REQUIRES companion companion_transport native_store native_platform driver hubo_radio)
