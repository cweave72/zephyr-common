if (CONFIG_TCPSOCKET)
    zephyr_include_directories(include)
    zephyr_library_sources("src/TcpSocket.c")
endif()
