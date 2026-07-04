if (CONFIG_PUBLISHER)
    set(srcs "src/Publisher.c")

    zephyr_include_directories(include)
    zephyr_library_sources(${srcs})
endif()
