if (CONFIG_RTOSUTILS)

    set(srcs "src/RtosUtils.c"
             "src/RtosUtilsRpc.c"
        )
    zephyr_include_directories(include)
    zephyr_library_sources("src/RtosUtils.c")
    zephyr_library_sources_ifdef(CONFIG_RTOSUTILS_RPC "src/RtosUtilsRpc.c")

endif()
