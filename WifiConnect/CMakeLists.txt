if (CONFIG_WIFICONNECT)
    set(srcs "src/WifiConnect.c"
             )

    zephyr_include_directories(include)
    zephyr_library_sources(${srcs})
endif()
