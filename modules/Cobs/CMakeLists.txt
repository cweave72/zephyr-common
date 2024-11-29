if (CONFIG_COBS)
    set(srcs "src/Cobs.c"
             "src/Cobs_frame.c"
             )

    zephyr_include_directories(include)
    zephyr_library_sources(${srcs})
endif()
