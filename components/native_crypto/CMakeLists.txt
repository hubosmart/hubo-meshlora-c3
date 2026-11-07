# rweather/Crypto (MIT, see the source file headers in upstream/crypto). AES-128 and SHA-256/HMAC use its software implementation, same as upstream MeshCore;
# the host cross-check test is tools/tests/test_crypto_vectors.py.
set(CRYPTO "${CMAKE_CURRENT_LIST_DIR}/../../upstream/crypto/libraries/Crypto")
idf_component_register(
    SRCS
        "${CRYPTO}/AES128.cpp"
        "${CRYPTO}/AESCommon.cpp"
        "${CRYPTO}/BlockCipher.cpp"
        "${CRYPTO}/SHA256.cpp"
        "${CRYPTO}/Ed25519.cpp"
        "${CRYPTO}/Curve25519.cpp"
        "${CRYPTO}/BigNumberUtil.cpp"
        "${CRYPTO}/Crypto.cpp"
        "${CRYPTO}/Hash.cpp"
        "${CRYPTO}/SHA512.cpp"
    INCLUDE_DIRS "${CRYPTO}"
)
