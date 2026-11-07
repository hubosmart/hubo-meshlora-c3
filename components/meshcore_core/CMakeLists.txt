set(CORE "${CMAKE_CURRENT_LIST_DIR}/../../upstream/meshcore")
set(BASE64 "${CMAKE_CURRENT_LIST_DIR}/../../upstream/base64/src")
idf_component_register(
    SRCS
        "${CORE}/src/Dispatcher.cpp"
        "${CORE}/src/Identity.cpp"
        "${CORE}/src/Mesh.cpp"
        "${CORE}/src/Packet.cpp"
        "${CORE}/src/Utils.cpp"
        "${CORE}/src/helpers/AdvertDataHelpers.cpp"
        "${CORE}/src/helpers/BaseChatMesh.cpp"
        "${CORE}/src/helpers/StaticPoolPacketManager.cpp"
        "${CORE}/src/helpers/TxtDataHelpers.cpp"
        "${CORE}/lib/ed25519/add_scalar.c"
        "${CORE}/lib/ed25519/fe.c"
        "${CORE}/lib/ed25519/ge.c"
        "${CORE}/lib/ed25519/key_exchange.c"
        "${CORE}/lib/ed25519/keypair.c"
        "${CORE}/lib/ed25519/sc.c"
        "${CORE}/lib/ed25519/sha512.c"
        "${CORE}/lib/ed25519/sign.c"
    INCLUDE_DIRS "${CORE}/src" "${CORE}/lib/ed25519" "${BASE64}"
    REQUIRES native_platform native_crypto
)
target_compile_definitions(${COMPONENT_LIB} PUBLIC
    MAX_CONTACTS=350 MAX_GROUP_CHANNELS=40 OFFLINE_QUEUE_SIZE=256)

# Keep the warnings of the locked upstream; only lower the level of warnings confirmed as part of the porting baseline.
target_compile_options(${COMPONENT_LIB} PUBLIC
    "$<$<COMPILE_LANGUAGE:CXX>:-Wno-error=reorder;-Wno-error=class-memaccess>")
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=format)
