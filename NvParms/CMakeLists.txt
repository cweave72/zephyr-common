if (CONFIG_NVPARMS)
    zephyr_include_directories(include)
    zephyr_library_sources("src/NvParms.c")
endif()
