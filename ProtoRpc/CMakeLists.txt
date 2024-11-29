if (CONFIG_PROTORPC)
    zephyr_include_directories(include)
    zephyr_library_sources("src/ProtoRpc.c")
endif()
