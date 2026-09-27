if (CONFIG_FSAPI)

    zephyr_include_directories(include)
    zephyr_library_sources("src/FsApi.c")
    zephyr_library_sources_ifdef(CONFIG_FSAPIRPC "src/FsApiRpc.c")

endif()
