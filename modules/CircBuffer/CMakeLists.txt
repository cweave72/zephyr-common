if (CONFIG_CIRCBUFFER)
    set(srcs "src/CircBuffer.c")

    zephyr_include_directories(include)
    zephyr_library_sources(${srcs})
endif()
