if (CONFIG_SWFIFO)
    zephyr_include_directories(include)
    zephyr_library_sources("src/SwFifo.c")
endif()
