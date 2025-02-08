if (CONFIG_UDPSOCKET)
    set(srcs "src/UdpSocket.c")

    zephyr_include_directories(include)
    zephyr_library_sources(${srcs})
endif()

