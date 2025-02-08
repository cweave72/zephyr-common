if (CONFIG_UDPSERVER)
    set(srcs "src/UdpServer.c")

    zephyr_include_directories(include)
    zephyr_library_sources(${srcs})
endif()

