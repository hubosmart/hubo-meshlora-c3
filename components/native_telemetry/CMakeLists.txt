set(CAYENNE "${CMAKE_CURRENT_LIST_DIR}/../../upstream/cayenne/src")
file(GLOB TELEMETRY_SOURCES "${CAYENNE}/*.cpp")
idf_component_register(SRCS ${TELEMETRY_SOURCES}
    INCLUDE_DIRS "${CAYENNE}" "../../upstream/arduinojson/src")
