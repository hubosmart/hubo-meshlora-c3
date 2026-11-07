idf_component_register(SRCS "stream_framing.cpp" "usb_transport.cpp"
    INCLUDE_DIRS "include"
    REQUIRES companion esp_driver_usb_serial_jtag bt)
if(CONFIG_HUBO_TRANSPORT_BLE)
    target_sources(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/ble_transport.cpp")
    target_sources(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/ble_nvs_namespace.cpp")
    target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=nvs_open" "-Wl,--undefined=__wrap_nvs_open")
    idf_component_get_property(bt_library bt COMPONENT_LIB)
    target_link_libraries(${COMPONENT_LIB} PUBLIC ${bt_library})
    if(CONFIG_HUBO_BLE_ATT_TRACE)
        target_sources(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/ble_att_trace.cpp")
        target_link_options(${COMPONENT_LIB} INTERFACE
            "-Wl,--wrap=ble_att_svr_rx_write"
            "-Wl,--wrap=ble_hci_trans_hs_acl_tx"
            "-Wl,--wrap=ble_store_write_peer_sec"
            "-Wl,--undefined=__wrap_ble_att_svr_rx_write"
            "-Wl,--undefined=__wrap_ble_hci_trans_hs_acl_tx"
            "-Wl,--undefined=__wrap_ble_store_write_peer_sec")
    endif()
endif()
