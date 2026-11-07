idf_component_register(SRCS "NativeSerialInterface.cpp" "MyMesh.cpp" "target.cpp" "HuboGnss.cpp"
    INCLUDE_DIRS "include" "."
    REQUIRES meshcore_core native_store hubo_radio native_telemetry driver)
target_compile_definitions(${COMPONENT_LIB} PUBLIC
    LORA_FREQ=433.65f LORA_BW=62.5f LORA_SF=8 LORA_CR=5
    LORA_TX_POWER=10 MAX_LORA_TX_POWER=10 USE_SX1268=1
    FIRMWARE_VERSION="hubo-idf-dev")
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=format)
if(DEFINED CONFIG_HUBO_GPS_EN_N_GPIO AND CONFIG_HUBO_GPS_EN_N_GPIO GREATER_EQUAL 0)
    # GNSS variant: enable the GPS custom vars and telemetry branch in the upstream companion
    target_compile_definitions(${COMPONENT_LIB} PUBLIC ENV_INCLUDE_GPS=1)
endif()
