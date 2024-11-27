if (CONFIG_PBGENERIC)
    zephyr_include_directories(include)
    zephyr_library_sources("src/PbGeneric.c")
endif()
