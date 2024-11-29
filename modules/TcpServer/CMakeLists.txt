if (CONFIG_TCPSERVER)
    zephyr_include_directories(include)
    zephyr_library_sources("src/TcpServer.c")
endif()
