set(CODEC "${CMAKE_CURRENT_LIST_DIR}/../../../upstream/meshcore_c")
idf_component_register(SRCS "${CODEC}/meshcore_companion.c"
    INCLUDE_DIRS "${CODEC}")
