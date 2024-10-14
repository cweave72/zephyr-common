if (CONFIG_TCPECHO)
    zephyr_include_directories(include)
    zephyr_library_sources("src/TcpEcho.c")
endif()
