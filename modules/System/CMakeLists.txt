if (CONFIG_SYSTEM)

    zephyr_include_directories(include)
    zephyr_library_sources("src/System.c")
    zephyr_library_sources_ifdef(CONFIG_SYSTEMRPC "src/SystemRpc.c")

endif()
