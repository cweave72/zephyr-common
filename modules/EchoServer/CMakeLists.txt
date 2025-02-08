if (CONFIG_ECHOSERVER)
    set(srcs "src/EchoServer.c")

    zephyr_include_directories(include)
    zephyr_library_sources(${srcs})
endif()
