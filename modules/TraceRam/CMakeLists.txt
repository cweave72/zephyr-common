if (CONFIG_TRACERAM)
    set(srcs "src/TraceRam.c")

    zephyr_include_directories(include)
    zephyr_library_sources(${srcs})
endif()

