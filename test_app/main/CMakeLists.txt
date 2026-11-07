idf_component_register(
    SRCS "test_main.cpp" "test_crypto.cpp" "test_stream.cpp" "test_store.cpp" "test_radio.cpp" "test_companion.cpp" "test_framing.cpp"
    INCLUDE_DIRS "."
    PRIV_REQUIRES unity meshcore_core native_platform native_store hubo_radio companion companion_codec companion_transport spiffs
    WHOLE_ARCHIVE
)
