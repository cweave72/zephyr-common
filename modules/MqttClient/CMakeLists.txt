if (CONFIG_MQTTCLIENT)
    set(srcs "src/MqttClient.c")

    zephyr_include_directories(include)
    zephyr_library_sources(${srcs})
endif()

