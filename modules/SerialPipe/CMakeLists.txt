if (CONFIG_SERIALPIPE)
    set(srcs "src/SerialPipe.c")

    zephyr_include_directories(include)
    zephyr_library_sources(${srcs})
endif()
