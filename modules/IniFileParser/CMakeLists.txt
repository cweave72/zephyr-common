if (CONFIG_INIFILEPARSER)

    zephyr_include_directories(include)
    zephyr_library_sources("src/IniFileParser.c")

endif()
