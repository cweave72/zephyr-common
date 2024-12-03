if (CONFIG_TCPRPCSERVER)
    zephyr_include_directories(include)
    zephyr_library_sources("src/TcpRpcServer.c")
endif()
