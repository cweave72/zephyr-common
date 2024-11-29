if (CONFIG_WS2812LED)
    set(srcs "src/WS2812Led.c"
             )

    zephyr_include_directories(include)
    zephyr_library_sources(${srcs})
endif()
